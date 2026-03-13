#include "dynamic_skinning.hpp"
#include <map>

using namespace cgp;


// ============================================================================
// Internal filter functions
// ============================================================================

// Oscillation kernel κ (eq. 11) — without the amplitude A which is applied
// separately in the convolution.
//   Paper:  κ(t) = A exp(-t/τ) sin(2πf t)
//   Code:   κ(t) =   exp(-t/τ) sin(ω t)   with ω = params.frequency = 2πf
// The frequency_slope extension allows a time-varying frequency (Appendix C, κ₁).
static float oscillating_kernel(int k, dynamic_skinning_parameters const& params)
{
    float dt = 0.017f;
    float t = k * dt;
    float df = t * params.frequency_slope;                 // linear freq increase (Appendix C)
    float phase = (params.frequency + df) * t;             // ω·t with ω = 2πf
    return std::exp(-params.attenuation * t) * std::sin(phase);  // attenuation = 1/τ
}

// Finite-difference approximation of the kernel derivative: dκ/dt ≈ (κ(t) - κ(t-Δt)) / Δt.
// Used in the integration-by-parts form of the convolution (eq. 14, Appendix B):
//   F^osc(Ω_j, t) = Ω_j(t) + ∫₀ᵂ κ(u) Ω̇_j(t-u) du
//                  = Ω_j(t) + ∫₀ᵂ κ̇(u) Ω_j(t-u) du       [integration by parts]
static float kernel_derivative(int k, dynamic_skinning_parameters const& params)
{
    float dt = 0.017f;
    return (oscillating_kernel(k, params) - oscillating_kernel(k - 1, params)) / dt;
}


