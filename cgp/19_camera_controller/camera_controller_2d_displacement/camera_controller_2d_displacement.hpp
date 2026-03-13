#pragma once

#include "../camera_controller_first_person_euler/camera_controller_first_person_euler.hpp"

namespace cgp {

struct camera_controller_2d_displacement : camera_controller_first_person_euler
{
	void action_mouse_move();
	void idle_frame();

	std::string doc_usage() const;
};


}