#include "cylinder_loader.hpp"

using namespace cgp;


// Generate grid connectivity for a Nu x Nv grid, with optional wrap in v direction
static numarray<uint3> connectivity_grid_cylinder(int Nu, int Nv, bool wrap_v)
{
    numarray<uint3> connectivity;
    for (int ku = 0; ku < Nu - 1; ++ku) {
        for (int kv = 0; kv < Nv - 1; ++kv) {
            unsigned int idx = static_cast<unsigned int>(ku * Nv + kv);
            connectivity.push_back(uint3{idx, idx + 1, idx + Nv + 1});
            connectivity.push_back(uint3{idx, idx + Nv + 1, idx + Nv});
        }
        // Wrap last column to first if cylinder
        if (wrap_v) {
            unsigned int idx = static_cast<unsigned int>(ku * Nv + (Nv - 1));
            unsigned int idx_next_row = static_cast<unsigned int>((ku + 1) * Nv);
            unsigned int idx_wrap = static_cast<unsigned int>(ku * Nv);
            connectivity.push_back(uint3{idx, idx_wrap, idx_next_row});
            connectivity.push_back(uint3{idx, idx_next_row, idx + Nv});
        }
    }
    return connectivity;
}


character_structure load_cylinder_bending()
{
    character_structure character;
    animated_model_structure& model = character.animated_model;

    // =============================================
    // 1. Setup skeleton hierarchy (3 joints)
    // =============================================
    int N_joints = 3;
    model.skeleton.joint_matrix_local.resize(N_joints);
    model.skeleton.joint_matrix_global.resize(N_joints);
    model.skeleton.joint_name.resize(N_joints);
    model.skeleton.parent_index.resize(N_joints);

    model.skeleton.joint_name[0] = "joint_0";
    model.skeleton.joint_name[1] = "joint_1";
    model.skeleton.joint_name[2] = "joint_2";

    model.skeleton.parent_index[0] = -1; // root
    model.skeleton.parent_index[1] = 0;
    model.skeleton.parent_index[2] = 1;

    // Rest pose: joints along x-axis
    // joint_0 at origin, joint_1 at x=0.5, joint_2 at x=1.0 (relative to joint_1: x=0.5)
    model.skeleton.joint_matrix_local[0] = mat4::build_identity(); // root at origin
    model.skeleton.joint_matrix_local[1] = mat4::build_translation(0.5f, 0.0f, 0.0f);
    model.skeleton.joint_matrix_local[2] = mat4::build_translation(0.5f, 0.0f, 0.0f);

    model.skeleton.update_joint_matrix_local_to_global();

    // =============================================
    // 2. Setup animation (bending/twisting)
    // =============================================
    skeleton_animation_structure anim;
    anim.joint_index.resize(N_joints);
    anim.times.resize(N_joints);
    anim.matrix.resize(N_joints);

    // All 3 joints are animated
    for (int k = 0; k < N_joints; ++k)
        anim.joint_index[k] = k;

    // Keyframe times: 7 frames (0 to 6 seconds)
    int N_frames = 7;
    float t_max = 6.0f;

    // Joint 0: identity at all times
    anim.times[0].resize(N_frames);
    anim.matrix[0].resize(N_frames);
    for (int k = 0; k < N_frames; ++k) {
        anim.times[0][k] = static_cast<float>(k);
        anim.matrix[0][k] = mat4::build_identity();
    }

    // Joint 1: bending and twisting animation
    // Frame 0: straight
    // Frame 1: bend pi/2 around z
    // Frame 2: straight
    // Frame 3: bend pi/2 around x
    // Frame 4: straight
    // Frame 5: bend -pi/2 around x
    // Frame 6: straight
    anim.times[1].resize(N_frames);
    anim.matrix[1].resize(N_frames);
    for (int k = 0; k < N_frames; ++k)
        anim.times[1][k] = static_cast<float>(k);

    mat4 M_local_joint1 = mat4::build_translation(0.5f, 0.0f, 0.0f);

    anim.matrix[1][0] = M_local_joint1; // straight
    {
        mat4 M = mat4::build_rotation_from_axis_angle({0, 0, 1}, 3.14159f / 2.0f);
        M.apply_translation({0.5f, 0.0f, 0.0f});
        anim.matrix[1][1] = M; // bend z
    }
    anim.matrix[1][2] = M_local_joint1; // straight
    {
        mat4 M = mat4::build_rotation_from_axis_angle({1, 0, 0}, 3.14159f / 2.0f);
        M.apply_translation({0.5f, 0.0f, 0.0f});
        anim.matrix[1][3] = M; // bend x
    }
    anim.matrix[1][4] = M_local_joint1; // straight
    {
        mat4 M = mat4::build_rotation_from_axis_angle({1, 0, 0}, -3.14159f / 2.0f);
        M.apply_translation({0.5f, 0.0f, 0.0f});
        anim.matrix[1][5] = M; // bend -x
    }
    anim.matrix[1][6] = M_local_joint1; // straight

    // Joint 2: always identity relative to parent (just for display)
    anim.times[2].resize(N_frames);
    anim.matrix[2].resize(N_frames);
    for (int k = 0; k < N_frames; ++k) {
        anim.times[2][k] = static_cast<float>(k);
        anim.matrix[2][k] = mat4::build_translation(0.5f, 0.0f, 0.0f);
    }

    anim.time_max = t_max;
    model.animation["Bending"] = anim;

    // =============================================
    // 3. Generate cylinder mesh
    // =============================================
    int const N = 50;
    float const r = 0.1f;

    mesh cylinder;
    for (int ku = 0; ku < N; ++ku) {
        for (int kv = 0; kv < N; ++kv) {
            float u = ku / float(N - 1);
            float v = kv / float(N);
            float theta = 2.0f * 3.14159f * v;

            vec3 p = {u, r * std::cos(theta), r * std::sin(theta)};
            vec3 n = {0, std::cos(theta), std::sin(theta)};

            cylinder.position.push_back(p);
            cylinder.normal.push_back(n);
        }
    }
    cylinder.connectivity = connectivity_grid_cylinder(N, N, true);
    cylinder.fill_empty_field();

    // =============================================
    // 4. Setup skinning weights
    // =============================================
    // Rig uses 2 joints (joint_0 and joint_1)
    int N_rig_joints = 2;
    int N_vertex = cylinder.position.size();

    controller_skinning_structure controller;
    controller.vertex_to_joint_dependence.resize(N_vertex);
    controller.rig_index_to_skeleton_index.resize(N_rig_joints);
    controller.rig_index_to_skeleton_index[0] = 0; // rig joint 0 = skeleton joint 0
    controller.rig_index_to_skeleton_index[1] = 1; // rig joint 1 = skeleton joint 1

    // Inverse bind matrices: the skeleton rest pose joints
    // Joint 0 at origin -> inverse is identity
    // Joint 1 at (0.5, 0, 0) -> inverse translates by (-0.5, 0, 0)
    controller.inverse_bind_matrices.resize(N_rig_joints);
    controller.inverse_bind_matrices[0] = mat4::build_identity();
    controller.inverse_bind_matrices[1] = mat4::build_translation(-0.5f, 0.0f, 0.0f);

    controller.global_bind_matrix = mat4::build_identity();

    for (int ku = 0; ku < N; ++ku) {
        float u = ku / float(N - 1);
        for (int kv = 0; kv < N; ++kv) {
            float w0, w1;
            float const alpha = 3.0f;
            if (u < 0.5f) {
                w1 = 0.5f * std::pow(u / 0.5f, alpha);
                w0 = 1.0f - w1;
            } else {
                w0 = 0.5f * std::pow(1.0f - (u - 0.5f) / 0.5f, alpha);
                w1 = 1.0f - w0;
            }

            numarray<skinning_weight_info> deps;
            deps.push_back({0, w0});
            deps.push_back({1, w1});
            controller.vertex_to_joint_dependence[ku * N + kv] = deps;
        }
    }

    // =============================================
    // 5. Assemble rigged mesh
    // =============================================
    rigged_mesh_structure rigged;
    rigged.mesh_bind_pose = cylinder;
    rigged.mesh_deformed = cylinder;
    rigged.controller_skinning = controller;

    model.rigged_mesh["Cylinder"] = rigged;

    // =============================================
    // 6. Initialize character
    // =============================================
    character.current_animation_name = "Bending";
    character.timer.event_period = t_max;

    // Initialize drawable
    mesh_drawable drawable;
    drawable.initialize_data_on_gpu(cylinder);
    drawable.material.color = {0.8f, 0.8f, 0.9f};
    character.drawable["Cylinder"] = drawable;

    // Initialize skeleton drawable (must use constructor for GPU init)
    character.sk_drawable = skeleton_drawable(model.skeleton);

    return character;
}
