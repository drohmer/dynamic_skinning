#include "scene.hpp"
#include "loaders/normalize_character.hpp"
#include <chrono>

using namespace cgp;


// Compute per-vertex weight colors for the given joint (white=0, red=1)
static void compute_weight_colors(
	numarray<vec3>& colors,
	int joint_skeleton_index,
	weight_display_mode mode,
	rigged_mesh_structure const& rigged_mesh,
	dynamic_skinning_state const& ds_state)
{
	int N = rigged_mesh.mesh_deformed.position.size();
	colors.resize(N);

	if (joint_skeleton_index < 0) {
		for (int i = 0; i < N; ++i)
			colors[i] = {1.0f, 1.0f, 1.0f};
		return;
	}

	auto const& rig_to_skel = rigged_mesh.controller_skinning.rig_index_to_skeleton_index;

	for (int i = 0; i < N; ++i) {
		float w = 0.0f;

		if (mode == weight_display_mode::skinning) {
			for (auto const& dep : rigged_mesh.controller_skinning.vertex_to_joint_dependence[i]) {
				int skel_idx = rig_to_skel[dep.joint_index];
				if (skel_idx == joint_skeleton_index) {
					w = dep.weight;
					break;
				}
			}
		}
		else if (mode == weight_display_mode::hierarchical) {
			auto const& joints = ds_state.joints_per_vertex[i];
			auto const& weights = ds_state.hierarchical_weights[i];
			for (int k = 0; k < joints.size(); ++k) {
				if (joints[k] == joint_skeleton_index) {
					w = weights[k];
					break;
				}
			}
		}

		colors[i] = {1.0f, 1.0f - w, 1.0f - w};
	}
}


void scene_structure::load_model(std::string const& name)
{
	current_model_name = name;

	if (name == "Cylinder") {
		character = load_cylinder_bending();
	}
	else if (name == "Lola") {
		filename_loader_structure loader_param;
		loader_param.set_skeleton(project::path+"assets/lola/skeleton/");
		loader_param.add_rigged_mesh("body", project::path+"assets/lola/mesh-lola/", project::path+"assets/lola/mesh-lola/texture.jpg");
		loader_param.add_animation("Dance1", project::path+"assets/lola/animation/dance_1/");
		loader_param.add_animation("Dance2", project::path+"assets/lola/animation/dance_2/");
		loader_param.add_animation("Dance3", project::path+"assets/lola/animation/dance_3/");
		loader_param.add_animation("Dance4", project::path+"assets/lola/animation/dance_4/");
		loader_param.add_animation("Idle", project::path+"assets/lola/animation/idle/");
		loader_param.add_animation("Walk", project::path+"assets/lola/animation/walk/");
		loader_param.add_animation("Walk2", project::path+"assets/lola/animation/walk_style/");
		character = character_structure();
		character.load_and_initialize(loader_param, affine_rts().set_scaling(0.01f));
	}
	else if (name == "Cthulhu") {
		std::string resolution = cthulhu_resolutions[cthulhu_resolution_index];
		character = load_cthulhu_character(project::path + "assets/cthulhu/",
		                                  resolution, cthulhu_use_bend_weights);
	}
	else {
		auto models = get_legacy_models();
		for (auto const& m : models) {
			if (m.name == name) {
				character = load_legacy_character(m.directory, m.scaling, m.invert_normals);
				break;
			}
		}
	}

	// Normalize all models to [-1, 1] range
	normalize_character(character);

	// Camera: hard-coded presets per model
	{
		struct { const char* name; vec3 eye; vec3 center; float dist; } presets[] = {
			{"Cylinder", {1.674f, 0.242f, 3.456f},  {-0.486f, 0.183f, 0.204f},  3.905f},
			{"Lola",     {0.725f, 0.818f, 3.400f},  {-0.399f, 0.197f, 0.167f},  3.479f},
			{"Cthulhu",  {0.707f, -2.442f, 0.169f}, {-0.400f, -0.069f, -0.089f}, 2.631f},
			{"Clown",    {0.475f, 0.505f, 2.761f},  {-0.362f, 0.084f, 0.037f},  2.881f},
			{"Dragon",   {-2.919f, 0.601f, 0.637f}, {-0.048f, 0.009f, -0.582f}, 3.174f},
			{"Girafe",   {2.318f, 0.671f, 1.742f},  {-0.742f, -0.059f, 0.005f}, 3.593f},
			{"Snail",    {1.532f, -2.366f, 0.286f}, {-0.473f, -0.125f, -0.164f}, 3.040f},
			{"Spot",     {-3.082f, 2.140f, 0.806f}, {0.409f, 0.494f, 0.209f},   3.905f},
		};
		bool found = false;
		for (auto& d : presets) {
			if (name == d.name) {
				camera_control.look_at(d.eye, d.center, {0, 1, 0});
				camera_control.camera_model.distance_to_center = d.dist;
				found = true;
				break;
			}
		}
		if (!found)
			camera_control.look_at({ 3.0f, 1.5f, 2.0f }, {0, 0, 0}, {0, 1, 0});
	}

	// Auto-enable GPU skinning for Cthulhu
	use_gpu_skinning = (name == "Cthulhu");

	reinitialize_after_model_change();
}


