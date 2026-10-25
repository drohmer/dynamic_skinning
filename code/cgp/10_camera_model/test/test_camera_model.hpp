#pragma once

namespace cgp_test
{
	// Tests on the camera models (module 10, GL-free):
	//  - look_at coherence (front/position) across the 4 camera types
	//  - uniform sign convention of the manipulator_translate_* functions
	void test_camera_model();
}
