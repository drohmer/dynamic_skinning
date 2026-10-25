#pragma once

namespace cgp_test
{
	// Tests on the timer module (module 17, GL-free since the clock is abstracted).
	// A deterministic fake time source (timer_set_time_source) drives the real
	//  update() path of every timer:
	//  - timer_basic: dt computation, scale, stop
	//  - timer_event_periodic: event flag and periodic wrap
	//  - timer_fps: fps = frames / period, counter reset
	//  - timer_interval: wrap into [t_min,t_max) and rejection of an oversized dt
	void test_timer();
}
