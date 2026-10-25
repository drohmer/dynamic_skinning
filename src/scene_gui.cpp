#include "scene.hpp"
#include <algorithm>

using namespace cgp;


// Button drawn as selected (highlighted) when active. Returns true when clicked.
static bool toggle_button(char const* label, bool active)
{
	if (active) {
		ImVec4 const color = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
	}
	bool const clicked = ImGui::Button(label);
	if (active)
		ImGui::PopStyleColor(2);
	return clicked;
}


// Main toolbar: interaction mode, model, animation and dynamic skinning
void scene_structure::display_gui()
{
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::Begin("Dynamic Skinning", NULL, ImGuiWindowFlags_AlwaysAutoResize);

	// ---- Interaction mode ----
	if (toggle_button("Bone edit", !camera_mode))
		camera_mode = false;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Left drag on a joint: rotation\nRight drag on a joint: twist\nScroll / pinch: zoom");
	ImGui::SameLine();
	if (toggle_button("Camera", camera_mode))
		camera_mode = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Left drag: rotation\nRight drag / scroll / pinch: zoom\nCtrl + left drag: translation");
	ImGui::SameLine();
	ImGui::TextDisabled("(Tab)");

	// ---- Model ----
	ImGui::SetNextItemWidth(160);
	if (ImGui::BeginCombo("Model", current_model_name.c_str())) {
		// Display order: models with a pre-designed animation first, then the other legacy models
		std::vector<std::string> model_names = {"Cylinder", "Girafe", "Clown", "Spot", "Snail", "Dragon", "Lola", "Cthulhu"};
		for (auto const& m : get_legacy_models())
			if (std::find(model_names.begin(), model_names.end(), m.name) == model_names.end())
				model_names.push_back(m.name);

		for (auto const& model_name : model_names) {
			bool is_selected = (current_model_name == model_name);
			if (ImGui::Selectable(model_name.c_str(), is_selected)) {
				if (current_model_name != model_name)
					load_model(model_name);
			}
			if (is_selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// ---- Animation and dynamic skinning ----
	if (ImGui::Button(animation_running ? "Pause" : "Play", ImVec2(60, 0))) {
		animation_running = !animation_running;
		if (animation_running) character.timer.start();
		else                   character.timer.stop();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Dynamic skinning", &ds_params.active);
	ImGui::SameLine();
	if (toggle_button("Settings", show_settings))
		show_settings = !show_settings;

	// ---- Main oscillation parameters (with the notations of the paper) ----
	if (ds_params.active) {
		// Frequency f (Hz), stored as the angular frequency omega = 2 pi f
		float f = ds_params.frequency / (2 * Pi);
		ImGui::SetNextItemWidth(160);
		if (ImGui::SliderFloat("Frequency f", &f, 0.0f, 5.0f, "%.2f Hz"))
			ds_params.frequency = 2 * Pi * f;

		// Damping tau (s), stored as the attenuation 1/tau
		float tau = 1.0f / std::max(ds_params.attenuation, 0.01f);
		ImGui::SetNextItemWidth(160);
		if (ImGui::SliderFloat("Damping tau", &tau, 0.05f, 5.0f, "%.2f s", ImGuiSliderFlags_Logarithmic))
			ds_params.attenuation = 1.0f / tau;

		ImGui::SetNextItemWidth(160);
		ImGui::SliderFloat("Amplitude A", &ds_params.amplitude, 0.0f, 2.0f, "%.2f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Oscillations (driven by the changes of velocity)");

		// Drag D: weight of the velocity skinning term, independent of the oscillations
		ImGui::SetNextItemWidth(160);
		ImGui::SliderFloat("Drag D", &ds_params.drag, 0.0f, 3.0f, "%.2f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Lag during the motion (proportional to the velocity)");
	}

	ImGui::End();
}


// Advanced settings (all the parameters), displayed in a separate window
void scene_structure::display_gui_settings()
{
	// ---- Model-specific options ----
	if (current_model_name == "Cthulhu" && ImGui::CollapsingHeader("Cthulhu", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool changed = false;
		if (ImGui::Combo("Resolution", &cthulhu_resolution_index,
		                 cthulhu_resolutions, cthulhu_resolution_count)) {
			changed = true;
		}
		if (ImGui::Checkbox("Per-vertex bend weights", &cthulhu_use_bend_weights)) {
			changed = true;
			if (!cthulhu_use_bend_weights) display_bend_weights = false;
		}
		if (cthulhu_use_bend_weights) {
			ImGui::Checkbox("Display bend weights", &display_bend_weights);
		}
		if (changed) {
			load_model("Cthulhu");
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
	}

	// ---- Dynamic Skinning ----
	if (ImGui::CollapsingHeader("Dynamic Skinning", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("GPU Skinning", &use_gpu_skinning);
		dynamic_skinning_display_gui(ds_params);
	}

	// ---- Skeleton Interaction ----
	if (ImGui::CollapsingHeader("Bone edit")) {
		if (ImGui::RadioButton("Translation", !picking.deformation_is_rotation))
			picking.deformation_is_rotation = false;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotation", picking.deformation_is_rotation))
			picking.deformation_is_rotation = true;
	}

	// ---- Display ----
	if (ImGui::CollapsingHeader("Display")) {
		ImGui::Checkbox("Mesh", &gui.display_surface); ImGui::SameLine();
		ImGui::Checkbox("Wireframe", &gui.display_wireframe); ImGui::SameLine();
		ImGui::Checkbox("Texture", &gui.display_texture);

		ImGui::Checkbox("Bones", &gui.display_skeleton_bones); ImGui::SameLine();
		ImGui::Checkbox("Joints", &gui.display_skeleton_joints); ImGui::SameLine();
		ImGui::Checkbox("Frames", &gui.display_skeleton_frames);

		ImGui::Separator();
		ImGui::Text("Weight display:");
		if (ImGui::RadioButton("None##wd", weight_display == weight_display_mode::none))
			weight_display = weight_display_mode::none;
		ImGui::SameLine();
		if (ImGui::RadioButton("Skinning##wd", weight_display == weight_display_mode::skinning))
			weight_display = weight_display_mode::skinning;
		ImGui::SameLine();
		if (ImGui::RadioButton("Hierarchical##wd", weight_display == weight_display_mode::hierarchical))
			weight_display = weight_display_mode::hierarchical;
	}

	// ---- Timings ----
	if (ImGui::CollapsingHeader("Timings")) {
		ImGui::Text("Animation + skeleton: %.3f ms", timing_animation.average());

		if (use_gpu_skinning) {
			ImGui::Text("GPU upload:           %.3f ms", timing_gpu_upload.average());
			ImGui::Text("(LBS + DS run in vertex shader)");
			ImGui::Separator();
			float total = timing_animation.average() + timing_gpu_upload.average();
			ImGui::Text("Frame total (CPU):    %.3f ms", total);
		}
		else {
			ImGui::Text("LBS skinning:         %.3f ms", timing_lbs.average());
			if (ds_params.active) {
				ImGui::Text("DS Angular vel. filter:  %.3f ms", timing_ds_phase1.average());
				ImGui::Text("DS Vertex summation:  %.3f ms", timing_ds_phase2.average());
				ImGui::Text("DS Normals:           %.3f ms", timing_ds_normals.average());
				ImGui::Separator();
				float ds_total = timing_ds_phase1.average() + timing_ds_phase2.average() + timing_ds_normals.average();
				ImGui::Text("DS Total:             %.3f ms", ds_total);
			}
			ImGui::Separator();
			float total = timing_animation.average() + timing_lbs.average();
			if (ds_params.active)
				total += timing_ds_phase1.average() + timing_ds_phase2.average() + timing_ds_normals.average();
			ImGui::Text("Frame total (CPU):    %.3f ms", total);
		}
	}
}
