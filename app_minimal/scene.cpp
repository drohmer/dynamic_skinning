#include "scene.hpp"
#include "loaders/cylinder_loader.hpp"
#include "loaders/legacy_loader.hpp"
#include "loaders/normalize_character.hpp"
#include "loaders/procedural_animations.hpp"

using namespace cgp;


void scene_structure::initialize()
{
	camera_control.initialize(inputs, window);
	load_model("Cylinder");
	timer.start();
}


void scene_structure::load_model(std::string const& name)
{
	model_name = name;

	// Character (skeleton, skinning weights, mesh) and its animation
	if (name == "Cylinder") {
		character = load_cylinder_bending();
	}
	else {
		for (auto const& m : get_legacy_models())
			if (m.name == name)
				character = load_legacy_character(project::path + m.directory, m.scaling, m.invert_normals, m.rotation);
		if (name == "Girafe") character.animated_model.animation["Swing"] = build_girafe_neck_swing(character.animated_model.skeleton);
		if (name == "Clown")  character.animated_model.animation["Swing"] = build_clown_swing(character.animated_model.skeleton);
		character.set_current_animation("Swing");
	}
	normalize_character(character);  // fits in [-1, 1]
	mesh_name = character.animated_model.rigged_mesh.begin()->first;

	// Precomputation of the hierarchical weights β_ij, and empty history of the angular velocities Ω_j
	dynamic_skinning_initialize(ds_state, character.animated_model, mesh_name);

	// Camera: eye, center of rotation, up vector
	if (name == "Cylinder") camera_control.look_at({1.67f, 0.24f, 3.46f}, {-0.49f, 0.18f, 0.20f}, {0, 1, 0});
	if (name == "Girafe")   camera_control.look_at({1.04f, 0.35f, 3.55f}, {0.00f, 0.10f, 0.05f}, {0, 1, 0});
	if (name == "Clown")    camera_control.look_at({0.79f, 0.58f, 3.17f}, {0.00f, 0.10f, 0.00f}, {0, 1, 0});

	character.timer.start();
}


void scene_structure::display_frame()
{
	environment.camera_view = camera_control.camera_model.matrix_view();
	environment.light = camera_control.camera_model.position();
	character.timer.update();
	timer.update();

	animated_model_structure& model = character.animated_model;
	rigged_mesh_structure& rigged_mesh = model.rigged_mesh[mesh_name];

	// Pose of the skeleton at the current time of the animation (joint frames, and positions q_j)
	model.set_skeleton_from_animation(character.current_animation_name, character.timer.t_periodic);

	// Linear Blend Skinning: mesh_deformed = p_i^LBS
	model.skinning_lbs(mesh_name);

	// Dynamic skinning: mesh_deformed = p_i^DS = p_i^LBS + Σ_j β_ij ψ^bend(F^osc(Ω_j), p_i^LBS - q_j)
	if (dynamic_skinning_active) {
		ds_parameters.frequency   = 2 * Pi * f;  // ω = 2π f
		ds_parameters.attenuation = 1 / tau;
		ds_parameters.amplitude   = A;
		ds_parameters.drag        = D;
		dynamic_skinning_apply(ds_state, ds_parameters, model, mesh_name, timer.t);
	}

	// Display of the deformed mesh
	mesh_drawable& drawable = character.drawable[mesh_name];
	drawable.vbo_position.update(rigged_mesh.mesh_deformed.position);
	drawable.vbo_normal.update(rigged_mesh.mesh_deformed.normal);
	draw(drawable, environment);
}


void scene_structure::display_gui()
{
	ImGui::Begin("Dynamic Skinning", NULL, ImGuiWindowFlags_AlwaysAutoResize);

	char const* models[] = {"Cylinder", "Girafe", "Clown"};
	for (char const* m : models) {
		if (ImGui::RadioButton(m, model_name == m) && model_name != m)
			load_model(m);
		ImGui::SameLine();
	}
	ImGui::NewLine();

	ImGui::Checkbox("Dynamic skinning", &dynamic_skinning_active);
	ImGui::SliderFloat("Frequency f", &f, 0.0f, 5.0f, "%.2f Hz");
	ImGui::SliderFloat("Damping tau", &tau, 0.05f, 5.0f, "%.2f s", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Amplitude A", &A, 0.0f, 5.0f, "%.2f");
	ImGui::SliderFloat("Drag D", &D, 0.0f, 3.0f, "%.2f");

	ImGui::End();
}


// Camera: left drag = rotation, right drag or scroll = zoom, ctrl + left drag = translation
void scene_structure::mouse_move_event() { camera_control.action_mouse_move(); }
void scene_structure::mouse_click_event() { camera_control.action_mouse_click(); }
void scene_structure::keyboard_event() { camera_control.action_keyboard(); }
void scene_structure::idle_frame() { camera_control.idle_frame(); }
void scene_structure::mouse_scroll_event()
{
	if (!inputs.mouse.on_gui)
		camera_control.camera_model.distance_to_center *= std::exp(-0.08f * float(inputs.mouse.scroll));
}
