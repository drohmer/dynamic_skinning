#include "gpu_skinning.hpp"
#include "dynamic_skinning_source/oscillation_filter.hpp"
#include <chrono>
#include <algorithm>

using namespace cgp;


// ============================================================================
// Raw OpenGL texture creation helpers
// ============================================================================

static void create_rgba32f_texture_2d(opengl_texture_image_structure& tex, int w, int h, const float* data = nullptr)
{
    tex.width = w;
    tex.height = h;
    tex.format = GL_RGBA32F;
    tex.texture_type = GL_TEXTURE_2D;
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}


// ============================================================================
// Initialization
// ============================================================================

void gpu_skinning_initialize(gpu_skinning_state& state,
                             animated_model_structure const& model,
                             std::string const& mesh_name,
                             dynamic_skinning_state const& ds_state,
                             delayed_lbs_state const& dlbs_state,
                             mesh_drawable& drawable)
{
    // Clean up previous state if re-initializing
    if (state.initialized) {
        if (state.skinning_matrices_tex.id)  glDeleteTextures(1, &state.skinning_matrices_tex.id);
        if (state.hier_weights_tex.id)       glDeleteTextures(1, &state.hier_weights_tex.id);
        if (state.history_tex.id)            glDeleteTextures(1, &state.history_tex.id);
        if (state.dlbs_joint_tex.id)         glDeleteTextures(1, &state.dlbs_joint_tex.id);
        state = gpu_skinning_state();
    }

    auto const& rigged_mesh = model.rigged_mesh.at(mesh_name);
    auto const& controller = rigged_mesh.controller_skinning;

    state.N_joints = model.skeleton.size();
    state.N_vertex = rigged_mesh.mesh_bind_pose.position.size();
    if (state.N_joints > gpu_skinning_state::MAX_JOINTS) {
        std::cerr << "GPU skinning: " << state.N_joints << " joints, the shader is limited to "
                  << gpu_skinning_state::MAX_JOINTS << " (MAX_JOINTS in gpu_skinning.hpp and skinning.vert.glsl)" << std::endl;
        return;
    }
    state.joint_velocity_data.assign(4 * state.N_joints, 0.0f);
    state.joint_center_data.assign(4 * state.N_joints, 0.0f);

    // ---- 1. Compile shader ----
    state.skinning_shader.load(project::path + "shaders/skinning/skinning.vert.glsl",
                                project::path + "shaders/skinning/skinning.frag.glsl");

    // ---- 2. Create dynamic textures (updated per frame) ----
    create_rgba32f_texture_2d(state.skinning_matrices_tex, 4, state.N_joints);
    create_rgba32f_texture_2d(state.history_tex, 4 * delayed_lbs_state::N_history, state.N_joints);

    // Static data of the delayed LBS per joint: texel (col, j) = column col of B_j^-1, texel (4, j) = (parent, d_j)
    {
        std::vector<float> data(5 * state.N_joints * 4, 0.0f);
        for (int j = 0; j < state.N_joints; ++j) {
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    data[(j * 5 + col) * 4 + row] = dlbs_state.inverse_bind[j](row, col);
            data[(j * 5 + 4) * 4 + 0] = static_cast<float>(model.skeleton.parent_index[j]);
            data[(j * 5 + 4) * 4 + 1] = dlbs_state.joint_distance[j];
        }
        create_rgba32f_texture_2d(state.dlbs_joint_tex, 5, state.N_joints, data.data());
    }

    // ---- 3. Create & fill static hier_weights_tex (tiled 2D) ----
    {
        int const M = state.MAX_HIER_PER_VERTEX;
        int const TW = state.HIER_TILE_W;
        int tex_w = M * TW;
        int tex_h = (state.N_vertex + TW - 1) / TW;
        state.hier_tex_rows = tex_h;

        std::vector<float> data(tex_w * tex_h * 4, 0.0f);

        for (int v = 0; v < state.N_vertex; ++v) {
            int tile_row = v / TW;
            int tile_col = v % TW;
            auto const& joints = ds_state.joints_per_vertex[v];
            auto const& weights = ds_state.hierarchical_weights[v];
            int count = std::min(static_cast<int>(joints.size()), M);
            for (int k = 0; k < count; ++k) {
                int px = tile_col * M + k;
                int py = tile_row;
                int offset = (py * tex_w + px) * 4;
                data[offset + 0] = static_cast<float>(joints[k]);
                data[offset + 1] = weights[k];
                data[offset + 2] = ds_state.weight_bend[v];
                data[offset + 3] = static_cast<float>(ds_state.center_joints_per_vertex[v][k]);
            }
        }
        create_rgba32f_texture_2d(state.hier_weights_tex, tex_w, tex_h, data.data());
    }

    // ---- 4. Prepare LBS VBO data ----
    {
        numarray<vec4> joint_indices_vbo(state.N_vertex);
        numarray<vec4> joint_weights_vbo(state.N_vertex);

        for (int v = 0; v < state.N_vertex; ++v) {
            auto const& deps = controller.vertex_to_joint_dependence[v];
            int N_dep = deps.size();

            struct entry { int skel_idx; float weight; };
            std::vector<entry> entries;
            entries.reserve(N_dep);
            for (int k = 0; k < N_dep; ++k) {
                int rig_idx = deps[k].joint_index;
                int skel_idx = controller.rig_index_to_skeleton_index[rig_idx];
                entries.push_back({skel_idx, deps[k].weight});
            }

            std::sort(entries.begin(), entries.end(),
                      [](entry const& a, entry const& b) { return a.weight > b.weight; });

            vec4 idx = {0, 0, 0, 0};
            vec4 wgt = {0, 0, 0, 0};
            int count = std::min(static_cast<int>(entries.size()), 4);
            float sum = 0.0f;
            for (int k = 0; k < count; ++k) {
                idx[k] = static_cast<float>(entries[k].skel_idx);
                wgt[k] = entries[k].weight;
                sum += wgt[k];
            }
            if (sum > 1e-6f) {
                for (int k = 0; k < count; ++k)
                    wgt[k] /= sum;
            }

            joint_indices_vbo[v] = idx;
            joint_weights_vbo[v] = wgt;
        }

        drawable.initialize_supplementary_data_on_gpu(joint_indices_vbo, 4);
        drawable.initialize_supplementary_data_on_gpu(joint_weights_vbo, 5);

        // Distance d_i along the skeleton (delayed LBS)
        numarray<vec2> delay_distance_vbo(state.N_vertex);
        for (int v = 0; v < state.N_vertex; ++v)
            delay_distance_vbo[v] = {dlbs_state.distance[v], 0.0f};
        drawable.initialize_supplementary_data_on_gpu(delay_distance_vbo, 6);
    }

    // ---- 5. Initialize angular velocity trackers (one per joint) ----
    state.joint_trackers.resize(state.N_joints);
    for (int j = 0; j < state.N_joints; ++j)
        state.joint_trackers[j] = angular_velocity_tracker();
    state.filtered_velocity.resize(state.N_joints);
    state.filtered_velocity.fill({0, 0, 0});

    state.initialized = true;
}


