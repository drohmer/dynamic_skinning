#pragma once

#include "cgp/cgp.hpp"
#include "dynamic_skinning.hpp"
#include "animated_character/animated_model/animated_model.hpp"


// Delayed Linear Blend Skinning (dLBS, paper Sec. "Time Delay Filter")
//
//   p_i^dLBS(t) = Σ_j α_ij T_j^d(t) p_i^0,   T_j^d(t) = Π_{k ∈ root..j} L_k(t - (d_i - d_k) / c)  B_j^-1
//
// d_i is the distance from the vertex to the root of the skeleton, measured along the skeleton
// (closest point on the bones, then cumulated bone lengths up to the root), d_k the same distance for
// the joint k, and c is the propagation speed. As in the paper, each local transformation L_k is delayed
// separately: the motion of a joint starts at this joint and propagates towards the extremities (e.g. the
// bumps of the engine of the train going to the last wagon, or a rotated neck of the giraffe going to the head).
//
// The past poses are stored as local joint matrices L_k (one per skeleton joint), sampled at a
// fixed time step in a ring buffer. The same history is used by the CPU path and uploaded for the GPU path.
struct delayed_lbs_state {
    static constexpr int N_history = 120;            // number of stored poses
    static constexpr float dt_history = 1.0f / 60;   // time step between two stored poses (2 s of history)

    // history[k] = local joint matrices L (per skeleton joint) of the k-th stored pose, ring buffer
    cgp::numarray<cgp::numarray<cgp::mat4>> history;
    int head = 0;                 // index of the most recent pose
    float last_record_time = 0;   // time of the most recent pose

    // Distance d_i along the skeleton, per vertex (computed in the initial pose)
    cgp::numarray<float> distance;
    // Distance d_k along the skeleton, per joint
    cgp::numarray<float> joint_distance;
    // Inverse bind matrices B_j^-1, per skeleton joint (identity for the joints that are not in the rig)
    cgp::numarray<cgp::mat4> inverse_bind;
};


// Skinning matrices T_j = G_j B_j^-1 indexed by skeleton joint (identity for the joints that are not in the rig)
cgp::numarray<cgp::mat4> skinning_matrices_per_skeleton_joint(animated_model_structure const& model,
                                                              std::string const& mesh_name);

// Compute the distances d_i, and fill the history with the current pose
void delayed_lbs_initialize(delayed_lbs_state& state, animated_model_structure const& model,
                            std::string const& mesh_name, float current_time);

// Store the current pose (once per elapsed time step)
void delayed_lbs_record(delayed_lbs_state& state, animated_model_structure const& model, float current_time);

// Number of time steps of delay per unit of distance (1 / (c dt))
float delayed_lbs_steps_per_distance(dynamic_skinning_parameters const& params);

// Delayed skinning matrix T_j^d of the skeleton joint j for a vertex at the distance d_i
cgp::mat4 delayed_lbs_skinning_matrix(delayed_lbs_state const& state, cgp::numarray<int> const& parent_index,
                                      int j, float distance, float steps_per_distance);

// CPU path: replace the LBS positions and normals of mesh_deformed by the delayed LBS
void delayed_lbs_apply(delayed_lbs_state const& state, dynamic_skinning_parameters const& params,
                       animated_model_structure& model, std::string const& mesh_name);
