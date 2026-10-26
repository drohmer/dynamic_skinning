#include "scene.hpp"
#include "loaders/normalize_character.hpp"
#include "loaders/procedural_animations.hpp"
#include <chrono>
#include <iomanip>

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
	else if (name == "Cthulhu") {
		std::string resolution = cthulhu_resolutions[cthulhu_resolution_index];
		character = load_cthulhu_character(project::path + "assets/cthulhu/",
		                                  resolution, cthulhu_use_bend_weights);
	}
	else {
		auto models = get_legacy_models();
		for (auto const& m : models) {
			if (m.name == name) {
				character = load_legacy_character(project::path + m.directory, m.scaling, m.invert_normals, m.rotation);
				break;
			}
		}
	}

	// Pre-designed animations (built on the rest pose, before normalization)
	if (name == "Giraffe") {
		character.animated_model.animation["Neck swing"] = build_giraffe_neck_swing(character.animated_model.skeleton);
		character.set_current_animation("Neck swing");
	}
	if (name == "Clown") {
		character.animated_model.animation["Swing"] = build_clown_swing(character.animated_model.skeleton);
		character.set_current_animation("Swing");
		// Flexible origins at the start of the arms and of the hat (paper, Sec. "Deformation origin control")
		if (clown_flexible_origins) {
			for (char const* joint : {"joint8", "joint11", "joint19"})
				character.animated_model.flexible_origin_joints.insert(find_joint(character.animated_model.skeleton, joint));
		}
	}
	if (name == "Spot") {
		character.animated_model.animation["Head nod & twist"] = build_spot_head_nod_twist(character.animated_model.skeleton);
		character.set_current_animation("Head nod & twist");
	}
	if (name == "Snail") {
		character.animated_model.animation["Head up & down"] = build_snail_head_up_down(character.animated_model.skeleton);
		character.set_current_animation("Head up & down");
	}
	if (name == "Dragon") {
		// The wings beat at the frequency f of the oscillations (resonance of the wing tips)
		flap_frequency = ds_params.frequency / (2 * Pi);
		character.animated_model.animation["Flight"] = build_dragon_flight(character.animated_model.skeleton, flap_frequency);
		character.set_current_animation("Flight");
	}
	if (name == "Train") {
		character.animated_model.animation["Bumps"] = build_train_bumps(character.animated_model.skeleton);
		character.set_current_animation("Bumps");
	}
	if (name == "Cthulhu") {
		character.animated_model.animation["Head shake"] = build_cthulhu_head_shake(character.animated_model.skeleton);
		character.set_current_animation("Head shake");
	}

	// Normalize all models to [-1, 1] range
	normalization_transform const normalization = normalize_character(character);
	if (name == "Train")
		initialize_train_track(normalization);

	// Camera: hard-coded presets per model
	{
		// eye, center, distance, and up vector of the camera (Shift+P prints the current camera as such a line)
		struct { const char* name; vec3 eye; vec3 center; float dist; vec3 up; } presets[] = {
			{"Cylinder", {1.674f, 0.242f, 3.456f},  {-0.486f, 0.183f, 0.204f},  3.905f, {0, 1, 0}},
			{"Cthulhu",  {0.508f, -0.751f, 0.067f}, {0.000f, -0.060f, 0.080f}, 0.857f, {-0.118f, -0.106f, 0.987f}},
			{"Clown",    {0.790f, 0.575f, 3.170f},  {0.000f, 0.100f, 0.000f},   3.300f, {0, 1, 0}},
			{"Dragon",   {-2.700f, 0.500f, 0.600f},  {0.000f, 0.100f, 0.000f},   2.600f, {0, 1, 0}},
			{"Giraffe",   {1.035f, 0.351f, 3.551f},  {0.000f, 0.100f, 0.050f},  3.659f, {0, 1, 0}},
			{"Snail",    {1.600f, 0.700f, 2.900f},  {0.100f, 0.000f, 0.000f},   3.300f, {0, 1, 0}},
			{"Spot",     {2.490f, 0.850f, -2.590f}, {0.000f, 0.100f, -0.100f}, 3.600f, {0, 1, 0}},
			{"Train",    {0.850f, 0.350f, 1.500f},  {-0.050f, -0.020f, 0.000f}, 1.750f, {0, 1, 0}},
		};
		bool found = false;
		for (auto& d : presets) {
			if (name == d.name) {
				camera_control.look_at(d.eye, d.center, d.up);
				camera_control.camera_model.distance_to_center = d.dist;
				found = true;
				break;
			}
		}
		if (!found)
			camera_control.look_at({ 3.0f, 1.5f, 2.0f }, {0, 0, 0}, {0, 1, 0});
	}

	// GPU skinning is enabled by default and the choice is kept between models,
	// except for Cthulhu (high resolution meshes) where the default path is always set
	if (name == "Cthulhu")
		use_gpu_skinning = gpu_skinning_default;

	// Amplitude A and drag D: default values, smaller for Cthulhu (large wings and long tentacles),
	// larger drag for the Clown with local centers (only the extremities are deformed)
	dynamic_skinning_parameters const ds_default;
	ds_params.amplitude = (name == "Cthulhu") ? 0.47f : ds_default.amplitude;
	ds_params.drag      = (name == "Cthulhu") ? 0.30f : ds_default.drag;
	if (name == "Clown" && clown_flexible_origins)
		ds_params.drag = 1.0f;

	// Delayed LBS: shown alone with the train (without dynamic skinning), off for the other models
	ds_params.active = (name != "Train");
	ds_params.delay_active = (name == "Train");
	ds_params.delay_speed = (name == "Train") ? 3.0f : ds_default.delay_speed;

	reinitialize_after_model_change();
}


