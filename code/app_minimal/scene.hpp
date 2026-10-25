#pragma once

#include "cgp/cgp.hpp"
#include "environment.hpp"
#include "animated_character/animated_character.hpp"
#include "dynamic_skinning_source/dynamic_skinning.hpp"


// Minimal scene: an animated character deformed by the dynamic skinning (CPU only)
//   p_i^DS = p_i^LBS + Σ_j β_ij ψ^bend(F^osc(Ω_j), p_i^LBS - q_j)
struct scene_structure : cgp::scene_inputs_generic {

	// ---- Camera, window and inputs ----
	camera_controller_orbit camera_control;
	camera_projection_perspective camera_projection;
	window_structure window;
	environment_structure environment;
	input_devices inputs;

	// ---- Character: skeleton, skinning weights, mesh and animation ----
	character_structure character;
	std::string model_name = "Cylinder";  // "Cylinder", "Girafe" or "Clown"
	std::string mesh_name;                // name of the rigged mesh of the character

	// ---- Parameters of the oscillation kernel κ(t) = A exp(-t/τ) sin(2π f t) ----
	float f   = 1.5f;   // frequency (Hz)
	float tau = 0.67f;  // damping τ (s)
	float A   = 0.88f;  // amplitude
	// ---- Weight D of the drag (lag proportional to the angular velocity, independent of the oscillations) ----
	float D   = 0.5f;
	bool dynamic_skinning_active = true;

	// ---- Dynamic skinning ----
	dynamic_skinning_parameters ds_parameters;  // f, τ, A, D converted to the parameters of the module
	dynamic_skinning_state ds_state;            // hierarchical weights β_ij, history of the angular velocities Ω_j
	cgp::timer_basic timer;                     // time t of the oscillations (not periodic, unlike the animation)

	void initialize();
	void load_model(std::string const& name);
	void display_frame();
	void display_gui();

	void mouse_move_event();
	void mouse_click_event();
	void mouse_scroll_event();
	void keyboard_event();
	void idle_frame();
};
