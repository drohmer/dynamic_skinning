#include "legacy_loader.hpp"
#include "file_readers.hpp"
#include "environment.hpp"

#include <fstream>
#include <sstream>

using namespace cgp;


// ---- Animation reading ----

struct anim_keyframe {
    float time;
    vec3 translation;
    quaternion rotation;
};

static numarray<numarray<anim_keyframe>> read_skeleton_animation(std::string const& filename, float scaling)
{
    numarray<numarray<anim_keyframe>> result;
    std::ifstream fid(filename);

    while (fid.good()) {
        int N_key = 0;
        fid >> N_key;
        if (!fid.good() || N_key <= 0) break;

        numarray<anim_keyframe> joint_anim;
        joint_anim.resize(N_key);

        for (int k = 0; k < N_key; ++k) {
            float t;
            vec3 p;
            float qx, qy, qz, qw;

            fid >> t >> p.x >> p.y >> p.z >> qx >> qy >> qz >> qw;

            // Normalize quaternion
            float qn = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
            if (qn > 1e-6f) { qx /= qn; qy /= qn; qz /= qn; qw /= qn; }

            joint_anim[k] = {t, p * scaling, {qx, qy, qz, qw}};
        }
        result.push_back(joint_anim);
    }
    return result;
}

static float find_animation_length(numarray<numarray<anim_keyframe>> const& anim)
{
    float end_time = 0;
    float prev_end_time = 0;
    for (int j = 0; j < anim.size(); ++j) {
        for (int k = 0; k < anim[j].size(); ++k) {
            if (anim[j][k].time > end_time) {
                prev_end_time = end_time;
                end_time = anim[j][k].time;
            }
        }
    }
    return end_time - (end_time - prev_end_time) / 10.0f;
}


// ---- Main loader ----

