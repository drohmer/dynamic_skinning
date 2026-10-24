#pragma once

#include "cgp/cgp.hpp"
#include "environment.hpp"
#include "dynamic_skinning/dynamic_skinning.hpp"
#include "animated_character/animated_model/animated_model.hpp"


struct gpu_skinning_state {
    cgp::opengl_shader_structure skinning_shader;

    // Data textures (created via raw OpenGL, stored in cgp texture structs)
    cgp::opengl_texture_image_structure skinning_matrices_tex;  // 2D, 4 x N_joints
    cgp::opengl_texture_image_structure joint_global_tex;       // 2D, 4 x N_joints
    cgp::opengl_texture_image_structure filtered_velocity_tex;  // 2D, 1 x N_joints (pre-filtered angular velocity)
    cgp::opengl_texture_image_structure hier_weights_tex;       // 2D, tiled (static)

    // CPU-side angular velocity tracking using the same tracker as CPU path
    cgp::numarray<angular_velocity_tracker> joint_trackers;
    cgp::numarray<cgp::vec3> filtered_velocity;  // pre-computed per joint

    int N_joints = 0;
    int N_vertex = 0;
    static constexpr int MAX_HIER_PER_VERTEX = 16;

    // Tiling for hier_weights_tex
    static constexpr int HIER_TILE_W = 1024;
    int hier_tex_rows = 0;

    bool initialized = false;
    float timing_upload_ms = 0.0f;
};


// Initialize GPU skinning: compile shaders, create textures, fill static data, set up VBOs.
void gpu_skinning_initialize(gpu_skinning_state& state,
                             animated_model_structure const& model,
                             std::string const& mesh_name,
                             dynamic_skinning_state const& ds_state,
                             cgp::mesh_drawable& drawable);

// Update per-frame GPU data: skinning matrices, joint globals, filtered velocity.
void gpu_skinning_update(gpu_skinning_state& state,
                         animated_model_structure const& model,
                         std::string const& mesh_name,
                         dynamic_skinning_parameters const& params,
                         float time);

// Draw with GPU skinning shader.
void gpu_skinning_draw(gpu_skinning_state& state,
                       cgp::mesh_drawable& drawable,
                       environment_structure const& env,
                       dynamic_skinning_parameters const& params,
                       bool display_bend_weights = false);
