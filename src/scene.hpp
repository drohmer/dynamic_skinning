#pragma once

#include <chrono>

#include "cgp/cgp.hpp"
#include "environment.hpp"

#include "animated_character/animated_character.hpp"

#include "dynamic_skinning/dynamic_skinning.hpp"
#include "interaction/joint_picking.hpp"
#include "loaders/cylinder_loader.hpp"
#include "loaders/legacy_loader.hpp"


using cgp::mesh_drawable;


// Running average over a fixed window for timing measurements
struct timing_average {
	static constexpr int N = 60;
	float samples[N] = {};
	int index = 0;
	int count = 0;

	void push(float ms) {
		samples[index] = ms;
		index = (index + 1) % N;
		if (count < N) ++count;
	}
	float average() const {
		if (count == 0) return 0.0f;
		float sum = 0.0f;
		for (int i = 0; i < count; ++i) sum += samples[i];
		return sum / count;
	}
};


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
	// Timing
	// ****************************** //
	timing_average timing_animation;
	timing_average timing_lbs;
	timing_average timing_ds_phase1;
	timing_average timing_ds_phase2;
	timing_average timing_ds_normals;
	bool display_timings = false;

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
