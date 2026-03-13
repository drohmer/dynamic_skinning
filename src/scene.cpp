#include "scene.hpp"
#include "loaders/normalize_character.hpp"
#include <chrono>

using namespace cgp;


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

	// Camera: look at origin with enough distance for a [-1,1] model
	camera_control.look_at({ 3.0f, 1.5f, 2.0f }, {0, 0, 0}, {0, 1, 0});

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

	// Reset picking state
	picking = joint_picking_state();

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

	// ********************************** //
	// Skinning (LBS)
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
	// Display
	// ********************************** //
	for (auto& rigged_mesh_entry : character.animated_model.rigged_mesh) {
		std::string mesh_name = rigged_mesh_entry.first;
		rigged_mesh_structure& rigged_mesh = rigged_mesh_entry.second;

		mesh_drawable& drawable = character.drawable[mesh_name];
		drawable.vbo_position.update(rigged_mesh.mesh_deformed.position);
		drawable.vbo_normal.update(rigged_mesh.mesh_deformed.normal);

		if (gui.display_surface) {
			drawable.material.texture_settings.active = gui.display_texture;
			draw(drawable, environment);
		}
		if (gui.display_wireframe) {
			draw_wireframe(drawable, environment);
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

	// Picking hover feedback
	if (picking.joint_hover >= 0) {
		vec3 hover_pos = character.animated_model.skeleton.joint_matrix_global[picking.joint_hover].get_block_translation();
		sphere_picking.model.translation = hover_pos;
		draw(sphere_picking, environment);
	}
}


void scene_structure::mouse_move_event()
{
	if (inputs.keyboard.shift) {
		auto& skeleton = character.animated_model.skeleton;

		if (picking.is_selected) {
			joint_picking_mouse_move(picking, skeleton, interactive_deformation,
				inputs.mouse.position.current,
				camera_control.camera_model, camera_projection,
				camera_control.camera_model.matrix_view());
		}
		else {
			picking.joint_hover = pick_skeleton_joint(skeleton,
				inputs.mouse.position.current,
				camera_control.camera_model, camera_projection);
		}
	}
	else {
		picking.joint_hover = -1;
		camera_control.action_mouse_move();
	}
}

void scene_structure::mouse_click_event()
{
	if (inputs.keyboard.shift) {
		auto& skeleton = character.animated_model.skeleton;

		bool click_left  = (inputs.mouse.click.last_action == last_mouse_cursor_action::click_left);
		bool click_right = (inputs.mouse.click.last_action == last_mouse_cursor_action::click_right);
		bool release     = (inputs.mouse.click.last_action == last_mouse_cursor_action::release_left) ||
		                   (inputs.mouse.click.last_action == last_mouse_cursor_action::release_right);

		if (release && picking.is_selected) {
			joint_picking_apply_deformation(picking, skeleton, interactive_deformation,
				character.animated_model.animation);
		}

		joint_picking_mouse_click(picking, skeleton,
			inputs.mouse.position.current,
			camera_control.camera_model, camera_projection,
			click_left, click_right, release);
	}
	else {
		camera_control.action_mouse_click();
	}
}

void scene_structure::keyboard_event()
{
	camera_control.action_keyboard();
}

void scene_structure::idle_frame()
{
	camera_control.idle_frame();
}