void scene_structure::reinitialize_after_model_change()
{
	vbo_has_bind_pose = false;  // new drawables
	// Initialize dynamic skinning on the first mesh
	std::string first_mesh = character.animated_model.rigged_mesh.begin()->first;
	dynamic_skinning_initialize(ds_state, character.animated_model, first_mesh);
	delayed_lbs_initialize(dlbs_state, character.animated_model, first_mesh, ds_timer.t);

	// Initialize interactive deformation (identity per joint)
	int N_joints = character.animated_model.skeleton.size();
	interactive_deformation.resize(N_joints);
	for (int k = 0; k < N_joints; ++k)
		interactive_deformation[k] = mat4::build_identity();

	// Rest local matrices (used for joints that have no animation channel)
	skeleton_local_rest = character.animated_model.skeleton.joint_matrix_local;
	animation_initial = character.animated_model.animation;
	skeleton_local_rest_initial = skeleton_local_rest;

	// Reset picking state and weight color cache
	picking = joint_picking_state();
	weight_colors_cached_joint = -2;

	// Initialize GPU skinning
	std::string first_mesh_gpu = character.animated_model.rigged_mesh.begin()->first;
	gpu_skinning_initialize(gpu_state, character.animated_model, first_mesh_gpu,
	                        ds_state, dlbs_state, character.drawable[first_mesh_gpu]);

	character.timer.start();
	character.timer.stop();
	animation_running = false;
}


void scene_structure::initialize_train_track(normalization_transform const& normalization)
{
	// Placement of the track relative to the train asset (as in the original scene, where the
	// train was scaled by 0.2 and the track by 0.0025, then translated by (-10, -0.12, 0))
	float const scaling_asset = 0.0025f / 0.2f;
	vec3 const translation_asset = vec3{-10.0f, -0.12f, 0.0f} / 0.2f;
	// Length of the repeated patterns in the track files
	float const period_bars = 100.0f;
	float const period_pillars = 582.353f;

	struct { char const* file; vec3 color; float period; } const parts[] = {
		{"train_rails.obj",   {0.55f, 0.55f, 0.6f},  period_bars},
		{"train_bars.obj",    {0.45f, 0.3f, 0.2f},   period_bars},
		{"train_pillars.obj", {0.75f, 0.72f, 0.68f}, period_pillars},
	};

	train_track.clear();
	for (auto const& part : parts) {
		track_part p;
		p.drawable.initialize_data_on_gpu(mesh_load_file_obj(project::path + "assets/train/track/" + part.file));
		p.drawable.material.color = part.color;
		// Display coordinates: p' = s (scaling_asset p + translation_asset - center)
		p.drawable.model.scaling = normalization.scaling * scaling_asset;
		p.drawable.model.translation = normalization.scaling * (translation_asset - normalization.center);
		p.period = normalization.scaling * scaling_asset * part.period;
		train_track.push_back(p);
	}
}


