#include "scene.hpp"


using namespace cgp;


// Normalize the displayed mesh to fit within [-1, 1] on the largest axis.
// Requires one skinning pass to have been run first (mesh_deformed in display coordinates).
// Only translations are scaled — rotation blocks stay pure (no scaling embedded).
//
// Math: after normalization, skinning produces p' = s*(p_deformed - center).
// - Skeleton: offset root by -center, scale all translations by s
// - Animations: same treatment
// - Inverse bind: scale by s (derived from inv_bind' = s * inv_bind)
// - Mesh bind pose: UNCHANGED (it may live in a different scale, as with asset_loader)
static void normalize_character(character_structure& character)
{
    auto& model = character.animated_model;

    // 1. Run one skinning pass to get mesh_deformed in display coordinates
    model.set_skeleton_from_animation(
        character.current_animation_name, 0.0f);
    for (auto& entry : model.rigged_mesh)
        model.skinning_lbs(entry.first);

    // 2. Compute bounding box from deformed mesh (display coordinates)
    vec3 p_min = vec3(1e10f, 1e10f, 1e10f);
    vec3 p_max = vec3(-1e10f, -1e10f, -1e10f);
    for (auto const& entry : model.rigged_mesh) {
        for (auto const& p : entry.second.mesh_deformed.position) {
            p_min = vec3(std::min(p_min.x, p.x), std::min(p_min.y, p.y), std::min(p_min.z, p.z));
            p_max = vec3(std::max(p_max.x, p.x), std::max(p_max.y, p.y), std::max(p_max.z, p.z));
        }
    }

    vec3 center = (p_min + p_max) / 2.0f;
    vec3 extent = p_max - p_min;
    float max_extent = std::max(std::max(extent.x, extent.y), extent.z);
    if (max_extent < 1e-5f) return;
    float s = 2.0f / max_extent;

    // 3. Transform skeleton: offset root by -center, then scale ALL translations by s.
    //    Rotation blocks are untouched — joint_matrix_global will have pure rotations.
    if (model.skeleton.joint_matrix_local.size() > 0) {
        vec3 t0 = model.skeleton.joint_matrix_local[0].get_block_translation();
        model.skeleton.joint_matrix_local[0].set_block_translation(t0 - center);

        int N_joints = model.skeleton.joint_matrix_local.size();
        for (int k = 0; k < N_joints; ++k)
            model.skeleton.joint_matrix_local[k].apply_scaling_to_block_translation(s);

        model.skeleton.update_joint_matrix_local_to_global();
    }

    // 4. Transform animation matrices (same pattern: offset root, scale all translations)
    for (auto& anim_entry : model.animation) {
        auto& anim = anim_entry.second;
        for (int k = 0; k < static_cast<int>(anim.joint_index.size()); ++k) {
            bool is_root = (anim.joint_index[k] == 0);

            int N_frames = anim.matrix[k].size();
            for (int t = 0; t < N_frames; ++t) {
                if (is_root) {
                    vec3 tr = anim.matrix[k][t].get_block_translation();
                    anim.matrix[k][t].set_block_translation(tr - center);
                }
                anim.matrix[k][t].apply_scaling_to_block_translation(s);
            }
        }
    }

    // 5. Transform inverse bind matrices: inv_bind' = s * inv_bind
    //    This is derived from the constraint: M_global' * inv_bind' = N * M_global * inv_bind
    //    where N = [sI | -s*center] is the display-space normalization.
    //    Since M_global'^{-1} * N * M_global = s*I for all joints, inv_bind' = s * inv_bind.
    for (auto& entry : model.rigged_mesh) {
        auto& inv_bind = entry.second.controller_skinning.inverse_bind_matrices;
        for (int k = 0; k < static_cast<int>(inv_bind.size()); ++k)
            inv_bind[k].apply_scaling(s);
    }

    // 6. Rebuild skeleton drawable with normalized joint positions
    character.sk_drawable = skeleton_drawable(model.skeleton);
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

	// ********************************** //
	// Set skeleton from animation
	// ********************************** //
	character.animated_model.set_skeleton_from_animation(
		character.current_animation_name, character.timer.t_periodic);

	// ********************************** //
	// Apply interactive deformation
	// ********************************** //
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

	// ********************************** //
	// Skinning (LBS)
	// ********************************** //
	for (auto& rigged_mesh_entry : character.animated_model.rigged_mesh) {
		character.animated_model.skinning_lbs(rigged_mesh_entry.first);
	}

	// ********************************** //
	// Dynamic skinning (after LBS)
	// ********************************** //
	if (ds_params.active) {
		std::string first_mesh = character.animated_model.rigged_mesh.begin()->first;
		dynamic_skinning_apply(ds_state, ds_params, character.animated_model, first_mesh, ds_timer.t);
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
	// but still have correct depth/lighting among themselves
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


void scene_structure::display_gui()
{
	// ---- Model Selection ----
	if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::BeginCombo("Model##combo", current_model_name.c_str())) {
			// Built-in models
			static const char* builtin[] = {"Cylinder", "Lola"};
			for (int i = 0; i < 2; ++i) {
				bool is_selected = (current_model_name == builtin[i]);
				if (ImGui::Selectable(builtin[i], is_selected)) {
					if (current_model_name != builtin[i])
						load_model(builtin[i]);
				}
				if (is_selected) ImGui::SetItemDefaultFocus();
			}
			// Legacy models
			auto legacy_models = get_legacy_models();
			for (auto const& m : legacy_models) {
				bool is_selected = (m.name == current_model_name);
				if (ImGui::Selectable(m.name.c_str(), is_selected)) {
					if (current_model_name != m.name)
						load_model(m.name);
				}
				if (is_selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	// ---- Animation ----
	if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
		// Animation selection combo box
		if (character.animated_model.animation.size() > 1) {
			if (ImGui::BeginCombo("Animation##combo", character.current_animation_name.c_str())) {
				for (auto const& entry : character.animated_model.animation) {
					bool is_selected = (entry.first == character.current_animation_name);
					if (ImGui::Selectable(entry.first.c_str(), is_selected)) {
						character.set_current_animation(entry.first);
					}
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		ImGui::SliderFloat("Timer", &character.timer.t_periodic, 0.0f, character.timer.event_period, "%.2f s");
		ImGui::SliderFloat("Time scale", &character.timer.scale, 0.0f, 3.0f, "%.3f");

		// Start / Stop buttons
		if (ImGui::Button("Start")) { character.timer.start(); }
		ImGui::SameLine();
		if (ImGui::Button("Stop")) { character.timer.stop(); }
	}

	// ---- Display ----
	if (ImGui::CollapsingHeader("Display")) {
		ImGui::Checkbox("Mesh", &gui.display_surface); ImGui::SameLine();
		ImGui::Checkbox("Wireframe", &gui.display_wireframe); ImGui::SameLine();
		ImGui::Checkbox("Texture", &gui.display_texture);

		ImGui::Checkbox("Bones", &gui.display_skeleton_bones); ImGui::SameLine();
		ImGui::Checkbox("Joints", &gui.display_skeleton_joints); ImGui::SameLine();
		ImGui::Checkbox("Frames", &gui.display_skeleton_frames);
	}

	// ---- Skeleton Interaction ----
	if (ImGui::CollapsingHeader("Sk. Interaction")) {
		if (ImGui::RadioButton("Translation", !picking.deformation_is_rotation))
			picking.deformation_is_rotation = false;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotation", picking.deformation_is_rotation))
			picking.deformation_is_rotation = true;
	}

	// ---- Dynamic Skinning ----
	if (ImGui::CollapsingHeader("Dynamic Skinning", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Bend magnitude", &ds_params.bend_magnitude, 0.0f, 2.0f, "%.2f");

		dynamic_skinning_display_gui(ds_params);

		ImGui::SliderFloat("Bend max angle", &ds_params.bend_max_angle, 0.0f, 3.14f);
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