// ============================================================================
// Per-frame update
// ============================================================================

void gpu_skinning_update(gpu_skinning_state& state,
                         animated_model_structure const& model,
                         std::string const& mesh_name,
                         dynamic_skinning_parameters const& params,
                         delayed_lbs_state const& dlbs_state,
                         float time)
{
    if (!state.initialized) return;

    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();

    auto const& controller = model.rigged_mesh.at(mesh_name).controller_skinning;

    // ---- 1. Compute & upload skinning matrices T_j ----
    {
        std::vector<float> mat_data(4 * state.N_joints * 4, 0.0f);

        for (int j = 0; j < state.N_joints; ++j) {
            int base = j * 4 * 4;
            mat_data[base + 0] = 1.0f;
            mat_data[base + 5] = 1.0f;
            mat_data[base + 10] = 1.0f;
            mat_data[base + 15] = 1.0f;
        }

        int N_rig = controller.rig_index_to_skeleton_index.size();
        for (int rig = 0; rig < N_rig; ++rig) {
            int skel = controller.rig_index_to_skeleton_index[rig];
            mat4 T = model.skeleton.joint_matrix_global[skel] * controller.inverse_bind_matrices[rig];

            int base = skel * 4 * 4;
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    mat_data[base + col * 4 + row] = T(row, col);
        }

        glBindTexture(GL_TEXTURE_2D, state.skinning_matrices_tex.id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, state.N_joints,
                        GL_RGBA, GL_FLOAT, mat_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // ---- 2. Compute filtered angular velocity per joint on CPU ----
    compute_filtered_angular_velocities(
        state.joint_trackers, state.filtered_velocity,
        model.skeleton, params, time);

    // ---- 3. Per-joint data of the bend deformer (uniform arrays, sent in gpu_skinning_draw) ----
    for (int j = 0; j < state.N_joints; ++j) {
        vec3 const& omega = state.filtered_velocity[j];
        float const n = norm(omega);
        vec3 const axis = n > 0.0f ? omega / n : vec3{0, 0, 0};
        mat4 const& M = model.skeleton.joint_matrix_global[j];
        for (int c = 0; c < 3; ++c) {
            state.joint_velocity_data[4 * j + c] = axis[c];
            state.joint_center_data[4 * j + c] = M(c, 3);
        }
        state.joint_velocity_data[4 * j + 3] = n;
    }

    // ---- 4. Upload the history of local joint matrices (delayed LBS) ----
    //    texel (4 k + col, j) = column col of L_j for the k-th stored pose
    if (params.delay_active) {
        int const N = delayed_lbs_state::N_history;
        std::vector<float> history_data(4 * N * state.N_joints * 4, 0.0f);
        for (int k = 0; k < N; ++k) {
            auto const& L = dlbs_state.history[k];
            for (int j = 0; j < state.N_joints; ++j)
                for (int col = 0; col < 4; ++col)
                    for (int row = 0; row < 4; ++row)
                        history_data[((j * 4 * N) + 4 * k + col) * 4 + row] = L[j](row, col);
        }
        glBindTexture(GL_TEXTURE_2D, state.history_tex.id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4 * N, state.N_joints,
                        GL_RGBA, GL_FLOAT, history_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    auto t1 = clock::now();
    state.timing_upload_ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
}


// ============================================================================
// Draw with GPU skinning
// ============================================================================

void gpu_skinning_draw(gpu_skinning_state& state,
                       mesh_drawable& drawable,
                       environment_structure const& env,
                       dynamic_skinning_parameters const& params,
                       delayed_lbs_state const& dlbs_state,
                       bool display_bend_weights)
{
    if (!state.initialized) return;

    auto saved_shader = drawable.shader;
    auto saved_textures = drawable.supplementary_texture;

    drawable.shader = state.skinning_shader;

    drawable.supplementary_texture.clear();
    drawable.supplementary_texture["skinning_matrices_tex"]  = state.skinning_matrices_tex;
    drawable.supplementary_texture["hier_weights_tex"]       = state.hier_weights_tex;
    drawable.supplementary_texture["history_tex"]            = state.history_tex;
    drawable.supplementary_texture["dlbs_joint_tex"]         = state.dlbs_joint_tex;

    glUseProgram(state.skinning_shader.id);

    float bend_factor_val = DS_BEND_COUPLING * params.bend_magnitude;
    opengl_uniform(state.skinning_shader, "ds_active", params.active ? 1 : 0);
    opengl_uniform(state.skinning_shader, "hier_max", state.MAX_HIER_PER_VERTEX);
    opengl_uniform(state.skinning_shader, "hier_tile_w", state.HIER_TILE_W);
    opengl_uniform(state.skinning_shader, "bend_factor", bend_factor_val);
    opengl_uniform(state.skinning_shader, "display_bend_weights", display_bend_weights ? 1 : 0);
    opengl_uniform(state.skinning_shader, "dlbs_active", params.delay_active ? 1 : 0);
    opengl_uniform(state.skinning_shader, "dlbs_head", dlbs_state.head);
    opengl_uniform(state.skinning_shader, "dlbs_N", delayed_lbs_state::N_history);
    opengl_uniform(state.skinning_shader, "dlbs_steps_per_distance", delayed_lbs_steps_per_distance(params));
    glUniform4fv(state.skinning_shader.query_uniform_location("joint_velocity"), state.N_joints, state.joint_velocity_data.data());
    glUniform4fv(state.skinning_shader.query_uniform_location("joint_center"), state.N_joints, state.joint_center_data.data());

    draw(drawable, env);

    drawable.shader = saved_shader;
    drawable.supplementary_texture = saved_textures;
}
