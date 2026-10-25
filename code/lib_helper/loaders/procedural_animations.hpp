#pragma once

#include "cgp/cgp.hpp"
#include "animated_character/skeleton_structure/skeleton_structure.hpp"
#include "animated_character/skeleton_animation/skeleton_animation.hpp"

#include <functional>
#include <string>


// A pose of the skeleton: one local matrix per joint
using pose_structure = cgp::numarray<cgp::mat4>;

// Return the index of the joint with the given name (error if not found).
int find_joint(skeleton_structure const& skeleton, std::string const& name);

// Rest pose (local matrices) of the skeleton
pose_structure rest_pose(skeleton_structure const& skeleton);

// Return a copy of the pose where the given joint is additionally rotated by `angle`
// around `axis_world` (axis given in world coordinates, converted using the rest pose).
pose_structure rotate_joint(skeleton_structure const& skeleton, pose_structure const& pose,
                            int joint, cgp::vec3 const& axis_world, float angle);

// Build a "pose + hold" animation: each pose k is held during hold[k] seconds, then a
// smooth (ease-in-out) transition of move[k] seconds leads to the next pose
// (move has one entry less than poses). For a seamless loop, the last pose should be
// equal to the first one.
skeleton_animation_structure build_pose_hold_animation(skeleton_structure const& skeleton,
                                                       cgp::numarray<pose_structure> const& poses,
                                                       cgp::numarray<float> const& hold,
                                                       cgp::numarray<float> const& move);

// Timing and amplitude of a left/right swing
struct swing_parameters {
    float angle_left  = 0.6f;   // (rad)
    float angle_right = 0.6f;   // (rad)
    float move_left   = 0.8f;   // (s) continuous motion to the left
    float hold_left   = 2.5f;   // (s) pause
    float move_right  = 0.8f;   // (s) rotation to the right
    float hold_right  = 3.5f;   // (s) pause, time to watch the oscillation
    float move_back   = 0.8f;   // (s) back to the rest pose
};

// Starting at rest, the pivot joint rotates to the left, pauses, rotates to the right, pauses,
// then comes back to rest. The rotation axis is horizontal and orthogonal to `right_direction`
// (world coordinates); `tip` is a joint further along the chain, used to orient the rotation.
skeleton_animation_structure build_left_right_swing(skeleton_structure const& skeleton, int pivot, int tip,
                                                    cgp::vec3 const& right_direction, swing_parameters const& param);

// Giraffe (teaser of the paper, front view): the neck swings to the left, then to the right.
skeleton_animation_structure build_giraffe_neck_swing(skeleton_structure const& skeleton);

// Clown (jack-in-the-box, figure of the paper): the body swings on its spring to the left, then to
// the right, then twists around the spring axis on each side so that the hands oscillate.
skeleton_animation_structure build_clown_swing(skeleton_structure const& skeleton);

// Spot: the head nods up and down (neck), pauses, then twists on each side so that
// the ears oscillate. Expects the skeleton in its rest pose.
skeleton_animation_structure build_spot_head_nod_twist(skeleton_structure const& skeleton);

// Snail: the head (rotation of Bone_008, parent of joint 10) goes up, pauses, comes back, then down, pauses,
// with a slight twist around the neck so that the eyes on their stalks oscillate. The tail (parent of
// joint 9) goes up with the head, then swings sideways when the head goes down.
skeleton_animation_structure build_snail_head_up_down(skeleton_structure const& skeleton);

// Build an animation by sampling a continuous pose function pose_at(t), t in [0, duration],
// every dt seconds (for motions that are not "pose + hold", like a periodic flapping).
skeleton_animation_structure build_sampled_animation(skeleton_structure const& skeleton,
                                                     std::function<pose_structure(float)> const& pose_at,
                                                     float duration, float dt);

// Dragon: the wings flap almost all the time at the given frequency (Hz, typically the frequency f of
// the oscillations, so that the wing tips resonate), with a short glide at the end of the loop.
// Meanwhile, the arms reach forward (one after the other, then both), the tail swings and lifts,
// the head turns, nods and opens the mouth, and the legs kick forward (quick moves and pauses:
// hands, tail, head and feet oscillate).
// Expects the skeleton in its rest pose.
skeleton_animation_structure build_dragon_flight(skeleton_structure const& skeleton, float flap_frequency);

// Cthulhu: the head, the wings, the arms and the legs move together (quick moves and pauses): wings up,
// arms forward, leg kicks and head nod, then the head shakes ("no") while the face tentacles swing.
// Small angles: the large wings and long tentacles quickly self-intersect. Best seen with the per-vertex
// bend weights (only the floppy parts deform: wing membranes, tentacles, spikes). Expects the rest pose.
skeleton_animation_structure build_cthulhu_head_shake(skeleton_structure const& skeleton);

// Train (figure of the paper, delayed LBS): the engine (root joint) goes over bumps of the track, with a
// small roll on alternate sides. With the delayed LBS, each bump propagates from the engine to the last wagon.
// Expects the skeleton in its rest pose.
skeleton_animation_structure build_train_bumps(skeleton_structure const& skeleton);
