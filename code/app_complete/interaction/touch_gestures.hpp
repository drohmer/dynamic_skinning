#pragma once

#include "cgp/cgp.hpp"

// Two-finger gestures on touch screens (tablet), web version only (emscripten).
// One-finger touches are already converted into left mouse clicks by the GLFW emscripten port;
// this module adds the pinch gesture (zoom) and the two-finger translation (pan), and tells the scene when several fingers are down,
// so that the single-finger action (camera rotation or bone edit) is suspended during a pinch.
// On desktop, the functions do nothing (zoom uses the mouse scroll / two-finger trackpad scroll).
struct touch_gesture_state {
    bool multi_touch = false;     // true from the second finger down until all fingers are lifted
    float zoom_factor = 1.0f;     // accumulated pinch ratio (previous distance / current distance), consumed by the scene
    float pinch_distance = 0.0f;  // distance between the two first fingers (0: no pinch in progress)
    float pinch_center[2] = {0.0f, 0.0f};  // middle of the two first fingers (canvas CSS pixels)
    float pan[2] = {0.0f, 0.0f};  // accumulated motion of the middle of the fingers (relative coordinates [-1,1], y up)
};

// Register the touch callbacks on the canvas (no-op on desktop)
void touch_gestures_initialize(touch_gesture_state& state);

// Return the accumulated zoom factor since the last call, and reset it to 1
float touch_gestures_consume_zoom(touch_gesture_state& state);

// Return the accumulated two-finger translation since the last call (relative coordinates), and reset it
cgp::vec2 touch_gestures_consume_pan(touch_gesture_state& state);
