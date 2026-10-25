#include "timer_basic.hpp"

#include <chrono>

namespace cgp
{

namespace
{
	double default_time_source()
	{
		using clock = std::chrono::steady_clock;
		static clock::time_point const origin = clock::now();
		return std::chrono::duration<double>(clock::now() - origin).count();
	}
	double (*g_time_source)() = default_time_source;
}

double timer_current_time() { return g_time_source(); }
void   timer_set_time_source(double (*source)()) { g_time_source = source ? source : default_time_source; }

timer_basic::timer_basic()
	:t(0),scale(1.0f),running(true),time_previous(timer_current_time())
{}
float timer_basic::update()
{
	if(!running)
        return 0.0f;

    const double time_current = timer_current_time();
    const float dt = static_cast<float>(scale*(time_current-time_previous));

    time_previous = time_current;
    t += dt;

    return dt;
}
void timer_basic::start()
{
    running = true;
    time_previous = timer_current_time();
}
void timer_basic::stop()
{
    running = false;
}


}