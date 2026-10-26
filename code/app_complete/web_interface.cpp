// Functions called by the HTML panel of the web page on touch screens (web version only): the panel replaces
// the ImGui toolbar with controls sized for the fingers. Called from JavaScript with Module.ccall.
// The parameters use the notations of the paper: f (Hz), tau (s), A, D, c.
#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <algorithm>
#include <string>

#include "scene.hpp"

extern scene_structure scene;

enum web_parameter { web_frequency = 0, web_damping = 1, web_amplitude = 2, web_drag = 3, web_delay_speed = 4 };

extern "C" {

// Names of the models, separated by commas, in the display order
EMSCRIPTEN_KEEPALIVE char const* web_model_names()
{
	static std::string names;
	names.clear();
	for (auto const& name : scene.model_names())
		names += (names.empty() ? "" : ",") + name;
	return names.c_str();
}

EMSCRIPTEN_KEEPALIVE char const* web_current_model() { return scene.current_model_name.c_str(); }

EMSCRIPTEN_KEEPALIVE void web_load_model(char const* name)
{
	if (scene.current_model_name != name)
		scene.load_model(name);
}

EMSCRIPTEN_KEEPALIVE int web_is_playing() { return scene.animation_running ? 1 : 0; }

EMSCRIPTEN_KEEPALIVE void web_set_playing(int playing)
{
	scene.animation_running = (playing != 0);
	if (scene.animation_running) scene.character.timer.start();
	else                         scene.character.timer.stop();
}

EMSCRIPTEN_KEEPALIVE void web_reset() { scene.reset_pose(); }

EMSCRIPTEN_KEEPALIVE int web_dynamic_skinning() { return scene.ds_params.active ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void web_set_dynamic_skinning(int active) { scene.ds_params.active = (active != 0); }

EMSCRIPTEN_KEEPALIVE int web_delay() { return scene.ds_params.delay_active ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void web_set_delay(int active)
{
	scene.ds_params.delay_active = (active != 0);
	// First activation: show the delay alone (as the Delay checkbox of the toolbar)
	if (scene.ds_params.delay_active && !scene.delay_enabled_once) {
		scene.delay_enabled_once = true;
		scene.ds_params.active = false;
	}
}

EMSCRIPTEN_KEEPALIVE int web_skeleton() { return scene.gui.display_skeleton_bones || scene.gui.display_skeleton_joints ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void web_set_skeleton(int visible)
{
	scene.gui.display_skeleton_bones = (visible != 0);
	scene.gui.display_skeleton_joints = (visible != 0);
}

EMSCRIPTEN_KEEPALIVE float web_parameter_value(int parameter)
{
	auto const& p = scene.ds_params;
	switch (parameter) {
	case web_frequency:   return p.frequency / (2 * cgp::Pi);
	case web_damping:     return 1.0f / std::max(p.attenuation, 0.01f);
	case web_amplitude:   return p.amplitude;
	case web_drag:        return p.drag;
	case web_delay_speed: return p.delay_speed;
	}
	return 0.0f;
}

EMSCRIPTEN_KEEPALIVE void web_set_parameter(int parameter, float value)
{
	auto& p = scene.ds_params;
	switch (parameter) {
	case web_frequency:   p.frequency = 2 * cgp::Pi * value; break;
	case web_damping:     p.attenuation = 1.0f / std::max(value, 0.01f); break;
	case web_amplitude:   p.amplitude = value; break;
	case web_drag:        p.drag = value; break;
	case web_delay_speed: p.delay_speed = value; break;
	}
}

// ImGui windows: hidden when the HTML panel is used, shown again by its "Advanced" button
EMSCRIPTEN_KEEPALIVE int web_show_gui() { return scene.show_gui ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE void web_set_show_gui(int visible) { scene.show_gui = (visible != 0); }
// Settings window of ImGui (all the parameters): opened with the toolbar by the "All settings" button
EMSCRIPTEN_KEEPALIVE void web_set_show_settings(int visible) { scene.show_settings = (visible != 0); }

}

#endif
