#include "camera_controller_generic_base.hpp"
#include "cgp/01_base/base.hpp"

#include <iostream>

namespace cgp
{
	void camera_controller_generic_base::initialize(input_devices& inputs_param, window_structure& window_param)
	{
		inputs = &inputs_param;
		window = &window_param;
	}

	bool camera_controller_generic_base::is_ready() const
	{
		assert_cgp_no_msg(inputs != nullptr);
		assert_cgp_no_msg(window != nullptr);
		return is_active;
	}

	void camera_controller_generic_base::update_cursor_capture()
	{
		assert_cgp_no_msg(inputs != nullptr);
		assert_cgp_no_msg(window != nullptr);
		if (inputs->keyboard.last_action.is_pressed(GLFW_KEY_C) && inputs->keyboard.shift) {
			is_cursor_trapped = !is_cursor_trapped;
			if (is_cursor_trapped)
				glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			else
				glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		// Escape also gives back the cursor
		if (inputs->keyboard.last_action.is_pressed(GLFW_KEY_ESCAPE)) {
			is_cursor_trapped = false;
			glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	std::string camera_controller_generic_base::doc_usage() const
	{
		std::string doc = "You seem to be using a Controller Generic Base - This controller is only a generic class and should be specialized.\n";
		return doc;
	}
}
