#pragma once

#include "cgp/cgp.hpp"


// Tracks angular velocity history from successive quaternion rotations.
// Stores a buffer of ~300+ smoothed angular velocity samples for convolution-based oscillation filtering.
struct angular_velocity_tracker
{
    angular_velocity_tracker();

    // Add a new rotation sample at the given time.
    void add(cgp::quaternion const& new_rotation, float time);

    cgp::quaternion last_rotation;
    float last_time;
    int counter_initialization;

    cgp::vec3 avg_angular_velocity;

    // Temporal filter: new_speed = alpha*old_speed + (1-alpha)*current_speed
    float alpha_smoothing = 0.75f;

    // History of smoothed angular velocities (axis * angle_per_second)
    cgp::numarray<cgp::vec3> angular_velocity_history;
};


// ---- Implementation (header-only) ----

inline angular_velocity_tracker::angular_velocity_tracker()
    : last_rotation({0,0,0,1}), last_time(0), counter_initialization(0), avg_angular_velocity({0,0,0})
{
    // Pre-fill buffer so the oscillation filter is active immediately
    angular_velocity_history.resize(400);
    angular_velocity_history.fill({0,0,0});
}

inline void angular_velocity_tracker::add(cgp::quaternion const& new_rotation, float time)
{
    if (std::abs(time - last_time) < 1e-4f)
        return;

    // Ensure quaternion hemisphere consistency: q and -q represent the same
    // rotation, but a sign flip between frames causes the tracker to see a
    // ~2π delta instead of a small one.  Always pick the shortest path.
    cgp::quaternion nr = new_rotation;
    float qd = nr.x * last_rotation.x + nr.y * last_rotation.y
             + nr.z * last_rotation.z + nr.w * last_rotation.w;
    if (qd < 0.0f)
        nr = {-nr.x, -nr.y, -nr.z, -nr.w};

    if (counter_initialization >= 1) {
        cgp::quaternion const q = nr * cgp::conjugate(last_rotation);

        // Extract axis-angle from quaternion
        cgp::vec3 n_sin = cgp::vec3(q.x, q.y, q.z);
        float s = cgp::norm(n_sin);
        cgp::vec3 axis;
        float angle;
        if (s < 1e-6f) {
            axis = {1, 0, 0};
            angle = 0.0f;
        } else {
            angle = 2.0f * std::atan2(s, q.w);
            axis = n_sin / s;
        }

        if (std::isnan(axis.x) || std::isnan(axis.y) || std::isnan(axis.z)) {
            axis = {1, 0, 0};
            angle = 0.0f;
        }

        cgp::vec3 new_angular_velocity = axis * angle / (time - last_time);

        // Clamp to reject spurious spikes (e.g. from mouse interaction jitter)
        static constexpr float MAX_ANGULAR_VELOCITY = 50.0f; // rad/s
        float const nv = cgp::norm(new_angular_velocity);
        if (nv > MAX_ANGULAR_VELOCITY)
            new_angular_velocity = new_angular_velocity * (MAX_ANGULAR_VELOCITY / nv);

        avg_angular_velocity = alpha_smoothing * avg_angular_velocity + (1.0f - alpha_smoothing) * new_angular_velocity;
    }

    if (counter_initialization == 1) counter_initialization = 2;
    if (counter_initialization == 0) counter_initialization = 1;

    last_rotation = nr;
    last_time = time;

    angular_velocity_history.push_back(avg_angular_velocity);

    // Keep buffer manageable
    if (angular_velocity_history.size() > 1000) {
        cgp::numarray<cgp::vec3> temp;
        temp.resize(400);
        for (int k = 0; k < 400; ++k) {
            temp[k] = angular_velocity_history[angular_velocity_history.size() - 400 + k];
        }
        angular_velocity_history = temp;
    }
}
