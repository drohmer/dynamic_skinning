#include "joint_picking.hpp"

using namespace cgp;


int pick_skeleton_joint(skeleton_structure const& skeleton,
                        vec2 const& screen_position,
                        camera_generic_base const& camera,
                        camera_projection_perspective const& projection,
                        float sphere_radius)
{
    int N = skeleton.size();
    numarray<vec3> joint_positions;
    joint_positions.resize(N);
    for (int k = 0; k < N; ++k) {
        joint_positions[k] = skeleton.joint_matrix_global[k].get_block_translation();
    }

    picking_structure result = picking_spheres(screen_position, joint_positions, sphere_radius, camera, projection);
    if (result.active)
        return result.index;
    return -1;
}


void joint_picking_mouse_click(joint_picking_state& picking,
                               skeleton_structure const& skeleton,
                               vec2 const& screen_position,
                               camera_generic_base const& camera,
                               camera_projection_perspective const& projection,
                               bool mouse_click_left,
                               bool mouse_click_right,
                               bool mouse_release)
{
    bool mouse_click = mouse_click_left || mouse_click_right;

    if (mouse_click_left)  picking.click_button = picking_click_button::left;
    if (mouse_click_right) picking.click_button = picking_click_button::right;
    if (mouse_release)     picking.click_button = picking_click_button::none;

    if (mouse_click) {
        int joint_selected = pick_skeleton_joint(skeleton, screen_position, camera, projection);
        if (joint_selected != -1) {
            vec3 const picked_position = skeleton.joint_matrix_global[joint_selected].get_block_translation();
            picking.is_selected = true;
            picking.selected_joint = joint_selected;
            picking.p_clicked = picked_position;
            picking.p_current = picked_position;
            picking.p_previous = picked_position;
            picking.accumulated_rotation = {0, 0, 0, 1};  // identity
            picking.accumulated_twist_angle = 0.0f;
            picking.selected_joint_true = picking.selected_joint;

            // For rotation mode: the actual deformed joint is the parent
            if (joint_selected != 0 && picking.deformation_is_rotation) {
                picking.selected_joint_true = skeleton.parent_index[joint_selected];
            }

            std::cout << "Picked joint: " << joint_selected
                      << " (" << skeleton.joint_name[joint_selected] << ")" << std::endl;
        }
    }

    if (mouse_release) {
        if (picking.is_selected) {
            picking.is_selected = false;
        }
    }
}


// Helper: project a 3D point to 2D screen coordinates using view+projection
static vec2 project_to_screen(vec3 const& p, mat4 const& view_matrix, camera_projection_perspective const& projection)
{
    mat4 P = projection.matrix();
    vec4 clip = P * view_matrix * vec4(p.x, p.y, p.z, 1.0f);
    if (std::abs(clip.w) < 1e-6f) return {0, 0};
    return {clip.x / clip.w, clip.y / clip.w};
}