void scene_structure::reinitialize_after_model_change()
{
	// Initialize dynamic skinning on the first mesh
	std::string first_mesh = character.animated_model.rigged_mesh.begin()->first;
	dynamic_skinning_initialize(ds_state, character.animated_model, first_mesh);

	// Initialize interactive deformation (identity per joint)
	int N_joints = character.animated_model.skeleton.size();
	interactive_deformation.resize(N_joints);
	for (int k = 0; k < N_joints; ++k)
		interactive_deformation[k] = mat4::build_identity();

	// Rest local matrices (used for joints that have no animation channel)
	skeleton_local_rest = character.animated_model.skeleton.joint_matrix_local;

	// Reset picking state and weight color cache
	picking = joint_picking_state();
	weight_colors_cached_joint = -2;

	// Initialize GPU skinning
	std::string first_mesh_gpu = character.animated_model.rigged_mesh.begin()->first;
	gpu_skinning_initialize(gpu_state, character.animated_model, first_mesh_gpu,
	                        ds_state, character.drawable[first_mesh_gpu]);

	character.timer.start();
	character.timer.stop();
}


void scene_structure::initialize()
{
	camera_control.initialize(inputs, window);
	camera_control.look_at({ 1.0f, 0.5f, 0.5f }, {0.5f, 0, 0}, {0, 1, 0});

	// Picking feedback sphere (green)
	sphere_picking.initialize_data_on_gpu(mesh_primitive_sphere());
	sphere_picking.model.scaling = 0.03f;
	sphere_picking.material.color = {0, 1, 0};

	load_model("Cylinder");
	ds_timer.start();
}


