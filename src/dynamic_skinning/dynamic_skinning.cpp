#include "dynamic_skinning.hpp"
#include "oscillation_filter.hpp"
#include <map>
#include <vector>
#include <chrono>

using namespace cgp;


// Bend deformer ψ^bend (eq. 9, Algorithm 2 lines 7-10)
//   Paper:  ψ^bend(Ω, q, p) = (R_κ - I) · (p - c)
//   where c = projection of p onto the rotation axis through q_j,
//   and R_κ is a rotation with axis ω = Ω/‖Ω‖ and angle θ = -γ^bend · ‖Ω × (p-q)‖.
//
//   Using Rodrigues' formula directly for (R-I)·v avoids building the 3×3 matrix:
//     (R-I)·v = sin(θ) (ω × v) + (1 - cos(θ)) (ω × (ω × v))
static vec3 bend_deformer(float gamma_bend,
                           vec3 const& p_vertex,
                           vec3 const& p_joint,
                           vec3 const& omega_dir,
                           float cross_omega_u_norm)
{
    vec3 const u = p_vertex - p_joint;

    // v = p - c, where c = q_j + (u · ω)ω  (projection onto rotation axis)
    // So v = u - (u · ω)ω = component of u perpendicular to ω
    float u_dot_w = dot(u, omega_dir);
    vec3 const v = u - u_dot_w * omega_dir;

    float theta = -gamma_bend * cross_omega_u_norm;
    float sin_t = std::sin(theta);
    float cos_t_m1 = std::cos(theta) - 1.0f;   // = -(1 - cos(θ))

    // (R-I)·v via Rodrigues:  sin(θ)(ω×v) + (1-cos(θ))(ω×(ω×v))
    // Since v ⊥ ω:  ω×(ω×v) = -v,  so:  (R-I)·v = sin(θ)(ω×v) - (1-cos(θ))v
    vec3 const w_cross_v = cross(omega_dir, v);
    return sin_t * w_cross_v + cos_t_m1 * v;
}


// ============================================================================
// Initialization — Compute hierarchical weights β_ij (Appendix A, Algorithm 1)
// ============================================================================

