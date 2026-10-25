#pragma once

#include "dynamic_skinning.hpp"

#include <cmath>


// ============================================================================
// Named constants for dynamic skinning
// ============================================================================

constexpr float DS_FRAME_DT           = 0.017f;   // Assumed frame delta time (~60 fps)
constexpr float DS_BEND_COUPLING      = 0.4f;     // Coupling coefficient (Appendix E §E.2)
constexpr int   DS_N_INTEGRAL         = 300;       // Number of convolution samples
constexpr float DS_AMPLITUDE_SCALING  = 10.0f;     // Empirical amplitude divisor (Appendix E §E.2)


// ============================================================================
// Oscillation kernel and derivative (eq. 11, 14-15)
// ============================================================================

// Oscillation kernel κ (eq. 11):
//   κ(t) = exp(-t/τ) sin(ω t)   with ω = params.frequency, 1/τ = params.attenuation
// The frequency_slope extension allows a time-varying frequency (Appendix C, κ₁).
inline float oscillating_kernel(int k, dynamic_skinning_parameters const& params)
{
    float t = k * DS_FRAME_DT;
    float df = t * params.frequency_slope;
    float phase = (params.frequency + df) * t;
    return std::exp(-params.attenuation * t) * std::sin(phase);
}

// Finite-difference kernel derivative: dκ/dt ≈ (κ(t) - κ(t-Δt)) / Δt
inline float kernel_derivative(int k, dynamic_skinning_parameters const& params)
{
    return (oscillating_kernel(k, params) - oscillating_kernel(k - 1, params)) / DS_FRAME_DT;
}


// ============================================================================
// Phase 1: Compute filtered angular velocity per joint (Algorithm 2)
// ============================================================================
//
// For each joint j:
//   1. Extract local rotation as quaternion, feed to angular velocity tracker
//   2. Convolve velocity history with oscillation kernel derivative (eq. 14-15)
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

        if (static_cast<int>(velocity_history.size()) > DS_N_INTEGRAL) {
            cgp::vec3 v_integral = {0, 0, 0};
            for (int k = 0; k < DS_N_INTEGRAL; ++k) {
                cgp::vec3 v = velocity_history[velocity_history.size() - 1 - k];
                v_integral += v * kernel_derivative(k, params);
            }
            // Oscillation term, weighted by A:  D * Omega + A * (Omega * kappa')
            float A = params.amplitude / DS_AMPLITUDE_SCALING;
            angular_velocity = angular_velocity + A * v_integral;
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