// Bend deformer ψ^bend (eq. 9, Algorithm 2 lines 7-10)
//   Paper:  ψ^bend(Ω, q, p) = R_κ(Ω, p-q) · (p - q)
//   where R_κ is a rotation with:
//     - axis  = Ω / ‖Ω‖
//     - angle = -γ^bend · ‖Ω × (p - q)‖
//   The deformation offset added to the vertex position is (R - I)·(p - q),
//   since R preserves the component along the axis, (R-I) zeroes it.
static vec3 bend_deformer(float gamma_bend,
                           vec3 const& p_vertex,
                           vec3 const& p_joint,
                           vec3 const& omega_dir,
                           float cross_omega_u_norm,
                           float /*bend_max_angle*/)
{
    vec3 const u = p_vertex - p_joint;
    mat3 const Id = mat3::build_identity();

    // Rotation center = projection of p onto the rotation axis through q_j
    vec3 const rotation_center = p_joint + dot(u, omega_dir) * omega_dir;
    float rotation_angle = cross_omega_u_norm * gamma_bend;    // -γ^bend ‖Ω × u‖

    mat3 const R = rotation_transform::from_axis_angle(omega_dir, -rotation_angle).matrix();
    vec3 const deformation = (R - Id) * (p_vertex - rotation_center);  // = (R-I)·u

    return deformation;
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
    state.weight_bend.resize(N_vertex);
    state.weight_bend.fill(1.0f);

    // Build gather mapping: vertex → joints with hierarchical weights β_ij
    // (Algorithm 1 / eq. 6: β_ij = Σ_{j_k ∈ D(j)} α_{ij_k})
    state.joints_per_vertex.resize(N_vertex);
    state.hierarchical_weights.resize(N_vertex);
    for (int k = 0; k < N_vertex; ++k) {
        state.joints_per_vertex[k].clear();
        state.hierarchical_weights[k].clear();
    }

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
            state.joints_per_vertex[k_vertex].push_back(entry.first);
            state.hierarchical_weights[k_vertex].push_back(entry.second);
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
                            animated_model_structure const& model,
                            std::string const& mesh_name,
                            float current_time)
{
    auto const& rigged_mesh = model.rigged_mesh.at(mesh_name);
    mesh& mesh_deformed = const_cast<rigged_mesh_structure&>(rigged_mesh).mesh_deformed;
    int N_vertex = mesh_deformed.position.size();
    int N_joint = model.skeleton.size();

    // Coupling coefficient: 0.4 · A (empirical, see Appendix E §E.2)
    float const default_bend_factor = 0.4f * params.amplitude;

    state.deformation_per_vertex.fill({0, 0, 0});
    state.filtered_angular_velocity.fill({0, 0, 0});

    // ---- Phase 1: filtered angular velocity per joint (Algorithm 2, lines 14-22) ----
    //
    // For each joint j, compute the filtered angular velocity F^osc(Ω_j, t)
    // using the integration-by-parts form (eq. 14, Appendix B):
    //   F^osc(Ω_j, t) = Ω_j(t) + A · ∫₀ᵂ κ̇(u) · Ω_j(t-u) du
    //
    // The integral is discretized as a sum over the history buffer (eq. 15).
    for (int joint = 0; joint < N_joint; ++joint) {
        mat3 R_local = model.skeleton.joint_matrix_local[joint].get_block_linear();

        // Extract local rotation as quaternion for angular velocity tracking
        quaternion q = rotation_transform::from_matrix(R_local).get_quaternion();
        float const q_norm = norm(q);
        if (q_norm < 1e-6f) continue;
        q = q / q_norm;

        // Update angular velocity history L_{Ω_j} (Algorithm 2, line 13)
        state.joint_angular_velocity_tracker[joint].add(q, current_time);

        auto& velocity_history = state.joint_angular_velocity_tracker[joint].angular_velocity_history;
        if (velocity_history.size() < 2)
            continue;

        // Ω_j(t) — most recent angular velocity sample
        vec3 angular_velocity = velocity_history[velocity_history.size() - 1];

        // Oscillation filter: discrete convolution with kernel derivative κ̇
        // (eq. 14-15, Appendix B)
        //   F^osc(Ω_j, t) = Ω_j(t) + (A/10) · Σ_k κ̇(k) · Ω_j(t - k·Δt)
        // The /10 factor is an empirical scaling (Appendix E §E.2).
        int N_integral = 300;
        if (static_cast<int>(velocity_history.size()) > N_integral) {
            vec3 v_integral = {0, 0, 0};
            for (int k = 0; k < N_integral; ++k) {
                vec3 v = velocity_history[velocity_history.size() - 1 - k];
                v_integral += v * kernel_derivative(k, params);
            }
            float A = params.amplitude / 10.0f;
            angular_velocity = angular_velocity + A * v_integral;
        }

        // Transform angular velocity from local to global coordinates
        // (Algorithm 2, line 23: Ω_j^global = R_parent^global · Ω_j^local)
        if (joint != 0) {
            int parent_id = model.skeleton.parent_index[joint];
            if (parent_id >= 0) {
                mat3 R_parent = model.skeleton.joint_matrix_global[parent_id].get_block_linear();
                angular_velocity = R_parent * angular_velocity;
            }
        }

        state.filtered_angular_velocity[joint] = angular_velocity;
    }

    // ---- Phase 2: gather deformation per vertex (eq. 8, Algorithm 2, lines 24-34) ----
    //
    //   d_i = Σ_j β_ij · ψ^bend(γ_i^bend, p_i, q_j, F^osc(Ω_j))      (eq. 8)
    //
    // where β_ij are hierarchical weights (eq. 6) and ψ^bend is the
    // bend deformer (eq. 9).
    for (int i = 0; i < N_vertex; ++i) {
        auto const& joints = state.joints_per_vertex[i];
        auto const& weights = state.hierarchical_weights[i];
        vec3 const& p_vertex = mesh_deformed.position[i];
        float w_bend = default_bend_factor * params.bend_magnitude * state.weight_bend[i];

        for (int k = 0; k < static_cast<int>(joints.size()); ++k) {
            int j = joints[k];
            float beta_ij = weights[k];                                    // eq. 6
            vec3 const& omega = state.filtered_angular_velocity[j];        // F^osc(Ω_j)
            float omega_norm = norm(omega);
            if (omega_norm < 1e-2f) continue;

            vec3 p_joint = model.skeleton.joint_matrix_global[j].get_block_translation();
            vec3 omega_dir = omega / omega_norm;
            vec3 vertex_velocity = cross(omega, p_vertex - p_joint);       // Ω × u

            vec3 bend = bend_deformer(w_bend, p_vertex, p_joint,           // ψ^bend (eq. 9)
                                       omega_dir, norm(vertex_velocity),
                                       params.bend_max_angle);
            state.deformation_per_vertex[i] += beta_ij * bend;             // eq. 8
        }
    }

    // Apply accumulated deformations: p_i' = p_i + d_i  (eq. 5)
    for (int k = 0; k < N_vertex; ++k) {
        mesh_deformed.position[k] += state.deformation_per_vertex[k];
    }

    // Recompute normals after deformation
    mesh_deformed.normal = normal_per_vertex(mesh_deformed.position, mesh_deformed.connectivity);
}


void dynamic_skinning_display_gui(dynamic_skinning_parameters& params)
{
    ImGui::Checkbox("Active", &params.active);
    if (params.active) {
        ImGui::SliderFloat("Amplitude", &params.amplitude, 0.0f, 2.0f);
        ImGui::SliderFloat("Frequency", &params.frequency, 0.0f, 30.0f);
        ImGui::SliderFloat("Attenuation", &params.attenuation, 0.0f, 5.0f);
        ImGui::SliderFloat("Freq Slope", &params.frequency_slope, 0.0f, 30.0f);
        ImGui::SliderFloat("Bend Max Angle", &params.bend_max_angle, 0.0f, 6.28f);
        ImGui::SliderFloat("Bend Magnitude", &params.bend_magnitude, 0.0f, 3.0f);
    }
}
