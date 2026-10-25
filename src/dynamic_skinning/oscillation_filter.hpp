#pragma once

#include "dynamic_skinning.hpp"

#include <cmath>


// ============================================================================
// Named constants for dynamic skinning
// ============================================================================

constexpr float DS_BEND_COUPLING      = 0.4f;     // Coupling coefficient (Appendix E §E.2)
constexpr int   DS_N_INTEGRAL         = 300;      // Number of convolution samples (5 s of history at DS_HISTORY_DT)


// ============================================================================
// Oscillation kernel (eq. 11)
// ============================================================================

// Oscillation kernel κ (eq. 11), at the time t = k Δt of the k-th sample of the history:
//   κ(t) = A exp(-t/τ) sin(ω t)   with A = params.amplitude, ω = params.frequency, 1/τ = params.attenuation
// The frequency_slope extension allows a time-varying frequency (Appendix C, κ₁).
inline float oscillating_kernel(int k, dynamic_skinning_parameters const& params)
{
    float t = k * DS_HISTORY_DT;
    float df = t * params.frequency_slope;
    float phase = (params.frequency + df) * t;
    return params.amplitude * std::exp(-params.attenuation * t) * std::sin(phase);
}


// ============================================================================
// Phase 1: Compute filtered angular velocity per joint (Algorithm 2)
// ============================================================================
//
// For each joint j:
//   1. Extract local rotation as quaternion, feed to angular velocity tracker
//   2. Convolve the history of the changes of velocity with the oscillation kernel (eq. 14-15)
//   3. Transform from local to global coordinates via parent rotation
//
// Result is written to filtered_velocity[joint].
// This function is shared between CPU and GPU paths.

inline void compute_filtered_angular_velocities(
    cgp::numarray<angular_velocity_tracker>& trackers,
    cgp::numarray<cgp::vec3>& filtered_velocity,
    skeleton_structure const& skeleton,
    dynamic_skinning_parameters const& params,
    float current_time)
{
    int N_joint = skeleton.size();
    filtered_velocity.fill({0, 0, 0});

    for (int joint = 0; joint < N_joint; ++joint) {
        cgp::mat3 R_local = skeleton.joint_matrix_local[joint].get_block_linear();
        cgp::quaternion q = cgp::rotation_transform::from_matrix(R_local).get_quaternion();
        float const q_norm = cgp::norm(q);
        if (q_norm < 1e-6f) continue;
        q = q / q_norm;

        trackers[joint].add(q, current_time);

        auto& velocity_history = trackers[joint].angular_velocity_history;
        if (velocity_history.size() < 2) continue;

        // Drag term (velocity skinning), weighted by D
        cgp::vec3 angular_velocity = params.drag * velocity_history[velocity_history.size() - 1];

        // Oscillation term (numerical form of eq. 15):  Σ_k κ(k Δt) (Ω((i-k) Δt) - Ω((i-k-1) Δt))
        //   (integration by parts of ∫ κ'(u) Ω(t-u) du; causal: no contribution of κ for t < 0)
        int const N_history = velocity_history.size();
        if (N_history > DS_N_INTEGRAL) {
            cgp::vec3 v_integral = {0, 0, 0};
            for (int k = 0; k < DS_N_INTEGRAL; ++k) {
                cgp::vec3 const delta_v = velocity_history[N_history - 1 - k] - velocity_history[N_history - 2 - k];
                v_integral += oscillating_kernel(k, params) * delta_v;
            }
            // F(Omega) = D * Omega + Σ κ ΔOmega
            angular_velocity = angular_velocity + v_integral;
        }

        if (joint != 0) {
            int parent_id = skeleton.parent_index[joint];
            if (parent_id >= 0) {
                cgp::mat3 R_parent = skeleton.joint_matrix_global[parent_id].get_block_linear();
                angular_velocity = R_parent * angular_velocity;
            }
        }

        filtered_velocity[joint] = angular_velocity;
    }
}
