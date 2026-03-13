
#include "cgp/01_base/base.hpp"
#include "camera_controller_orbit_euler.hpp"


namespace cgp
{


	void camera_controller_orbit_euler::action_mouse_move()
	{
		// Preconditions
		assert_cgp_no_msg(inputs != nullptr);
		assert_cgp_no_msg(window != nullptr);
		if (!is_active) return;

		vec2 const& p1 = inputs->mouse.position.current;
		vec2 const& p0 = inputs->mouse.position.previous;
		vec2 const dp = p1 - p0;

		bool const event_valid = !inputs->mouse.on_gui;
		bool const click_left = inputs->mouse.click.left && !(inputs->mouse.click.left && inputs->keyboard.is_pressed("k"));
		bool const click_right = inputs->mouse.click.right || (inputs->mouse.click.left && inputs->keyboard.is_pressed("k"));
		bool const ctrl = inputs->keyboard.ctrl;

		if (event_valid) { // If the mouse cursor is not on the ImGui area

			if (click_left && !ctrl)     // Rotation of the camera around its center
				camera_model.manipulator_rotate_roll_pitch_yaw(0, dp.y, -dp.x);
			else if (click_left && ctrl) // Translate/Pan the camera in the viewspace plane
				camera_model.manipulator_translate_in_plane(p1 - p0);
			else if (click_right && !ctrl) // Move the camera closer/further with respect to its center
				camera_model.manipulator_scale_distance_to_center((p1 - p0).y);
			else if (click_right && ctrl) // Translate the camera center in front/back
				camera_model.manipulator_translate_front((p1 - p0).y);
		}

	}
	void camera_controller_orbit_euler::idle_frame()
	{
		// Preconditions
		assert_cgp_no_msg(inputs != nullptr);
		assert_cgp_no_msg(window != nullptr);
		if (!is_active) return;

		bool const shift = inputs->keyboard.shift;
		float const angle_magnitude = 2 * inputs->time_interval;
		if (shift && (inputs->keyboard.left || inputs->keyboard.is_pressed(GLFW_KEY_R))) {
			camera_model.manipulator_twist_rotation_axis(angle_magnitude);
		}
		if (shift && (inputs->keyboard.right || inputs->keyboard.is_pressed(GLFW_KEY_F))) {
			camera_model.manipulator_twist_rotation_axis(-angle_magnitude); 
		}

	}

	void camera_controller_orbit_euler::set_rotation_axis(vec3 const& rotation_axis)
	{
		camera_model.set_rotation_axis(rotation_axis);
	}
	void camera_controller_orbit_euler::set_rotation_axis_x()
	{
		camera_model.set_rotation_axis({ 1,0,0 });
	}
	void camera_controller_orbit_euler::set_rotation_axis_y()
	{
		camera_model.set_rotation_axis({ 0,1,0 });
	}
	void camera_controller_orbit_euler::set_rotation_axis_z()
	{
		camera_model.set_rotation_axis({ 0,0,1 });
	}

	void camera_controller_orbit_euler::look_at(vec3 const& eye, vec3 const& center, vec3 const& )
	{
		camera_model.look_at(eye, center);
	}

	std::string camera_controller_orbit_euler::doc_usage() const
	{
		std::string doc;
		doc += "Info Camera Controller: Orbit Euler\n";
		doc += "  Camera that rotates around a central focus point.\n";
		doc += "  Note: Uses Euler angle description (XYZ/roll-pitch-yaw Tait-Bryan convention).\n";
		doc += "\n";
		doc += "Camera control:\n";
		doc += "  Mouse left + drag          : Rotate camera (pitch/yaw) around focus point\n";
		doc += "  Mouse right + drag         : Move closer/farther from focus point\n";
		doc += "   (or 'k' + Mouse left)       (focus point remains unchanged)\n";
		doc += "  Ctrl + Mouse left + drag   : Pan camera and focus point in view plane\n";
		doc += "  Ctrl + Mouse right + drag  : Translate camera and focus point front/back\n";
		doc += "   (or 'k' + Ctrl + left)    \n";
		doc += "  Shift + Left/Right (or r/f): Rotate \"up\" direction (rotation around z)\n";

		return doc;
	}
	
}