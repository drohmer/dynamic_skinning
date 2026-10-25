#include "delayed_lbs.hpp"
#include <algorithm>
#include <cmath>

using namespace cgp;


// Skinning matrices of the current pose, indexed by skeleton joint:  T_j = G_j B_j^-1
//   (G_j: global joint matrix, B_j^-1: inverse bind matrix)
// The rig may only use a subset of the skeleton joints: the other joints get the identity.
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


// ============================================================================
// Initialization — Distances along the skeleton (eq. "distance" of the paper)
// ============================================================================
//
//   Paper:  d_ij = ‖p_i - p_⊥‖ + ‖p_⊥ - q_jmax‖ + Σ (bone lengths from j_max up to j)
//   where p_⊥ is the closest point of p_i on its bone.
//
//   We store this distance relative to the root:
//     d_i = ‖p_i - p_⊥‖ + (distance along the skeleton from the root to p_⊥)   per vertex
//     d_j = cumulated bone lengths from the root to the joint j                per joint
//   so that d_ij = d_i - d_j for any ancestor j of the bone of p_⊥.
//
//   As in the paper, p_⊥ is searched on the bones of the joint j_max of largest skinning weight α_ij
//   (the bone from its parent, and the bones to its children), so that the propagation follows the limb
//   the vertex belongs to, even when another limb is geometrically closer.

