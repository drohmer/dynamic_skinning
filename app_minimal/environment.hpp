#pragma once

#include "cgp/cgp.hpp"

using namespace cgp;


// Variables sent to the shaders when drawing a mesh: camera and light
struct environment_structure : environment_generic_structure
{
	vec3 background_color = {1, 1, 1};
	mat4 camera_view;
	mat4 camera_projection;
	vec3 light = {1, 1, 1};

	void send_opengl_uniform(opengl_shader_structure const& shader, bool expected = default_expected_uniform) const override;
};


struct project {
	// Path to the root of the project (access to shaders/ and assets/)
	static std::string path;
};