void dynamic_skinning_initialize(dynamic_skinning_state& state,
                                 animated_model_structure const& model,
                                 std::string const& mesh_name)
{
    auto const& rigged_mesh = model.rigged_mesh.at(mesh_name);
    auto const& controller = rigged_mesh.controller_skinning;
    int N_vertex = rigged_mesh.mesh_bind_pose.position.size();
    int N_skeleton_joints = model.skeleton.size();

    // Angular velocity history per joint (L_Ω_j in Algorithm 2 line 13)
    state.joint_angular_velocity_tracker.resize(N_skeleton_joints);
    for (int k = 0; k < N_skeleton_joints; ++k)
        state.joint_angular_velocity_tracker[k] = angular_velocity_tracker();

    state.deformation_per_vertex.resize(N_vertex);
    state.deformation_per_vertex.fill({0, 0, 0});

    // Per-vertex bend weight γ_i^bend (Sec. 6.1 — weight painting)
    if (rigged_mesh.weight_bend.size() == N_vertex) {
        state.weight_bend = rigged_mesh.weight_bend;
    } else {
        state.weight_bend.resize(N_vertex);
        state.weight_bend.fill(1.0f);
    }

    // Build gather mapping: vertex → joints with hierarchical weights β_ij
    // (Algorithm 1 / eq. 6: β_ij = Σ_{j_k ∈ D(j)} α_{ij_k})
    state.joints_per_vertex.resize(N_vertex);
    state.hierarchical_weights.resize(N_vertex);
    state.center_joints_per_vertex.resize(N_vertex);
    for (int k = 0; k < N_vertex; ++k) {
        state.joints_per_vertex[k].clear();
        state.hierarchical_weights[k].clear();
        state.center_joints_per_vertex[k].clear();
    }

    // Flexible origins below each joint (strict descendants)
    std::map<int, std::vector<int>> origins_below_joint;
    for (int m : model.flexible_origin_joints)
        for (int j = model.skeleton.parent_index[m]; j >= 0; j = model.skeleton.parent_index[j])
            origins_below_joint[j].push_back(m);

    state.filtered_angular_velocity.resize(N_skeleton_joints);
    state.filtered_angular_velocity.fill({0, 0, 0});

    // For each vertex, propagate skinning weights α_ij up the skeleton hierarchy
    // to compute β_ij (each ancestor joint accumulates the weights of its descendants)
    for (int k_vertex = 0; k_vertex < N_vertex; ++k_vertex) {
        auto const& deps = controller.vertex_to_joint_dependence[k_vertex];
        int N_dep = deps.size();

        std::map<int, float> beta_per_joint;  // β_ij for this vertex

        for (int k = 0; k < N_dep; ++k) {
            int rig_joint = deps[k].joint_index;
            float alpha = deps[k].weight;                               // skinning weight α
            int current_joint = controller.rig_index_to_skeleton_index[rig_joint];

            // Walk up to root: β_{i,ancestor} += α_{i,j}  (Algorithm 1)
            while (current_joint >= 0) {
                beta_per_joint[current_joint] += alpha;
                current_joint = model.skeleton.parent_index[current_joint];
            }
        }

        for (auto const& entry : beta_per_joint) {
            // Filter out excluded joints (e.g. FBones for cthulhu model)
            if (model.excluded_ds_joints.count(entry.first) > 0)
                continue;

            // Joint above flexible origins: only the part of the vertex in each flexible extremity
            // (weight β_im) is bent, around the origin q_m; the remaining part (rigid base) is not deformed
            auto const it = origins_below_joint.find(entry.first);
            if (it != origins_below_joint.end()) {
                for (int m : it->second) {
                    auto const beta_m = beta_per_joint.find(m);
                    if (beta_m == beta_per_joint.end()) continue;
                    state.joints_per_vertex[k_vertex].push_back(entry.first);
                    state.hierarchical_weights[k_vertex].push_back(beta_m->second);
                    state.center_joints_per_vertex[k_vertex].push_back(m);
                }
                continue;
            }

            state.joints_per_vertex[k_vertex].push_back(entry.first);
            state.hierarchical_weights[k_vertex].push_back(entry.second);
            state.center_joints_per_vertex[k_vertex].push_back(entry.first);
        }
    }
}


// ============================================================================
// Per-frame dynamic skinning (Algorithm 2, Appendix E)
// ============================================================================
//
// Two-phase approach:
//   Phase 1 — Compute filtered angular velocity F^osc(Ω_j, t) per joint
//             (Algorithm 2, lines 14-22)
//   Phase 2 — Gather deformation per vertex: d_i = Σ_j β_ij · ψ^bend(…)
//             (eq. 8 with eq. 5-6)

