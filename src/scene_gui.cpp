#include "scene.hpp"

using namespace cgp;


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

	// ---- Timings ----
	if (ImGui::CollapsingHeader("Timings")) {
		ImGui::Text("Animation + skeleton: %.3f ms", timing_animation.average());
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
