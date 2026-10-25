#include "touch_gestures.hpp"

#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>

static EM_BOOL touch_callback(int event_type, EmscriptenTouchEvent const* event, void* user_data)
{
    touch_gesture_state& state = *static_cast<touch_gesture_state*>(user_data);
    bool const is_end = (event_type == EMSCRIPTEN_EVENT_TOUCHEND || event_type == EMSCRIPTEN_EVENT_TOUCHCANCEL);

    // Fingers still on the screen (the ones lifted by a touchend are flagged as changed)
    int N_down = 0;
    float x[2] = {0, 0}, y[2] = {0, 0};
    for (int k = 0; k < event->numTouches; ++k) {
        EmscriptenTouchPoint const& t = event->touches[k];
        if (is_end && t.isChanged)
            continue;
        if (N_down < 2) {
            x[N_down] = float(t.targetX);
            y[N_down] = float(t.targetY);
        }
        ++N_down;
    }

    if (N_down >= 2) {
        state.multi_touch = true;
        float const d = std::sqrt((x[1] - x[0]) * (x[1] - x[0]) + (y[1] - y[0]) * (y[1] - y[0]));
        if (state.pinch_distance > 0.0f && d > 1.0f)
            state.zoom_factor *= state.pinch_distance / d;  // fingers apart -> closer camera
        state.pinch_distance = d;
    }
    else {
        state.pinch_distance = 0.0f;
        if (N_down == 0)
            state.multi_touch = false;
    }

    // Let the GLFW port also receive the event (one-finger touch = left click)
    return EM_FALSE;
}

void touch_gestures_initialize(touch_gesture_state& state)
{
    char const* target = "#canvas";
    emscripten_set_touchstart_callback(target, &state, true, touch_callback);
    emscripten_set_touchmove_callback(target, &state, true, touch_callback);
    emscripten_set_touchend_callback(target, &state, true, touch_callback);
    emscripten_set_touchcancel_callback(target, &state, true, touch_callback);
}

#else

void touch_gestures_initialize(touch_gesture_state&) {}

#endif


float touch_gestures_consume_zoom(touch_gesture_state& state)
{
    float const f = state.zoom_factor;
    state.zoom_factor = 1.0f;
    return f;
}
