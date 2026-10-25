#pragma once

#include "cgp/cgp.hpp"
#include "animated_character/skeleton_structure/skeleton_structure.hpp"
#include "animated_character/skeleton_animation/skeleton_animation.hpp"
#include "animated_character/animated_model/animated_model.hpp"


enum class picking_click_button { none, left, right };

// State for interactive joint picking and deformation
struct joint_picking_state {
    bool is_selected = false;
    int selected_joint = -1;
    int selected_joint_true = -1;   // Actual joint being deformed (parent for rotation)
    int joint_hover = -1;

    cgp::vec3 p_clicked;
    cgp::vec3 p_current;
    cgp::vec3 p_previous;                    // previous frame 3D position (for incremental rotation)
    cgp::quaternion accumulated_rotation;    // accumulated rotation quaternion (left-click)
    float accumulated_twist_angle = 0.0f;   // accumulated twist angle (right-click)

    picking_click_button click_button = picking_click_button::none;

    bool deformation_is_rotation = true;
};


// Pick a skeleton joint under the mouse cursor. Returns joint index or -1.
int pick_skeleton_joint(skeleton_structure const& skeleton,
                        cgp::vec2 const& screen_position,
                        cgp::camera_generic_base const& camera,
                        cgp::camera_projection_perspective const& projection,
                        float sphere_radius = 0.08f);

// Handle mouse click for joint picking (Shift must be held by caller).
void joint_picking_mouse_click(joint_picking_state& picking,
                               skeleton_structure const& skeleton,
                               cgp::vec2 const& screen_position,
                               cgp::camera_generic_base const& camera,
                               cgp::camera_projection_perspective const& projection,
                               bool mouse_click_left,
                               bool mouse_click_right,
                               bool mouse_release);

// Handle mouse move during joint drag.
void joint_picking_mouse_move(joint_picking_state& picking,
                              skeleton_structure const& skeleton,
                              cgp::numarray<cgp::mat4>& interactive_deformation,
                              cgp::vec2 const& screen_position,
                              cgp::camera_generic_base const& camera,
                              cgp::camera_projection_perspective const& projection,
                              cgp::mat4 const& view_matrix);

// Apply the interactive deformation to all animation keyframes and to the rest
// local matrices (for joints without animation channel), then reset.
void joint_picking_apply_deformation(joint_picking_state const& picking,
                                     skeleton_structure const& skeleton,
                                     cgp::numarray<cgp::mat4>& interactive_deformation,
                                     std::map<std::string, skeleton_animation_structure>& animations,
                                     cgp::numarray<cgp::mat4>& skeleton_local_rest);