void scene_structure::draw_train_track()
{
	// The track scrolls backward (-x), wrapping around its repeated pattern
	for (auto& part : train_track) {
		vec3 const translation = part.drawable.model.translation;
		part.drawable.model.translation.x -= std::fmod(train_speed * character.timer.t, part.period);
		draw(part.drawable, environment);
		part.drawable.model.translation = translation;
	}
}


void scene_structure::update_flight_frequency()
{
	float const f = ds_params.frequency / (2 * Pi);
	if (std::abs(f - flap_frequency) < 1e-3f)
		return;
	flap_frequency = f;

	// Rebuild from the rest pose (normalized skeleton)
	skeleton_structure rest = character.animated_model.skeleton;
	rest.joint_matrix_local = skeleton_local_rest;
	rest.update_joint_matrix_local_to_global();
	character.animated_model.animation["Flight"] = build_dragon_flight(rest, f);

	// The duration of the loop depends on f
	if (character.current_animation_name == "Flight") {
		character.set_current_animation("Flight");
		character.timer.t_periodic = std::fmod(character.timer.t_periodic, character.timer.event_period);
	}
}


bool scene_structure::show_bend_weights() const
{
	// Only for the Cthulhu with its per-vertex bend weights (the other models have uniform weights)
	return display_bend_weights && current_model_name == "Cthulhu" && cthulhu_use_bend_weights;
}


