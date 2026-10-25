#pragma once

#include <chrono>
#include "cgp/cgp.hpp"
#include "angular_velocity_tracker.hpp"
#include "animated_character/animated_model/animated_model.hpp"
#include "animated_character/skeleton_structure/skeleton_structure.hpp"
#include "animated_character/controller_skinning/controller_skinning.hpp"


// Parameters controlling the dynamic skinning effect
struct dynamic_skinning_parameters {
    float frequency       = 2.0f * 3.14159f * 1.5f;
    float amplitude       = 0.88f;  // amplitude A of the oscillation kernel κ (eq. 11)
    float drag            = 0.5f;   // weight D of the velocity (drag) term, independent of the oscillation amplitude
    float attenuation     = 1.5f;
    float frequency_slope = 0.0f;

    // Delayed LBS (dLBS): the skinning uses the pose of the skeleton at time t - d_i / c
    bool delay_active  = false;
    float delay_speed  = 2.0f;   // propagation speed c (model units per second, the model fits in [-1, 1])

    float bend_max_angle = 3.14159f;
    float bend_magnitude = 1.0f;

    bool active = true;
};


// State for dynamic skinning (one per animated character)
struct dynamic_skinning_state {
    // One angular velocity tracker per skeleton joint
    cgp::numarray<angular_velocity_tracker> joint_angular_velocity_tracker;

    // Per-vertex deformation accumulator
    cgp::numarray<cgp::vec3> deformation_per_vertex;

    // For each vertex, which joints influence it and with what hierarchical weight
    cgp::numarray<cgp::numarray<int>>   joints_per_vertex;       // joints_per_vertex[i] = list of joints j influencing vertex i
    cgp::numarray<cgp::numarray<float>> hierarchical_weights;     // hierarchical_weights[i][k] = β_ij for the k-th joint of vertex i
    cgp::numarray<cgp::numarray<int>>   center_joints_per_vertex; // center of the bend rotation for the k-th joint of vertex i (j itself, or a flexible origin)

    // Filtered angular velocity per joint (recomputed each frame, phase 1)
    cgp::numarray<cgp::vec3> filtered_angular_velocity;

    // Per-vertex bend weight (default 1.0)
    cgp::numarray<float> weight_bend;

    // Timing of last frame (in ms)
    float timing_phase1_ms = 0.0f;   // filtered angular velocity
    float timing_phase2_ms = 0.0f;   // gather deformation
    float timing_normals_ms = 0.0f;  // normal recomputation
};


// Initialize the dynamic skinning state from an animated model and a specific mesh name.
void dynamic_skinning_initialize(dynamic_skinning_state& state,
                                 animated_model_structure const& model,
                                 std::string const& mesh_name);

// Apply dynamic skinning deformations for the current frame (call AFTER skinning_lbs).
void dynamic_skinning_apply(dynamic_skinning_state& state,
                            dynamic_skinning_parameters const& params,
                            animated_model_structure& model,
                            std::string const& mesh_name,
                            float current_time);

// Display ImGui controls for dynamic skinning parameters.
void dynamic_skinning_display_gui(dynamic_skinning_parameters& params);