void dynamic_skinning_apply(dynamic_skinning_state& state,
                            dynamic_skinning_parameters const& params,
                            animated_model_structure& model,
                            std::string const& mesh_name,
                            float current_time)
{
    auto& rigged_mesh = model.rigged_mesh.at(mesh_name);
    mesh& mesh_deformed = rigged_mesh.mesh_deformed;
    int N_vertex = mesh_deformed.position.size();
    int N_joint = model.skeleton.size();

    // Bend strength is independent of the oscillation amplitude (original: 0.4 * flapping_power)
    float const default_bend_factor = DS_BEND_COUPLING;

    state.deformation_per_vertex.fill({0, 0, 0});
    state.filtered_angular_velocity.fill({0, 0, 0});

    using clock = std::chrono::high_resolution_clock;
    auto t0 = clock::now();

    // ---- Phase 1: filtered angular velocity per joint ----
    compute_filtered_angular_velocities(
        state.joint_angular_velocity_tracker,
        state.filtered_angular_velocity,
        model.skeleton, params, current_time);

    auto t1 = clock::now();

    // ---- Phase 2: gather deformation per vertex (eq. 8, Algorithm 2, lines 24-34) ----
    //
    //   d_i = Σ_j β_ij · ψ^bend(γ_i^bend, p_i, q_j, F^osc(Ω_j))      (eq. 8)
    //
    // where β_ij are hierarchical weights (eq. 6) and ψ^bend is the
    // bend deformer (eq. 9).

    // Precompute per-joint data: position, direction, norm (avoids redundant
    // work when many vertices share the same joint).
    // Also build a mask of active joints (‖Ω‖ > threshold) for early-out.
    struct joint_precomp { vec3 position; vec3 omega_dir; float omega_norm; };
    numarray<joint_precomp> joint_data(N_joint);
    numarray<int> joint_active(N_joint);
    for (int j = 0; j < N_joint; ++j) {
        vec3 const& omega = state.filtered_angular_velocity[j];
        float on = norm(omega);
        joint_active[j] = (on >= 1e-2f);
        joint_data[j].position = model.skeleton.joint_matrix_global[j].get_block_translation();  // also used as a rotation center
        if (joint_active[j]) {
            joint_data[j].omega_dir  = omega / on;
            joint_data[j].omega_norm = on;
        }
    }

    float const bend_base = default_bend_factor * params.bend_magnitude;

    for (int i = 0; i < N_vertex; ++i) {
        auto const& joints = state.joints_per_vertex[i];
        auto const& weights = state.hierarchical_weights[i];
        auto const& centers = state.center_joints_per_vertex[i];
        vec3 const& p_vertex = mesh_deformed.position[i];
        float w_bend = bend_base * state.weight_bend[i];

        for (int k = 0; k < static_cast<int>(joints.size()); ++k) {
            int j = joints[k];
            if (!joint_active[j]) continue;

            float beta_ij = weights[k];                                    // eq. 6
            vec3 const& omega_dir = joint_data[j].omega_dir;
            vec3 const& p_joint   = joint_data[centers[k]].position;           // q_j, or the flexible origin
            float omega_norm      = joint_data[j].omega_norm;

            // ‖Ω × u‖ = ‖Ω‖ · ‖ω × u‖  where ω = Ω/‖Ω‖
            vec3 u = p_vertex - p_joint;
            float cross_norm = omega_norm * norm(cross(omega_dir, u));     // ‖Ω × u‖

            vec3 bend = bend_deformer(w_bend, p_vertex, p_joint,           // ψ^bend (eq. 9)
                                       omega_dir, cross_norm);
            state.deformation_per_vertex[i] += beta_ij * bend;             // eq. 8
        }
    }

    auto t2 = clock::now();

    // Apply accumulated deformations: p_i' = p_i + d_i  (eq. 5)
    for (int k = 0; k < N_vertex; ++k) {
        mesh_deformed.position[k] += state.deformation_per_vertex[k];
    }

    // Recompute normals after deformation
    mesh_deformed.normal = normal_per_vertex(mesh_deformed.position, mesh_deformed.connectivity);

    auto t3 = clock::now();

    // Store timings
    auto to_ms = [](auto d) { return std::chrono::duration<float, std::milli>(d).count(); };
    state.timing_phase1_ms  = to_ms(t1 - t0);
    state.timing_phase2_ms  = to_ms(t2 - t1);
    state.timing_normals_ms = to_ms(t3 - t2);
}


void dynamic_skinning_display_gui(dynamic_skinning_parameters& params)
{
    ImGui::Checkbox("Active", &params.active);
    if (params.active) {
        ImGui::SliderFloat("Amplitude", &params.amplitude, 0.0f, 5.0f);
        ImGui::SliderFloat("Drag", &params.drag, 0.0f, 3.0f);
        ImGui::SliderFloat("Frequency", &params.frequency, 0.0f, 30.0f);
        ImGui::SliderFloat("Attenuation", &params.attenuation, 0.0f, 5.0f);
        ImGui::SliderFloat("Freq Slope", &params.frequency_slope, 0.0f, 30.0f);
        ImGui::SliderFloat("Bend Max Angle", &params.bend_max_angle, 0.0f, 6.28f);
        ImGui::SliderFloat("Bend Magnitude", &params.bend_magnitude, 0.0f, 3.0f);
    }
}
