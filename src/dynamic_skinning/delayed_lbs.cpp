#include "delayed_lbs.hpp"
#include <algorithm>
#include <cmath>

using namespace cgp;


numarray<mat4> skinning_matrices_per_skeleton_joint(animated_model_structure const& model,
                                                    std::string const& mesh_name)
{
    auto const& controller = model.rigged_mesh.at(mesh_name).controller_skinning;
    numarray<mat4> T(model.skeleton.size());
    T.fill(mat4::build_identity());
    int const N_rig = controller.rig_index_to_skeleton_index.size();
    for (int rig = 0; rig < N_rig; ++rig) {
        int const j = controller.rig_index_to_skeleton_index[rig];
        T[j] = model.skeleton.joint_matrix_global[j] * controller.inverse_bind_matrices[rig];
    }
    return T;
}


void delayed_lbs_initialize(delayed_lbs_state& state, animated_model_structure const& model,
                            std::string const& mesh_name, float current_time)
{
    auto const& rigged_mesh = model.rigged_mesh.at(mesh_name);
    auto const& controller = rigged_mesh.controller_skinning;
    auto const& skeleton = model.skeleton;
    int const N_vertex = rigged_mesh.mesh_bind_pose.position.size();
    int const N_joint = skeleton.size();

    numarray<mat4> const T = skinning_matrices_per_skeleton_joint(model, mesh_name);

    // Joint positions, and cumulated bone lengths from the root to each joint
    numarray<vec3> q(N_joint);
    numarray<float> length_to_root(N_joint);
    for (int j = 0; j < N_joint; ++j) {
        q[j] = skeleton.joint_matrix_global[j].get_block_translation();
        int const parent = skeleton.parent_index[j];
        length_to_root[j] = (parent >= 0) ? length_to_root[parent] + norm(q[j] - q[parent]) : 0.0f;
    }
    state.joint_distance = length_to_root;

    state.inverse_bind.resize(N_joint);
    state.inverse_bind.fill(mat4::build_identity());
    for (int rig = 0; rig < int(controller.rig_index_to_skeleton_index.size()); ++rig)
        state.inverse_bind[controller.rig_index_to_skeleton_index[rig]] = controller.inverse_bind_matrices[rig];

    // d_i = ||p_i - p_perp|| + distance along the skeleton from the root to p_perp,
    // where p_perp is the closest point on the bones (segments parent -> child)
    state.distance.resize(N_vertex);
    for (int i = 0; i < N_vertex; ++i) {
        // Vertex position in the current pose (LBS)
        vec3 p;
        for (auto const& dep : controller.vertex_to_joint_dependence[i]) {
            int const j = controller.rig_index_to_skeleton_index[dep.joint_index];
            p += dep.weight * T[j].transform_position(rigged_mesh.mesh_bind_pose.position[i]);
        }

        state.distance[i] = norm(p - q[0]);  // used if the skeleton has no bone
        float closest = 1e10f;
        for (int j = 0; j < N_joint; ++j) {
            int const parent = skeleton.parent_index[j];
            if (parent < 0) continue;
            vec3 const bone = q[j] - q[parent];
            float const L2 = dot(bone, bone);
            float const s = (L2 > 1e-12f) ? std::clamp(dot(p - q[parent], bone) / L2, 0.0f, 1.0f) : 0.0f;
            vec3 const p_perp = q[parent] + s * bone;
            float const distance_to_bone = norm(p - p_perp);
            if (distance_to_bone < closest) {
                closest = distance_to_bone;
                state.distance[i] = distance_to_bone + length_to_root[parent] + s * std::sqrt(L2);
            }
        }
    }

    // History filled with the current pose (no jump at the start)
    state.history.resize(delayed_lbs_state::N_history);
    for (int k = 0; k < delayed_lbs_state::N_history; ++k)
        state.history[k] = skeleton.joint_matrix_local;
    state.head = 0;
    state.last_record_time = current_time;
}


void delayed_lbs_record(delayed_lbs_state& state, animated_model_structure const& model, float current_time)
{
    int const N = delayed_lbs_state::N_history;
    float const dt = delayed_lbs_state::dt_history;
    int N_steps = int((current_time - state.last_record_time) / dt);
    if (N_steps <= 0)
        return;
    state.last_record_time += N_steps * dt;
    N_steps = std::min(N_steps, N);  // long frame: the whole history is replaced

    for (int k = 0; k < N_steps; ++k) {
        state.head = (state.head + 1) % N;
        state.history[state.head] = model.skeleton.joint_matrix_local;
    }
}


float delayed_lbs_steps_per_distance(dynamic_skinning_parameters const& params)
{
    return 1.0f / (std::max(params.delay_speed, 1e-3f) * delayed_lbs_state::dt_history);
}


mat4 delayed_lbs_skinning_matrix(delayed_lbs_state const& state, numarray<int> const& parent_index,
                                 int j, float distance, float steps_per_distance)
{
    int const N = delayed_lbs_state::N_history;
    mat4 G = mat4::build_identity();
    for (int k = j; k >= 0; k = parent_index[k]) {
        // Delay of L_k, in number of time steps, interpolated between two stored poses
        float const s = std::clamp((distance - state.joint_distance[k]) * steps_per_distance, 0.0f, float(N - 2));
        int const k0 = int(s);
        float const a = s - k0;
        mat4 const& L0 = state.history[(state.head - k0 + N) % N][k];
        mat4 const& L1 = state.history[(state.head - k0 - 1 + N) % N][k];
        G = ((1 - a) * L0 + a * L1) * G;
    }
    return G * state.inverse_bind[j];
}


void delayed_lbs_apply(delayed_lbs_state const& state, dynamic_skinning_parameters const& params,
                       animated_model_structure& model, std::string const& mesh_name)
{
    auto& rigged_mesh = model.rigged_mesh.at(mesh_name);
    auto const& controller = rigged_mesh.controller_skinning;
    mesh const& mesh_bind_pose = rigged_mesh.mesh_bind_pose;
    mesh& mesh_deformed = rigged_mesh.mesh_deformed;
    numarray<int> const& parent_index = model.skeleton.parent_index;
    int const N_vertex = mesh_bind_pose.position.size();
    float const steps_per_distance = delayed_lbs_steps_per_distance(params);

    for (int i = 0; i < N_vertex; ++i) {
        vec3 const& p0 = mesh_bind_pose.position[i];
        vec3 const& n0 = mesh_bind_pose.normal[i];
        vec3 p, n;
        for (auto const& dep : controller.vertex_to_joint_dependence[i]) {
            int const j = controller.rig_index_to_skeleton_index[dep.joint_index];
            mat4 const T = delayed_lbs_skinning_matrix(state, parent_index, j, state.distance[i], steps_per_distance);
            p += dep.weight * T.transform_position(p0);
            n += dep.weight * T.transform_vector(n0);
        }
        mesh_deformed.position[i] = p;
        mesh_deformed.normal[i] = normalize(n);
    }
}
