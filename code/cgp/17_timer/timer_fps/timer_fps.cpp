#include "timer_fps.hpp"


namespace cgp
{
    timer_fps::timer_fps(float update_fps_period)
        :timer_event_periodic(update_fps_period), fps(0), counter(0)
    {}

    float timer_fps::update()
    {
        if(running) ++counter;

        float const dt = timer_event_periodic::update();
        if (event)
        {
            if(event_period > 1e-6f)
                fps = int(counter / event_period);
            counter = 0;
        }

        return dt;
    }



}