void joint_picking_mouse_move(joint_picking_state& picking,
                              skeleton_structure const& skeleton,
                              numarray<mat4>& interactive_deformation,
                              vec2 const& screen_position,
                              camera_generic_base const& camera,
                              camera_projection_perspective const& projection,
                              mat4 const& view_matrix)
{
    if (!picking.is_selected)
        return;

    // Ray-plane intersection to track the mouse in 3D
    intersection_structure inter = picking_plane_orthogonal_to_camera(
        screen_position, picking.p_clicked, camera, projection);

    if (!inter.valid)
        return;

    picking.p_current = inter.position;

    int parent = skeleton.parent_index[picking.selected_joint];
    bool is_translation = (!picking.deformation_is_rotation || picking.selected_joint == 0);

    if (is_translation) {
        quaternion q_parent = {0,0,0,1};
        if (parent > -1) {
            mat3 R = skeleton.joint_matrix_global[parent].get_block_linear();
            q_parent = rotation_transform::from_matrix(R).get_quaternion();
        }

        vec3 const translation = picking.p_current - picking.p_clicked;
        vec3 local_translation = rotation_transform(conjugate(q_parent)) * translation;

        interactive_deformation[picking.selected_joint].set_block_translation(local_translation);
    }
    else {
        if (parent > -1) {
            vec3 p_parent = skeleton.joint_matrix_global[parent].get_block_translation();

            int grand_parent = skeleton.parent_index[parent];
            quaternion q_grand_parent = {0,0,0,1};
            if (grand_parent > -1) {
                mat3 R_gp = skeleton.joint_matrix_global[grand_parent].get_block_linear();
                q_grand_parent = rotation_transform::from_matrix(R_gp).get_quaternion();
            }

            if (picking.click_button == picking_click_button::left) {
                // Incremental rotation: small delta from p_previous to p_current
                vec3 const u_prev = picking.p_previous - p_parent;
                vec3 const u_curr = picking.p_current - p_parent;
                float n_prev = norm(u_prev);
                float n_curr = norm(u_curr);

                if (n_prev > 1e-6f && n_curr > 1e-6f) {
                    vec3 const d_prev = u_prev / n_prev;
                    vec3 const d_curr = u_curr / n_curr;

                    float d = dot(d_prev, d_curr);
                    d = std::max(-1.0f, std::min(1.0f, d));
                    float const angle = std::acos(d);

                    if (angle > 1e-6f) {
                        vec3 axis = cross(d_prev, d_curr);
                        float axis_norm = norm(axis);
                        if (axis_norm > 1e-6f) {
                            axis = axis / axis_norm;
                            quaternion delta_q = rotation_transform::from_axis_angle(axis, angle).get_quaternion();
                            picking.accumulated_rotation = delta_q * picking.accumulated_rotation;
                        }
                    }
                }
            }
            else if (picking.click_button == picking_click_button::right) {
                // Incremental twist: delta angle between p_previous and p_current in screen space
                vec3 const bone_axis = normalize(picking.p_clicked - p_parent);

                vec2 const p0_screen = project_to_screen(p_parent, view_matrix, projection);
                vec2 const p1_screen = project_to_screen(picking.p_clicked, view_matrix, projection);
                vec2 const p_prev_screen = project_to_screen(picking.p_previous, view_matrix, projection);
                vec2 const p_curr_screen = project_to_screen(picking.p_current, view_matrix, projection);

                vec2 const a = normalize(p1_screen - p0_screen);
                vec2 const b_prev = p_prev_screen - p0_screen;
                vec2 const b_curr = p_curr_screen - p0_screen;

                float angle_prev = a.x * b_prev.y - a.y * b_prev.x;
                float angle_curr = a.x * b_curr.y - a.y * b_curr.x;
                float delta_angle = -2.0f * 3.14159f * (angle_curr - angle_prev);

                picking.accumulated_twist_angle += delta_angle;
                picking.accumulated_rotation = rotation_transform::from_axis_angle(bone_axis, picking.accumulated_twist_angle).get_quaternion();
            }

            // Apply accumulated rotation in grand-parent frame
            mat3 R_rot = rotation_transform(conjugate(q_grand_parent) * picking.accumulated_rotation * q_grand_parent).matrix();
            interactive_deformation[parent].set_block_linear(R_rot);

            picking.p_previous = picking.p_current;
        }
    }
}


void joint_picking_apply_deformation(joint_picking_state const& picking,
                                     skeleton_structure const& skeleton,
                                     numarray<mat4>& interactive_deformation,
                                     std::map<std::string, skeleton_animation_structure>& animations,
                                     numarray<mat4>& skeleton_local_rest)
{
    int parent = skeleton.parent_index[picking.selected_joint];

    vec3 translation = interactive_deformation[picking.selected_joint].get_block_translation();
    mat3 rotation_mat = mat3::build_identity();
    if (parent > -1) {
        rotation_mat = interactive_deformation[parent].get_block_linear();
    }

    // Apply to the rest local matrices (overwritten by the animation for animated joints)
    skeleton_local_rest[picking.selected_joint].apply_translation(translation);
    if (parent > -1) {
        mat3 current_linear = skeleton_local_rest[parent].get_block_linear();
        skeleton_local_rest[parent].set_block_linear(rotation_mat * current_linear);
    }

    // Apply to all animation keyframes
    for (auto& anim_entry : animations) {
        auto& anim = anim_entry.second;

        for (int k_channel = 0; k_channel < anim.joint_index.size(); ++k_channel) {
            int anim_joint = anim.joint_index[k_channel];

            if (anim_joint == picking.selected_joint) {
                int N_time = anim.matrix[k_channel].size();
                for (int kt = 0; kt < N_time; ++kt) {
                    anim.matrix[k_channel][kt].apply_translation(translation);
                }
            }

            if (parent > -1 && anim_joint == parent) {
                int N_time = anim.matrix[k_channel].size();
                for (int kt = 0; kt < N_time; ++kt) {
                    mat3 current_linear = anim.matrix[k_channel][kt].get_block_linear();
                    anim.matrix[k_channel][kt].set_block_linear(rotation_mat * current_linear);
                }
            }
        }
    }

    // Reset all interactive deformations to identity
    for (int k = 0; k < interactive_deformation.size(); ++k) {
        interactive_deformation[k] = mat4::build_identity();
    }
}
