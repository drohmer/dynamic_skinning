#include "test_timer.hpp"

#include "cgp/01_base/base.hpp"
#include "cgp/17_timer/timer_basic/timer_basic.hpp"
#include "cgp/17_timer/timer_event_periodic/timer_event_periodic.hpp"
#include "cgp/17_timer/timer_fps/timer_fps.hpp"
#include "cgp/17_timer/timer_interval/timer_interval.hpp"

#include <cmath>

using namespace cgp;

namespace
{
	// Deterministic fake clock shared by all timers while a test runs.
	//  All chosen time values are exact in binary floating point (multiples of
	//  0.25) so the comparisons below are exact, not just approximate.
	double g_now = 0.0;
	double fake_clock() { return g_now; }

	bool approx(float a, float b, float eps = 1e-5f) { return std::abs(a - b) < eps; }
}

namespace cgp_test
{
	void test_timer()
	{
		timer_set_time_source(fake_clock);

		// ---------- timer_basic: dt, scale, stop ----------
		{
			g_now = 0.0;
			timer_basic t;
			t.start();                       // time_previous := 0

			g_now = 0.5;
			float dt = t.update();           // dt = 0.5
			assert_cgp_no_msg(approx(dt, 0.5f));
			assert_cgp_no_msg(approx(t.t, 0.5f));

			t.scale = 2.0f;
			g_now = 1.0;
			dt = t.update();                 // dt = scale*(1.0-0.5) = 1.0
			assert_cgp_no_msg(approx(dt, 1.0f));
			assert_cgp_no_msg(approx(t.t, 1.5f));

			t.stop();
			g_now = 5.0;
			dt = t.update();                 // stopped -> no time advance
			assert_cgp_no_msg(approx(dt, 0.0f));
		}

		// ---------- timer_event_periodic: event flag and wrap ----------
		{
			g_now = 0.0;
			timer_event_periodic t(1.0f);
			t.start();

			g_now = 0.5; t.update();
			assert_cgp_no_msg(t.event == false);          // 0.5 < period
			g_now = 1.0; t.update();
			assert_cgp_no_msg(t.event == true);           // crossed the period
			assert_cgp_no_msg(approx(t.t_periodic, 0.0f)); // 1.0 - period
			g_now = 1.5; t.update();
			assert_cgp_no_msg(t.event == false);          // back to 0.5 into the period
		}

		// ---------- timer_fps: fps = frames / period, counter reset ----------
		{
			g_now = 0.0;
			timer_fps t(1.0f);               // event period = 1 s
			t.start();
			// Four frames within one period; the last one clearly crosses 1.0.
			g_now = 0.25; t.update();
			g_now = 0.5;  t.update();
			g_now = 0.75; t.update();
			g_now = 1.5;  t.update();        // event fires -> fps computed
			assert_cgp_no_msg(t.fps == 4);   // int(4 frames / 1.0 s)
		}

		// ---------- timer_interval: wrap into [t_min, t_max) ----------
		{
			g_now = 0.0;
			timer_interval t(0.0f, 1.0f);    // t starts at t_min = 0
			t.start();

			g_now = 0.5; t.update();
			assert_cgp_no_msg(approx(t.t, 0.5f));
			g_now = 1.0; t.update();         // reaches t_max -> wraps to 0
			assert_cgp_no_msg(approx(t.t, 0.0f));
			g_now = 1.5; t.update();
			assert_cgp_no_msg(approx(t.t, 0.5f));
		}

		// ---------- timer_interval: a dt larger than the range is rejected ----------
		// The fixed behavior is to undo the step and return 0 rather than letting a
		//  huge dt skip arbitrarily far through the interval.
		{
			g_now = 0.0;
			timer_interval t(0.0f, 1.0f);
			t.start();

			g_now = 2.0;                     // dt = 2.0 > (t_max - t_min) = 1.0
			float dt = t.update();
			assert_cgp_no_msg(approx(dt, 0.0f));   // step rejected
			assert_cgp_no_msg(approx(t.t, 0.0f));  // t left unchanged
		}

		timer_set_time_source(nullptr);      // restore the real monotonic clock
	}
}