void scene_structure::display_frame()
{
	// Camera & environment
	camera_projection.aspect_ratio = window.aspect_ratio();
	environment.camera_projection = camera_projection.matrix();
	environment.camera_view = camera_control.camera_model.matrix_view();
	environment.light = camera_control.camera_model.position();

	// Update timers
	character.timer.update();
	ds_timer.update();

	using clock = std::chrono::high_resolution_clock;
	auto to_ms = [](auto d) { return std::chrono::duration<float, std::milli>(d).count(); };

	// ********************************** //
	// Set skeleton from animation
	// ********************************** //
	auto ta0 = clock::now();
	// Reset local matrices so joints without animation channel don't accumulate
	// the interactive deformation from one frame to the next
	character.animated_model.skeleton.joint_matrix_local = skeleton_local_rest;
	character.animated_model.set_skeleton_from_animation(
		character.current_animation_name, character.timer.t_periodic);

	// Apply interactive deformation
	{
		auto& skeleton = character.animated_model.skeleton;
		int N_joints = skeleton.size();
		for (int k = 0; k < N_joints; ++k) {
			vec3 t = interactive_deformation[k].get_block_translation();
			mat3 R = interactive_deformation[k].get_block_linear();

			if (norm(t) > 1e-6f)
				skeleton.joint_matrix_local[k].apply_translation(t);

			mat3 Id = mat3::build_identity();
			mat3 diff = R - Id;
			if (norm(vec3(diff(0,0), diff(1,1), diff(2,2))) > 1e-6f) {
				mat3 current = skeleton.joint_matrix_local[k].get_block_linear();
				skeleton.joint_matrix_local[k].set_block_linear(R * current);
			}
		}
		skeleton.update_joint_matrix_local_to_global();
	}
	auto ta1 = clock::now();
	timing_animation.push(to_ms(ta1 - ta0));

	if (use_gpu_skinning) {
		// ********************************** //
		// GPU Skinning path
		// ********************************** //
		std::string first_mesh = character.animated_model.rigged_mesh.begin()->first;
		auto& rigged_mesh = character.animated_model.rigged_mesh[first_mesh];
		mesh_drawable& drawable = character.drawable[first_mesh];

		// Ensure VBO contains bind-pose data (GPU shader applies LBS itself)
		drawable.vbo_position.update(rigged_mesh.mesh_bind_pose.position);
		drawable.vbo_normal.update(rigged_mesh.mesh_bind_pose.normal);

		gpu_skinning_update(gpu_state, character.animated_model, first_mesh, ds_params, ds_timer.t);
		timing_gpu_upload.push(gpu_state.timing_upload_ms);

		// Weight visualization (skinning or hierarchical) on GPU path
		if (weight_display != weight_display_mode::none) {
			int raw_joint = picking.is_selected ? picking.selected_joint : picking.joint_hover;
			int active_joint = raw_joint;
			if (raw_joint > 0 && picking.deformation_is_rotation)
				active_joint = character.animated_model.skeleton.parent_index[raw_joint];
			if (active_joint != weight_colors_cached_joint || weight_display != weight_colors_cached_mode) {
				compute_weight_colors(weight_colors_cache, active_joint, weight_display, rigged_mesh, ds_state);
				weight_colors_cached_joint = active_joint;
				weight_colors_cached_mode = weight_display;
			}
			drawable.vbo_color.update(weight_colors_cache);
		}

		if (gui.display_surface) {
			bool show_texture = gui.display_texture && (weight_display == weight_display_mode::none);
			drawable.material.texture_settings.active = show_texture;
			gpu_skinning_draw(gpu_state, drawable, environment, ds_params, display_bend_weights);
		}
	}
	else {
		// ********************************** //
		// CPU Skinning path (LBS)
		// ********************************** //
		auto tl0 = clock::now();
		for (auto& rigged_mesh_entry : character.animated_model.rigged_mesh) {
			character.animated_model.skinning_lbs(rigged_mesh_entry.first);
		}
		auto tl1 = clock::now();
		timing_lbs.push(to_ms(tl1 - tl0));

		// ********************************** //
		// Dynamic skinning (after LBS)
		// ********************************** //
		if (ds_params.active) {
			std::string first_mesh = character.animated_model.rigged_mesh.begin()->first;
			dynamic_skinning_apply(ds_state, ds_params, character.animated_model, first_mesh, ds_timer.t);
			timing_ds_phase1.push(ds_state.timing_phase1_ms);
			timing_ds_phase2.push(ds_state.timing_phase2_ms);
			timing_ds_normals.push(ds_state.timing_normals_ms);
		}

		// ********************************** //
		// Display (CPU path)
		// ********************************** //
		for (auto& rigged_mesh_entry : character.animated_model.rigged_mesh) {
			std::string mesh_name = rigged_mesh_entry.first;
			rigged_mesh_structure& rigged_mesh = rigged_mesh_entry.second;

			mesh_drawable& drawable = character.drawable[mesh_name];
			drawable.vbo_position.update(rigged_mesh.mesh_deformed.position);
			drawable.vbo_normal.update(rigged_mesh.mesh_deformed.normal);

			// Encode bend weights as color: white (w=0) → red (w=1)
			if (display_bend_weights && ds_state.weight_bend.size() > 0) {
				int N = rigged_mesh.mesh_deformed.position.size();
				numarray<vec3> colors(N);
				for (int i = 0; i < N; ++i) {
					float w = ds_state.weight_bend[i];
					colors[i] = {1.0f, 1.0f - w, 1.0f - w};  // white→red
				}
				drawable.vbo_color.update(colors);
			}

			// Weight visualization (skinning or hierarchical)
			if (weight_display != weight_display_mode::none) {
				int raw_joint = picking.is_selected ? picking.selected_joint : picking.joint_hover;
				int active_joint = raw_joint;
				if (raw_joint > 0 && picking.deformation_is_rotation)
					active_joint = character.animated_model.skeleton.parent_index[raw_joint];
				if (active_joint != weight_colors_cached_joint || weight_display != weight_colors_cached_mode) {
					compute_weight_colors(weight_colors_cache, active_joint, weight_display, rigged_mesh, ds_state);
					weight_colors_cached_joint = active_joint;
					weight_colors_cached_mode = weight_display;
				}
				drawable.vbo_color.update(weight_colors_cache);
			}

			if (gui.display_surface) {
				bool show_texture = gui.display_texture && (weight_display == weight_display_mode::none);
				drawable.material.texture_settings.active = show_texture;
				draw(drawable, environment);
			}
			if (gui.display_wireframe) {
				draw_wireframe(drawable, environment);
			}
		}
	}

	// Clear depth buffer so skeleton and picking sphere render on top of mesh
	glClear(GL_DEPTH_BUFFER_BIT);

	// Display skeleton
	if (gui.display_skeleton_bones || gui.display_skeleton_joints || gui.display_skeleton_frames) {
		character.sk_drawable.update(character.animated_model.skeleton);
		character.sk_drawable.display_joint_frame = gui.display_skeleton_frames;
		character.sk_drawable.display_joint_sphere = gui.display_skeleton_joints;
		character.sk_drawable.display_segments = gui.display_skeleton_bones;
		draw(character.sk_drawable, environment);
	}

	// Picking hover/selection feedback
	{
		int feedback_joint = picking.is_selected ? picking.selected_joint : picking.joint_hover;
		if (feedback_joint >= 0) {
			auto const& skeleton = character.animated_model.skeleton;

			// Green sphere on the clicked/hovered joint
			vec3 joint_pos = skeleton.joint_matrix_global[feedback_joint].get_block_translation();
			sphere_picking.material.color = {0, 1, 0};
			sphere_picking.model.translation = joint_pos;
			draw(sphere_picking, environment);

			// Orange sphere on the parent joint (the one actually deformed)
			if (feedback_joint > 0 && picking.deformation_is_rotation) {
				int parent = skeleton.parent_index[feedback_joint];
				if (parent >= 0) {
					vec3 parent_pos = skeleton.joint_matrix_global[parent].get_block_translation();
					sphere_picking.material.color = {1.0f, 0.6f, 0.0f};
					sphere_picking.model.translation = parent_pos;
					draw(sphere_picking, environment);
				}
			}
		}
	}
}