void delayed_lbs_initialize(delayed_lbs_state& state, animated_model_structure const& model,
                            std::string const& mesh_name, float current_time)
{
    auto const& rigged_mesh = model.rigged_mesh.at(mesh_name);
    auto const& controller = rigged_mesh.controller_skinning;
    auto const& skeleton = model.skeleton;
    int const N_vertex = rigged_mesh.mesh_bind_pose.position.size();
    int const N_joint = skeleton.size();

    numarray<mat4> const T = skinning_matrices_per_skeleton_joint(model, mesh_name);

    // ---- Per joint: position q_j, and distance d_j to the root (cumulated bone lengths) ----
    // (parents are stored before their children: d_parent is known when d_j is computed)
    numarray<vec3> q(N_joint);
    numarray<float> length_to_root(N_joint);
    for (int j = 0; j < N_joint; ++j) {
        q[j] = skeleton.joint_matrix_global[j].get_block_translation();
        int const parent = skeleton.parent_index[j];
        length_to_root[j] = (parent >= 0) ? length_to_root[parent] + norm(q[j] - q[parent]) : 0.0f;
    }
    state.joint_distance = length_to_root;

    // ---- Per joint: inverse bind matrix B_j^-1 (the delayed global matrix replaces G_j in T_j = G_j B_j^-1) ----
    state.inverse_bind.resize(N_joint);
    state.inverse_bind.fill(mat4::build_identity());
    for (int rig = 0; rig < int(controller.rig_index_to_skeleton_index.size()); ++rig)
        state.inverse_bind[controller.rig_index_to_skeleton_index[rig]] = controller.inverse_bind_matrices[rig];

    // ---- Per joint: children (to find the bones of a joint) ----
    numarray<numarray<int>> children(N_joint);
    for (int j = 0; j < N_joint; ++j)
        if (skeleton.parent_index[j] >= 0)
            children[skeleton.parent_index[j]].push_back(j);

    // ---- Per vertex: distance d_i to the root along the skeleton ----
    //   d_i = ‖p_i - p_⊥‖ + d_parent + ‖p_⊥ - q_parent‖
    //   where p_⊥ is the closest point on the bones (segments parent → child) of the joint j_max
    state.distance.resize(N_vertex);
    for (int i = 0; i < N_vertex; ++i) {
        // Vertex position in the current pose (LBS), consistent with the current joint positions q_j,
        // and joint j_max of largest skinning weight
        vec3 p;
        int j_max = 0;
        float weight_max = -1.0f;
        for (auto const& dep : controller.vertex_to_joint_dependence[i]) {
            int const j = controller.rig_index_to_skeleton_index[dep.joint_index];
            p += dep.weight * T[j].transform_position(rigged_mesh.mesh_bind_pose.position[i]);
            if (dep.weight > weight_max) {
                weight_max = dep.weight;
                j_max = j;
            }
        }

        // Bones of j_max, given by their child joint: the bone from its parent, and the bones to its children
        numarray<int> bones = children[j_max];
        if (skeleton.parent_index[j_max] >= 0)
            bones.push_back(j_max);

        state.distance[i] = norm(p - q[0]);  // used if j_max has no bone
        float closest = 1e10f;
        for (int j : bones) {
            int const parent = skeleton.parent_index[j];
            // Closest point p_⊥ = q_parent + s (q_j - q_parent) on the bone, s ∈ [0, 1]
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

    // ---- History filled with the current pose (no motion propagating at the start) ----
    state.history.resize(delayed_lbs_state::N_history);
    for (int k = 0; k < delayed_lbs_state::N_history; ++k)
        state.history[k] = skeleton.joint_matrix_local;
    state.head = 0;
    state.last_record_time = current_time;
}


// ============================================================================
// Per-frame recording of the pose — history of the local joint matrices L_j(t)
// ============================================================================
//
// The poses are sampled at a fixed time step dt (independent of the frame rate), so that the delay
// d/c corresponds to a fixed number of stored poses. Nothing is recorded while the animation is paused:
// the propagation is frozen.

void delayed_lbs_record(delayed_lbs_state& state, animated_model_structure const& model, float current_time)
{
    int const N = delayed_lbs_state::N_history;
    float const dt = delayed_lbs_state::dt_history;

    // Number of time steps elapsed since the last recorded pose
    int N_steps = int((current_time - state.last_record_time) / dt);
    if (N_steps <= 0)
        return;
    state.last_record_time += N_steps * dt;
    N_steps = std::min(N_steps, N);  // long frame: the whole history is replaced

    // The current pose is pushed once per elapsed time step (the ring buffer advances with the time)
    for (int k = 0; k < N_steps; ++k) {
        state.head = (state.head + 1) % N;
        state.history[state.head] = model.skeleton.joint_matrix_local;
    }
}


// Delay in number of stored poses per unit of distance:  (d / c) / dt
float delayed_lbs_steps_per_distance(dynamic_skinning_parameters const& params)
{
    return 1.0f / (std::max(params.delay_speed, 1e-3f) * delayed_lbs_state::dt_history);
}


// ============================================================================
// Delayed skinning matrix T_j^{delay/i} (eq. dLBS of the paper)
// ============================================================================
//
//   T_j^d = L_root(t - (d_i - d_root)/c) ··· L_parent(j)(t - (d_i - d_parent(j))/c) L_j(t - (d_i - d_j)/c) B_j^-1
//
// Computed from the joint j up to the root, multiplying on the left. Each local matrix is read in the
// history at its own delay, linearly interpolated between the two stored poses around t - (d_i - d_k)/c.
// The delay is clamped to [0, length of the history]: a joint farther than the vertex is not delayed.

mat4 delayed_lbs_skinning_matrix(delayed_lbs_state const& state, numarray<int> const& parent_index,
                                 int j, float distance, float steps_per_distance)
{
    int const N = delayed_lbs_state::N_history;
    mat4 G = mat4::build_identity();  // delayed global matrix, built from the joint j up to the root
    for (int k = j; k >= 0; k = parent_index[k]) {
        // Delay of L_k, in number of time steps (d_ik / c / dt with d_ik = d_i - d_k)
        float const s = std::clamp((distance - state.joint_distance[k]) * steps_per_distance, 0.0f, float(N - 2));
        int const k0 = int(s);
        float const a = s - k0;
        // Stored poses just after (k0 steps back) and just before (k0+1 steps back) the delayed time
        mat4 const& L0 = state.history[(state.head - k0 + N) % N][k];
        mat4 const& L1 = state.history[(state.head - k0 - 1 + N) % N][k];
        G = ((1 - a) * L0 + a * L1) * G;
    }
    return G * state.inverse_bind[j];
}


// ============================================================================
// Per-frame delayed LBS (CPU path)
// ============================================================================
//
//   p_i^dLBS = Σ_j α_ij T_j^d p_i^0         n_i^dLBS = normalize(Σ_j α_ij T_j^d n_i^0)
//
// Replaces the LBS positions and normals of mesh_deformed, before the dynamic skinning deformation.
// The cost is proportional to the depth of the joints in the skeleton (one product per ancestor).

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
        // Same blending as the LBS, with the delayed skinning matrices of the vertex i
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
