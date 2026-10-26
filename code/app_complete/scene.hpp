#pragma once

#include <chrono>

#include "cgp/cgp.hpp"
#include "environment.hpp"

#include "animated_character/animated_character.hpp"

#include "dynamic_skinning_source/dynamic_skinning.hpp"
#include "interaction/joint_picking.hpp"
#include "interaction/touch_gestures.hpp"
#include "gpu_skinning/gpu_skinning.hpp"
#include "loaders/cylinder_loader.hpp"
#include "loaders/legacy_loader.hpp"
#include "loaders/cthulhu_loader.hpp"
#include "loaders/normalize_character.hpp"


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


enum class weight_display_mode { none, skinning, hierarchical };

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

	// Cthulhu-specific options
	int cthulhu_resolution_index = 0;  // index into cthulhu_resolutions[]
	bool cthulhu_use_bend_weights = true;  // floppy parts only (wing membranes, tentacles)
	bool display_bend_weights = true;  // color the floppy parts (white -> red)
	bool delay_enabled_once = false;     // the first activation of the delay turns off the dynamic skinning
	bool clown_flexible_origins = true;  // Clown: rigid body, arms and hat bent around their own origin

	// ****************************** //
	// Dynamic Skinning & Joint Picking
	// ****************************** //
	dynamic_skinning_parameters ds_params;
	dynamic_skinning_state ds_state;
	delayed_lbs_state dlbs_state;  // history of the poses for the delayed LBS
	joint_picking_state picking;
	cgp::numarray<cgp::mat4> interactive_deformation;
	cgp::numarray<cgp::mat4> skeleton_local_rest;  // local matrices restored each frame before animation
	// Animations and rest local matrices of the loaded model, before the joint edits (restored by reset_pose)
	std::map<std::string, skeleton_animation_structure> animation_initial;
	cgp::numarray<cgp::mat4> skeleton_local_rest_initial;
	float flap_frequency = 0.0f;  // (Hz) wing beat frequency of the current dragon "Flight" animation
	mesh_drawable sphere_picking;

	// Train: track (rails, bars, pillars) scrolling backward, so that the train seems to move forward
	struct track_part {
		mesh_drawable drawable;
		float period = 0;  // length of the repeated pattern (display units): the scrolling wraps around it
	};
	std::vector<track_part> train_track;
	float train_speed = 1.2f;  // scrolling speed of the track (display units per second)
	void initialize_train_track(normalization_transform const& normalization);
	void draw_train_track();
	bool camera_mode = false;  // false = bone-edit mode, true = camera mode
	touch_gesture_state touch;  // pinch zoom on touch screens (web version)
	// Touch screens: no mode to choose, a finger on a joint rotates it, elsewhere it orbits the camera
	bool touch_interaction = false;
	bool touch_orbit = false;  // the finger down started outside the joints: camera rotation
	weight_display_mode weight_display = weight_display_mode::none;

	// Cache for weight color visualization (avoid recomputing every frame)
	cgp::numarray<cgp::vec3> weight_colors_cache;
	int weight_colors_cached_joint = -2;  // -2 = invalid/uninitialized
	weight_display_mode weight_colors_cached_mode = weight_display_mode::none;

	cgp::timer_basic ds_timer;

	// ****************************** //
	// GPU Skinning
	// ****************************** //
	gpu_skinning_state gpu_state;
	bool use_gpu_skinning = true;
	// Default skinning path, also imposed when Cthulhu is loaded. False on touch screens (web version): on mobile
	// GPUs, the vertex shader is the bottleneck and the CPU path is faster (iPad Air M1, Cthulhu: 27 vs 18 fps)
	bool gpu_skinning_default = true;
	bool vbo_has_bind_pose = false;  // the VBO holds the bind pose expected by the GPU path (not a CPU-deformed mesh)

	// ****************************** //
	// Timing
	// ****************************** //
	timing_average timing_animation;
	timing_average timing_lbs;
	timing_average timing_ds_phase1;
	timing_average timing_ds_phase2;
	timing_average timing_ds_normals;
	timing_average timing_gpu_upload;
	bool display_timings = false;

	// ****************************** //
	// GUI
	// ****************************** //
	bool show_settings = false;      // advanced settings window (hidden by default)
	bool show_gui = true;            // ImGui windows (hidden on touch screens, which use the HTML panel of the web page)
	bool animation_running = false;  // state of the Play/Pause button

	// ****************************** //
	// Functions
	// ****************************** //

	void initialize();
	void display_frame();
	void display_gui();           // main toolbar (mode, model, play, dynamic skinning)
	void display_gui_settings();  // content of the advanced settings window

	void mouse_move_event();
	void mouse_click_event();
	void mouse_scroll_event();
	void keyboard_event();
	void idle_frame();

	// Reload dynamic skinning state after model change
	void reinitialize_after_model_change();
	// Dragon: rebuild the "Flight" animation when the frequency f of the oscillations has changed
	void update_flight_frequency();
	// True when the per-vertex bend weights are displayed as colors (Cthulhu only)
	bool show_bend_weights() const;
	// Zoom: multiply the camera distance to its center by factor
	void zoom_camera(float factor);
	void reset_pose();  // cancel the joint edits: back to the pose of the model when loaded, animation at t=0
	std::vector<std::string> model_names() const;  // models of the Model list, in display order
	// Load a model by name
	void load_model(std::string const& name);

};
