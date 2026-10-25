#pragma once

namespace cgp
{

	class timer_basic
	{
	public:
		timer_basic();
		float update();
		void start();
		void stop();

		float t;
		float scale;

	protected:
		bool running;
		double time_previous;
	};

	// Time source (in seconds) shared by all timers. Defaults to a steady
	//  monotonic clock; tests may override it for deterministic timing
	//  (pass nullptr to timer_set_time_source to restore the default).
	double timer_current_time();
	void   timer_set_time_source(double (*source)());
}