#pragma once


#include "cgp/cgp.hpp"
#include "environment.hpp"

#include "animated_character/animated_character.hpp"

#include "dynamic_skinning/dynamic_skinning.hpp"
#include "interaction/joint_picking.hpp"
#include "loaders/cylinder_loader.hpp"
#include "loaders/legacy_loader.hpp"


using cgp::mesh_drawable;


struct gui_parameters {
	bool display_wireframe = false;
	bool display_surface = true;
	bool display_texture = true;

	bool display_skeleton_bones = true;
	bool display_skeleton_joints = true;
	bool display_skeleton_frames = false;
};


// The structure of the custom scene
struct scene_structure : cgp::scene_inputs_generic {

	// ****************************** //
	// Elements and shapes of the scene
	// ****************************** //
	camera_controller_orbit camera_control;
	camera_projection_perspective camera_projection;
	window_structure window;

	environment_structure environment;
	input_devices inputs;
	gui_parameters gui;

	// ****************************** //
	// Character
	// ****************************** //
	character_structure character;
	std::string current_model_name = "Cylinder";

	// ****************************** //
	// Dynamic Skinning & Joint Picking
	// ****************************** //
	dynamic_skinning_parameters ds_params;
	dynamic_skinning_state ds_state;
	joint_picking_state picking;
	cgp::numarray<cgp::mat4> interactive_deformation;
	mesh_drawable sphere_picking;

	cgp::timer_basic ds_timer;

	// ****************************** //
	// Functions
	// ****************************** //

	void initialize();
	void display_frame();
	void display_gui();

	void mouse_move_event();
	void mouse_click_event();
	void keyboard_event();
	void idle_frame();

	// Reload dynamic skinning state after model change
	void reinitialize_after_model_change();
	// Load a model by name
	void load_model(std::string const& name);

};