character_structure load_legacy_character(std::string const& directory, float scaling, bool invert_normals,
                                          rotation_transform const& rotation)
{
    std::string dir = project::path + directory;

    // Load mesh with vertex correspondance (OBJ may duplicate vertices for UV)
    numarray<numarray<int>> vertex_correspondance;
    mesh m = mesh_load_file_obj(dir + "mesh.obj", vertex_correspondance);
    m.position *= scaling;
    for (int k = 0; k < m.position.size(); ++k)
        m.position[k] = rotation * m.position[k];
    if (invert_normals) {
        // Flip triangle winding
        for (int k = 0; k < m.connectivity.size(); ++k)
            std::swap(m.connectivity[k].y, m.connectivity[k].z);
    }
    m.normal = normal_per_vertex(m.position, m.connectivity);
    m.fill_empty_field();

    // Load skeleton data
    numarray<std::string> joint_names = read_joint_names(dir + "skeleton_joint_name");
    numarray<int> parent_indices = read_parent_indices(dir + "skeleton_connectivity_parent");
    numarray<vec3> rest_translations = read_vec3_file(dir + "skeleton_rest_pose_translation");
    numarray<quaternion> rest_rotations = read_quaternion_file(dir + "skeleton_rest_pose_rotation");

    int N_joint = joint_names.size();

    // Load rig data (per original vertex, before correspondance)
    numarray<numarray<int>> rig_joints_raw = read_rig_joint(dir + "rig_joint");
    numarray<numarray<float>> rig_weights_raw = read_rig_weights(dir + "rig_weights");

    // Apply vertex correspondance mapping
    // (original vertex k maps to multiple mesh vertices via vertex_correspondance[k])
    int N_vertex = m.position.size();

    // Build mapped skinning weights
    numarray<numarray<skinning_weight_info>> vertex_deps;
    vertex_deps.resize(N_vertex);

    // Map correspondance: for each original vertex k, copy its skinning to all duplicated vertices
    for (int k = 0; k < vertex_correspondance.size(); ++k) {
        numarray<skinning_weight_info> deps;
        if (k < rig_joints_raw.size()) {
            for (int j = 0; j < rig_joints_raw[k].size(); ++j) {
                deps.push_back({rig_joints_raw[k][j], rig_weights_raw[k][j]});
            }
        }
        for (int d = 0; d < vertex_correspondance[k].size(); ++d) {
            int mapped_vertex = vertex_correspondance[k][d];
            if (mapped_vertex < N_vertex)
                vertex_deps[mapped_vertex] = deps;
        }
    }

    // ---- Build skeleton ----
    skeleton_structure skeleton;
    skeleton.joint_name.resize(N_joint);
    skeleton.parent_index.resize(N_joint);
    skeleton.joint_matrix_local.resize(N_joint);
    skeleton.joint_matrix_global.resize(N_joint);

    for (int k = 0; k < N_joint; ++k) {
        skeleton.joint_name[k] = joint_names[k];
        skeleton.parent_index[k] = parent_indices[k];
        skeleton.joint_matrix_local[k] = build_joint_matrix(rest_translations[k] * scaling, rest_rotations[k]);
    }
    // Global rotation of the model: applied to the root joints
    mat4 const M_rotation = mat4::build_affine(rotation.matrix(), vec3{0, 0, 0});
    for (int k = 0; k < N_joint; ++k)
        if (parent_indices[k] < 0)
            skeleton.joint_matrix_local[k] = M_rotation * skeleton.joint_matrix_local[k];
    skeleton.update_joint_matrix_local_to_global();

    // ---- Build controller_skinning ----
    // In old format, rig joints directly index the skeleton (no separate rig-to-skeleton mapping)
    controller_skinning_structure controller;
    controller.vertex_to_joint_dependence = vertex_deps;

    // rig_index_to_skeleton_index is identity: joint i in rig = joint i in skeleton
    controller.rig_index_to_skeleton_index.resize(N_joint);
    for (int k = 0; k < N_joint; ++k)
        controller.rig_index_to_skeleton_index[k] = k;

    // Compute inverse bind matrices from rest pose global matrices
    controller.inverse_bind_matrices.resize(N_joint);
    for (int k = 0; k < N_joint; ++k)
        controller.inverse_bind_matrices[k] = inverse(skeleton.joint_matrix_global[k]);

    controller.global_bind_matrix = mat4::build_identity();

    // ---- Build animation ----
    // Check if animation file exists
    std::string anim_file = dir + "skeleton_animation";
    std::ifstream test_anim(anim_file);
    bool has_animation = test_anim.good();
    test_anim.close();

    float anim_time_max = 2.0f;
    skeleton_animation_structure anim;
    anim.joint_index.resize(N_joint);
    anim.times.resize(N_joint);
    anim.matrix.resize(N_joint);

    for (int k = 0; k < N_joint; ++k)
        anim.joint_index[k] = k;

    if (has_animation) {
        auto raw_anim = read_skeleton_animation(anim_file, scaling);
        anim_time_max = find_animation_length(raw_anim);

        for (int k = 0; k < N_joint && k < raw_anim.size(); ++k) {
            int N_key = raw_anim[k].size();
            anim.times[k].resize(N_key);
            anim.matrix[k].resize(N_key);
            for (int t = 0; t < N_key; ++t) {
                anim.times[k][t] = raw_anim[k][t].time;
                anim.matrix[k][t] = build_joint_matrix(raw_anim[k][t].translation, raw_anim[k][t].rotation);
                if (parent_indices[k] < 0)
                    anim.matrix[k][t] = M_rotation * anim.matrix[k][t];
            }
        }
    }
    else {
        // Static pose: 3 identical keyframes
        for (int k = 0; k < N_joint; ++k) {
            anim.times[k].resize(3);
            anim.matrix[k].resize(3);
            for (int t = 0; t < 3; ++t) {
                anim.times[k][t] = static_cast<float>(t);
                anim.matrix[k][t] = skeleton.joint_matrix_local[k];
            }
        }
    }
    anim.time_max = anim_time_max;

    // ---- Assemble character ----
    character_structure character;
    animated_model_structure& model = character.animated_model;
    model.skeleton = skeleton;

    rigged_mesh_structure rigged;
    rigged.mesh_bind_pose = m;
    rigged.mesh_deformed = m;
    rigged.controller_skinning = controller;
    // Per-vertex bend weight γ^bend: 0.5 for all legacy models, as in the original code
    // (init_local_buffers: weight_flappy.fill(0.5f))
    rigged.weight_bend.resize(m.position.size());
    rigged.weight_bend.fill(0.5f);
    model.rigged_mesh["body"] = rigged;
    model.animation["Default"] = anim;

    character.current_animation_name = "Default";
    character.timer.event_period = anim_time_max;

    // Initialize drawable
    mesh_drawable drawable;
    drawable.initialize_data_on_gpu(m);

    // Try to load texture
    std::string tex_png = dir + "texture.png";
    std::string tex_jpg = dir + "texture.jpg";
    std::ifstream test_png(tex_png);
    std::ifstream test_jpg(tex_jpg);
    if (test_png.good()) {
        drawable.texture.load_and_initialize_texture_2d_on_gpu(tex_png);
    } else if (test_jpg.good()) {
        drawable.texture.load_and_initialize_texture_2d_on_gpu(tex_jpg);
    }
    test_png.close();
    test_jpg.close();

    character.drawable["body"] = drawable;
    character.sk_drawable = skeleton_drawable(model.skeleton);

    return character;
}


std::vector<legacy_model_entry> get_legacy_models()
{
    return {
        {"Clown",       "assets/clown/",        1.0f,  false, {}},
        {"Dragon",      "assets/dragon/",       0.09f, false, {}},
        {"Girafe",      "assets/girafe/",       2.0f,  true, {}},
        {"Snail",       "assets/snail/",        1.0f,  false, rotation_transform::from_axis_angle({1, 0, 0}, -Pi / 2)},  // Z-up model
        {"Spot",        "assets/spot/",         1.0f,  false, rotation_transform::from_axis_angle({1, 0, 0}, -Pi / 2)},  // Z-up model
    };
}