void scene_structure::initialize()
{
	camera_control.initialize(inputs, window);
	camera_control.look_at({ 1.0f, 0.5f, 0.5f }, {0.5f, 0, 0}, {0, 1, 0});

	// Picking feedback sphere (green)
	sphere_picking.initialize_data_on_gpu(mesh_primitive_sphere());
	sphere_picking.model.scaling = 0.03f;
	sphere_picking.material.color = {0, 1, 0};

	load_model("Giraffe");  // default model: the global oscillation of the teaser of the paper
	ds_timer.start();

	// Pinch zoom on touch screens (web version only)
	touch_gestures_initialize(touch);
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
	if (current_model_name == "Dragon")
		update_flight_frequency();
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
	// History of the poses for the delayed LBS
	delayed_lbs_record(dlbs_state, character.animated_model, ds_timer.t);
	auto ta1 = clock::now();
	timing_animation.push(to_ms(ta1 - ta0));

	if (use_gpu_skinning) {
		// ********************************** //
		// GPU Skinning path
		// ********************************** //
		std::string first_mesh = character.animated_model.rigged_mesh.begin()->first;
		auto& rigged_mesh = character.animated_model.rigged_mesh[first_mesh];
		mesh_drawable& drawable = character.drawable[first_mesh];

		// Ensure VBO contains bind-pose data (GPU shader applies LBS itself): uploaded only after a model change or
		// the CPU path, not at each frame (about 6 MB per frame for Cthulhu)
		if (!vbo_has_bind_pose) {
			drawable.vbo_position.update(rigged_mesh.mesh_bind_pose.position);
			drawable.vbo_normal.update(rigged_mesh.mesh_bind_pose.normal);
			vbo_has_bind_pose = true;
		}

		gpu_skinning_update(gpu_state, character.animated_model, first_mesh, ds_params, dlbs_state, ds_timer.t);
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
			gpu_skinning_draw(gpu_state, drawable, environment, ds_params, dlbs_state, show_bend_weights());
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
		// Delayed LBS replaces p^LBS by p^dLBS (before the dynamic skinning deformation)
		if (ds_params.delay_active) {
			std::string first_mesh = character.animated_model.rigged_mesh.begin()->first;
			delayed_lbs_apply(dlbs_state, ds_params, character.animated_model, first_mesh);
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
			vbo_has_bind_pose = false;

			// Encode bend weights as color: white (w=0) → red (w=1)
			if (show_bend_weights() && ds_state.weight_bend.size() > 0) {
				int N = rigged_mesh.mesh_deformed.position.size();
				numarray<vec3> colors(N);
				for (int i = 0; i < N; ++i) {
					float w = ds_state.weight_bend[i];
					colors[i] = (1 - w) * vec3{1, 1, 1} + w * vec3{0.85f, 0.45f, 0.42f};  // white -> soft red
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

	if (current_model_name == "Train")
		draw_train_track();

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

	// Flexible origins (yellow spheres)
	if (ds_params.active) {
		sphere_picking.material.color = {1.0f, 0.9f, 0.1f};
		for (int joint : character.animated_model.flexible_origin_joints) {
			sphere_picking.model.translation = character.animated_model.skeleton.joint_matrix_global[joint].get_block_translation();
			draw(sphere_picking, environment);
		}
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

	// Two fingers on a touch screen: pinch zoom only (handled in idle_frame)
	if (touch.multi_touch) {
		picking.joint_hover = -1;
		return;
	}

	if (camera_mode) {
		// Camera mode: standard behavior of the framework
		// (left click = rotation, right click = zoom, Ctrl + click = translation)
		camera_control.action_mouse_move();
		picking.joint_hover = -1;
		return;
	}

	// Touch screen: the finger drags the joint picked when it touched the screen, or orbits the camera
	if (touch_interaction) {
		picking.joint_hover = -1;
		if (picking.is_selected)
			joint_picking_mouse_move(picking, character.animated_model.skeleton, interactive_deformation,
				inputs.mouse.position.current,
				camera_control.camera_model, camera_projection,
				camera_control.camera_model.matrix_view());
		else if (touch_orbit && inputs.mouse.click.left)
			camera_control.camera_model.manipulator_rotate_arcball(p0, p1);
		return;
	}

	// Bone-edit mode:
	// Shift + left click → camera rotation (arcball)
	if (inputs.mouse.click.left && inputs.keyboard.shift && !on_gui) {
		picking.joint_hover = -1;
		camera_control.camera_model.manipulator_rotate_arcball(p0, p1);
	}
	// Shift + right click → zoom
	else if (inputs.mouse.click.right && inputs.keyboard.shift && !on_gui) {
		picking.joint_hover = -1;
		camera_control.camera_model.manipulator_scale_distance_to_center((p1 - p0).y);
	}
	else {
		// Left click → joint rotation, right click → joint twist
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
		// Camera mode or Shift → no picking
		return;
	}

	// Bone-edit mode: left click = rotation, right click = twist
	auto& skeleton = character.animated_model.skeleton;

	bool click_left  = (inputs.mouse.click.last_action == last_mouse_cursor_action::click_left);
	bool click_right = (inputs.mouse.click.last_action == last_mouse_cursor_action::click_right);
	bool release     = (inputs.mouse.click.last_action == last_mouse_cursor_action::release_left) ||
	                   (inputs.mouse.click.last_action == last_mouse_cursor_action::release_right);

	// Click on the GUI → no picking (the release is still handled, to end a drag)
	if (inputs.mouse.on_gui && (click_left || click_right))
		return;

	// Touch screen: a finger on a joint (within 18 GUI units: about 30 points on a tablet) drags it, elsewhere it
	// orbits the camera
	if (touch_interaction) {
		if (click_left) {
			int const joint = pick_skeleton_joint_screen(skeleton, inputs.mouse.position.current,
				camera_control.camera_model, camera_projection,
				{float(window.width), float(window.height)}, 18.0f * project::gui_scale);
			if (joint != -1) {
				joint_picking_select(picking, skeleton, joint);
				picking.click_button = picking_click_button::left;
			}
			else
				touch_orbit = true;
		}
		if (release) {
			if (picking.is_selected)
				joint_picking_apply_deformation(picking, skeleton, interactive_deformation,
					character.animated_model.animation, skeleton_local_rest);
			picking.is_selected = false;
			picking.click_button = picking_click_button::none;
			touch_orbit = false;
		}
		return;
	}

	if (release && picking.is_selected) {
		joint_picking_apply_deformation(picking, skeleton, interactive_deformation,
			character.animated_model.animation, skeleton_local_rest);
	}

	joint_picking_mouse_click(picking, skeleton,
		inputs.mouse.position.current,
		camera_control.camera_model, camera_projection,
		click_left, click_right, release);
}

void scene_structure::mouse_scroll_event()
{
	// Mouse wheel / two-finger trackpad scroll (and trackpad pinch on the web version) = zoom, in both modes
	if (inputs.mouse.on_gui)
		return;
	float const scroll = std::max(-3.0f, std::min(3.0f, float(inputs.mouse.scroll)));
	zoom_camera(std::exp(-0.08f * scroll));
}

void scene_structure::zoom_camera(float factor)
{
	auto& cam = camera_control.camera_model;
	cam.distance_to_center = std::max(0.05f, std::min(50.0f, cam.distance_to_center * factor));
}

void scene_structure::reset_pose()
{
	// The joint edits are stored in the animations and in the rest local matrices (joint_picking_apply_deformation)
	character.animated_model.animation = animation_initial;
	skeleton_local_rest = skeleton_local_rest_initial;
	for (auto& M : interactive_deformation)
		M = mat4::build_identity();
	picking.is_selected = false;
	picking.click_button = picking_click_button::none;
	flap_frequency = 0.0f;  // Dragon: "Flight" rebuilt at the next frame for the current frequency

	// Animation paused at its start (the jump to this pose triggers the oscillations, like a release)
	animation_running = false;
	character.timer.stop();
	character.timer.t = 0.0f;
	character.timer.t_periodic = 0.0f;
}

void scene_structure::keyboard_event()
{
	camera_control.action_keyboard();

	// Tab → toggle bone-edit / camera mode (only on key press, not release)
	if (inputs.keyboard.last_action.key == GLFW_KEY_TAB && inputs.keyboard.last_action.action == GLFW_PRESS) {
		camera_mode = !camera_mode;
	}

	// Shift+P → print the current camera as a preset line to paste in load_model
	if (inputs.keyboard.last_action.key == GLFW_KEY_P && inputs.keyboard.last_action.action == GLFW_PRESS && inputs.keyboard.shift) {
		auto const& cam = camera_control.camera_model;
		vec3 const eye = cam.position();
		vec3 const center = cam.center_of_rotation;
		vec3 const up = cam.orientation() * vec3{0, 1, 0};
		std::cout << std::fixed << std::setprecision(3)
		          << "{\"" << current_model_name << "\", {" << eye.x << "f, " << eye.y << "f, " << eye.z << "f}, {"
		          << center.x << "f, " << center.y << "f, " << center.z << "f}, " << cam.distance_to_center << "f, {"
		          << up.x << "f, " << up.y << "f, " << up.z << "f}}," << std::endl;
	}
}

void scene_structure::idle_frame()
{
	camera_control.idle_frame();

	// Pinch zoom accumulated by the touch callbacks
	float const pinch = touch_gestures_consume_zoom(touch);
	if (pinch != 1.0f)
		zoom_camera(pinch);

	// Two-finger translation: the center of rotation moves in the plane of the screen, so that the model
	// follows the fingers (at the depth of the center: a relative unit is d tan(fov/2) in height)
	vec2 const pan = touch_gestures_consume_pan(touch);
	if (pan.x != 0.0f || pan.y != 0.0f) {
		auto& cam = camera_control.camera_model;
		float const s = cam.distance_to_center * std::tan(camera_projection.field_of_view / 2.0f);
		cam.manipulator_translate_in_plane(-s * vec2{camera_projection.aspect_ratio * pan.x, pan.y});
	}
}
