#include "test_camera_model.hpp"

#include "cgp/01_base/base.hpp"
#include "cgp/10_camera_model/camera_model.hpp"

#include <cmath>

using namespace cgp;

namespace cgp_test
{
	namespace
	{
		// All camera models must share the same sign convention for the
		//  manipulator_translate_* functions:
		//    manipulator_translate_in_plane({sx,sy}) moves the camera by
		//         sx * right() + sy * up()
		//    manipulator_translate_front(s)          moves the camera by
		//         s * front()
		//  (verified through camera.position(), which for the orbit cameras
		//   moves together with the center of rotation).
		template <typename CAM>
		void check_translation_convention()
		{
			{
				CAM cam;
				vec3 const r = cam.right();
				vec3 const p0 = cam.position();
				cam.manipulator_translate_in_plane({ 1.0f, 0.0f });
				assert_cgp_no_msg(is_equal(cam.position() - p0, r));
			}
			{
				CAM cam;
				vec3 const u = cam.up();
				vec3 const p0 = cam.position();
				cam.manipulator_translate_in_plane({ 0.0f, 1.0f });
				assert_cgp_no_msg(is_equal(cam.position() - p0, u));
			}
			{
				CAM cam;
				vec3 const f = cam.front();
				vec3 const p0 = cam.position();
				cam.manipulator_translate_front(1.0f);
				assert_cgp_no_msg(is_equal(cam.position() - p0, f));
			}
		}

		// look_at must produce a front vector pointing from eye to center, and a
		//  position equal to eye. Same expected values for all 4 cameras.
		void check_look_at_results(camera_generic_base const& cam, vec3 const& eye, vec3 const& expected_front)
		{
			assert_cgp_no_msg(is_equal(cam.front(), expected_front));
			assert_cgp_no_msg(is_equal(cam.position(), eye));
		}
	}

	void test_camera_model()
	{
		float const sqrt2 = std::sqrt(2.0f);

		// ---------- sign convention is uniform across the 4 camera models ----------
		check_translation_convention<camera_orbit>();
		check_translation_convention<camera_orbit_euler>();
		check_translation_convention<camera_first_person>();
		check_translation_convention<camera_first_person_euler>();

		// ---------- look_at coherence (front + position) ----------
		// Cameras with an explicit up parameter
		{
			camera_orbit cam;
			cam.look_at({ 1,0,0 }, { 0,0,0 }, { 0,0,1 });
			check_look_at_results(cam, { 1,0,0 }, { -1,0,0 });
			cam.look_at({ 0,1,0 }, { 0,0,0 }, { 0,0,1 });
			check_look_at_results(cam, { 0,1,0 }, { 0,-1,0 });
			cam.look_at({ 1,1,0 }, { 0,0,0 }, { 0,0,1 });
			check_look_at_results(cam, { 1,1,0 }, { -1 / sqrt2, -1 / sqrt2, 0 });
		}
		{
			camera_first_person cam;
			cam.look_at({ 1,0,0 }, { 0,0,0 }, { 0,0,1 });
			check_look_at_results(cam, { 1,0,0 }, { -1,0,0 });
			cam.look_at({ 0,1,0 }, { 0,0,0 }, { 0,0,1 });
			check_look_at_results(cam, { 0,1,0 }, { 0,-1,0 });
			cam.look_at({ 1,1,0 }, { 0,0,0 }, { 0,0,1 });
			check_look_at_results(cam, { 1,1,0 }, { -1 / sqrt2, -1 / sqrt2, 0 });
		}
		// Euler cameras: look_at without up parameter, same expected results
		{
			camera_orbit_euler cam;
			cam.look_at({ 1,0,0 }, { 0,0,0 });
			check_look_at_results(cam, { 1,0,0 }, { -1,0,0 });
			cam.look_at({ 0,1,0 }, { 0,0,0 });
			check_look_at_results(cam, { 0,1,0 }, { 0,-1,0 });
			cam.look_at({ 1,1,0 }, { 0,0,0 });
			check_look_at_results(cam, { 1,1,0 }, { -1 / sqrt2, -1 / sqrt2, 0 });
		}
		{
			camera_first_person_euler cam;
			cam.look_at({ 1,0,0 }, { 0,0,0 });
			check_look_at_results(cam, { 1,0,0 }, { -1,0,0 });
			cam.look_at({ 0,1,0 }, { 0,0,0 });
			check_look_at_results(cam, { 0,1,0 }, { 0,-1,0 });
			cam.look_at({ 1,1,0 }, { 0,0,0 });
			check_look_at_results(cam, { 1,1,0 }, { -1 / sqrt2, -1 / sqrt2, 0 });
		}
	}
}