void scene_structure::mouse_move_event()
{
	vec2 const& p0 = inputs.mouse.position.previous;
	vec2 const& p1 = inputs.mouse.position.current;
	bool const on_gui = inputs.mouse.on_gui;

	if (camera_mode) {
		// Mode caméra : comportement standard du framework
		camera_control.action_mouse_move();
		picking.joint_hover = -1;
		return;
	}

	// Mode bone-edit :
	// Shift + clic gauche → rotation caméra (arcball)
	if (inputs.mouse.click.left && inputs.keyboard.shift && !on_gui) {
		picking.joint_hover = -1;
		camera_control.camera_model.manipulator_rotate_arcball(p0, p1);
	}
	// Shift + clic droit → zoom
	else if (inputs.mouse.click.right && inputs.keyboard.shift && !on_gui) {
		picking.joint_hover = -1;
		camera_control.camera_model.manipulator_scale_distance_to_center((p1 - p0).y);
	}
	else {
		// Clic gauche → rotation du joint, clic droit → twist du joint
		auto& skeleton = character.animated_model.skeleton;

		if (picking.is_selected) {
			joint_picking_mouse_move(picking, skeleton, interactive_deformation,
				inputs.mouse.position.current,
				camera_control.camera_model, camera_projection,
				camera_control.camera_model.matrix_view());
		}
		else if (on_gui) {
			picking.joint_hover = -1;
		}
		else {
			picking.joint_hover = pick_skeleton_joint(skeleton,
				inputs.mouse.position.current,
				camera_control.camera_model, camera_projection);
		}
	}
}

void scene_structure::mouse_click_event()
{
	if (camera_mode || inputs.keyboard.shift) {
		// Mode caméra ou Shift → pas de picking
		return;
	}

	// Mode bone-edit : clic gauche = rotation, clic droit = twist
	auto& skeleton = character.animated_model.skeleton;

	bool click_left  = (inputs.mouse.click.last_action == last_mouse_cursor_action::click_left);
	bool click_right = (inputs.mouse.click.last_action == last_mouse_cursor_action::click_right);
	bool release     = (inputs.mouse.click.last_action == last_mouse_cursor_action::release_left) ||
	                   (inputs.mouse.click.last_action == last_mouse_cursor_action::release_right);

	// Clic sur la GUI → pas de picking (le relâchement reste traité pour terminer un drag)
	if (inputs.mouse.on_gui && (click_left || click_right))
		return;

	if (release && picking.is_selected) {
		joint_picking_apply_deformation(picking, skeleton, interactive_deformation,
			character.animated_model.animation, skeleton_local_rest);
	}

	joint_picking_mouse_click(picking, skeleton,
		inputs.mouse.position.current,
		camera_control.camera_model, camera_projection,
		click_left, click_right, release);
}

void scene_structure::keyboard_event()
{
	camera_control.action_keyboard();

	// Tab → toggle bone-edit / camera mode (only on key press, not release)
	if (inputs.keyboard.last_action.key == GLFW_KEY_TAB && inputs.keyboard.last_action.action == GLFW_PRESS) {
		camera_mode = !camera_mode;
	}
}

void scene_structure::idle_frame()
{
	camera_control.idle_frame();
}
