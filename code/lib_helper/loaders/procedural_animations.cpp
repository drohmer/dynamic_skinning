#include "procedural_animations.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace cgp;


int find_joint(skeleton_structure const& skeleton, std::string const& name)
{
    for (int k = 0; k < skeleton.size(); ++k)
        if (skeleton.joint_name[k] == name)
            return k;
    error_cgp("Joint not found in skeleton: " + name);
    return -1;
}


pose_structure rest_pose(skeleton_structure const& skeleton)
{
    return skeleton.joint_matrix_local;
}


pose_structure rotate_joint(skeleton_structure const& skeleton, pose_structure const& pose,
                            int joint, vec3 const& axis_world, float angle)
{
    // Express the world axis in the joint frame (rest pose): axis_local = R_global^T * axis_world
    mat3 const R = skeleton.joint_matrix_global[joint].get_block_linear();
    vec3 axis_local;
    for (int c = 0; c < 3; ++c)
        axis_local[c] = R(0, c) * axis_world.x + R(1, c) * axis_world.y + R(2, c) * axis_world.z;
    axis_local = normalize(axis_local);

    pose_structure result = pose;
    result[joint] = pose[joint] * mat4::build_rotation_from_axis_angle(axis_local, angle);
    return result;
}


skeleton_animation_structure build_pose_hold_animation(skeleton_structure const& skeleton,
                                                       numarray<pose_structure> const& poses,
                                                       numarray<float> const& hold,
                                                       numarray<float> const& move)
{
    assert_cgp(poses.size() >= 2 && hold.size() == poses.size() && move.size() == poses.size() - 1,
               "Need at least 2 poses, one hold duration per pose and one move duration per transition");
    int const N_joints = skeleton.size();

    // Interpolation between two joint matrices (nlerp on rotation, lerp on translation)
    auto interpolate = [](mat4 const& M0, mat4 const& M1, float alpha) {
        rotation_transform const R0 = rotation_transform::from_matrix(M0.get_block_linear());
        rotation_transform const R1 = rotation_transform::from_matrix(M1.get_block_linear());
        vec3 const t0 = M0.get_block_translation();
        vec3 const t1 = M1.get_block_translation();
        return mat4::build_affine(rotation_transform::lerp(R0, R1, alpha).matrix(), (1 - alpha) * t0 + alpha * t1);
    };

    // One channel per joint, all sharing the same key times
    skeleton_animation_structure anim;
    anim.joint_index.resize(N_joints);
    anim.times.resize(N_joints);
    anim.matrix.resize(N_joints);
    for (int j = 0; j < N_joints; ++j)
        anim.joint_index[j] = j;

    auto add_keyframe = [&](float time, pose_structure const& pose) {
        for (int j = 0; j < N_joints; ++j) {
            anim.times[j].push_back(time);
            anim.matrix[j].push_back(pose[j]);
        }
    };

    // A pose with hold>0 gets two keyframes (start and end of hold), otherwise a single one.
    // Transitions are sampled with a smooth ease-in-out (zero velocity at both ends),
    // so that each motion is perceived as a single continuous gesture.
    int const N_substeps = 12;
    float t = 0.0f;
    int N_poses = poses.size();
    for (int p = 0; p < N_poses; ++p) {
        add_keyframe(t, poses[p]);
        if (hold[p] > 0.0f) {
            t += hold[p];
            add_keyframe(t, poses[p]);
        }
        if (p < N_poses - 1) {
            for (int k = 1; k < N_substeps; ++k) {
                float const u = k / float(N_substeps);
                float const alpha = u * u * (3.0f - 2.0f * u);  // smoothstep
                pose_structure pose(N_joints);
                for (int j = 0; j < N_joints; ++j)
                    pose[j] = interpolate(poses[p][j], poses[p + 1][j], alpha);
                add_keyframe(t + u * move[p], pose);
            }
            t += move[p];
        }
    }
    anim.time_max = t;

    return anim;
}


skeleton_animation_structure build_left_right_swing(skeleton_structure const& skeleton, int pivot, int tip,
                                                    vec3 const& right_direction, swing_parameters const& param)
{
    // Rotation around the horizontal axis orthogonal to the left-right direction
    vec3 const axis = normalize(cross(right_direction, vec3{0, 1, 0}));

    // Sign: a positive angle moves the tip toward the right
    auto position = [&](int j) { return skeleton.joint_matrix_global[j].get_block_translation(); };
    vec3 const u_tip = position(tip) - position(pivot);
    vec3 const u_tip_rotated = rotation_transform::from_axis_angle(axis, param.angle_right) * u_tip;
    float const sign = (dot(u_tip_rotated - u_tip, right_direction) > 0.0f) ? 1.0f : -1.0f;

    pose_structure const rest  = rest_pose(skeleton);
    pose_structure const left  = rotate_joint(skeleton, rest, pivot, axis, -sign * param.angle_left);
    pose_structure const right = rotate_joint(skeleton, rest, pivot, axis,  sign * param.angle_right);

    // upright -> left -> pause -> right -> pause -> back upright (loop)
    numarray<pose_structure> poses = { rest, left, right, rest };
    numarray<float> hold = { 0.0f, param.hold_left, param.hold_right, 0.0f };
    numarray<float> move = { param.move_left, param.move_right, param.move_back };
    return build_pose_hold_animation(skeleton, poses, hold, move);
}


skeleton_animation_structure build_girafe_neck_swing(skeleton_structure const& skeleton)
{
    swing_parameters param;
    param.angle_left  = 35.0f * Pi / 180.0f;
    param.angle_right = 35.0f * Pi / 180.0f;
    param.move_left   = 0.5f;
    param.hold_left   = 2.5f;
    param.move_right  = 0.5f;
    param.hold_right  = 3.5f;
    param.move_back   = 0.5f;

    int const neck = find_joint(skeleton, "gi_neckC7");
    int const head = find_joint(skeleton, "gi_head");
    int const L_shoulder = find_joint(skeleton, "gi_L_shoulder");
    int const R_shoulder = find_joint(skeleton, "gi_R_shoulder");

    // Right = toward the R_shoulder side (screen right in the front view)
    vec3 const right_direction = normalize(skeleton.joint_matrix_global[R_shoulder].get_block_translation()
                                         - skeleton.joint_matrix_global[L_shoulder].get_block_translation());
    return build_left_right_swing(skeleton, neck, head, right_direction, param);
}


skeleton_animation_structure build_clown_swing(skeleton_structure const& skeleton)
{
    // Tunable parameters
    float const angle_swing = 35.0f * Pi / 180.0f;  // body swing to the left / right
    float const angle_twist = 50.0f * Pi / 180.0f;  // body twist around the spring axis (hands)
    float const move_swing  = 0.5f;                  // (s) each swing motion
    float const hold_left   = 2.5f;                  // (s) pause on the left
    float const hold_right  = 3.5f;                  // (s) pause on the right
    float const move_back   = 0.5f;                  // (s) back to rest
    float const hold_rest   = 1.5f;                  // (s) pause at rest between swing and twist
    float const move_twist  = 0.3f;                  // (s) each twist motion
    float const hold_twist  = 2.5f;                  // (s) pause after each twist (hands oscillation)

    // The body pivots at the top of the spring (joint4), the head is the end of the chain (joint6)
    int const spring_top = find_joint(skeleton, "joint4");
    int const head       = find_joint(skeleton, "joint6");

    auto position = [&](int j) { return skeleton.joint_matrix_global[j].get_block_translation(); };

    // The clown faces +z: right = +x (screen right in the front view)
    vec3 const right_direction = {1, 0, 0};
    vec3 const axis_swing = normalize(cross(right_direction, vec3{0, 1, 0}));
    vec3 const axis_twist = normalize(position(head) - position(spring_top));  // body axis

    // Sign: a positive swing angle moves the head toward the right
    vec3 const u_head = position(head) - position(spring_top);
    vec3 const u_head_rotated = rotation_transform::from_axis_angle(axis_swing, angle_swing) * u_head;
    float const sign = (dot(u_head_rotated - u_head, right_direction) > 0.0f) ? 1.0f : -1.0f;

    pose_structure const rest    = rest_pose(skeleton);
    pose_structure const left    = rotate_joint(skeleton, rest, spring_top, axis_swing, -sign * angle_swing);
    pose_structure const right   = rotate_joint(skeleton, rest, spring_top, axis_swing,  sign * angle_swing);
    pose_structure const twist_1 = rotate_joint(skeleton, rest, spring_top, axis_twist,  angle_twist);
    pose_structure const twist_2 = rotate_joint(skeleton, rest, spring_top, axis_twist, -angle_twist);

    // swing: rest -> left -> right -> rest (pause), then twist: one side -> rest -> other side -> rest (loop)
    // (the twist always starts from rest, so that the large angle never doubles between both sides)
    numarray<pose_structure> poses = { rest, left, right, rest, twist_1, rest, twist_2, rest };
    numarray<float> hold = { 0.0f, hold_left, hold_right, hold_rest, hold_twist, hold_rest, hold_twist, 0.0f };
    numarray<float> move = { move_swing, move_swing, move_back, move_twist, move_twist, move_twist, move_twist };
    return build_pose_hold_animation(skeleton, poses, hold, move);
}


skeleton_animation_structure build_spot_head_nod_twist(skeleton_structure const& skeleton)
{
    // Tunable parameters
    float const angle_nod   = 12.0f * Pi / 180.0f;  // head up / down (neck rotation)
    float const angle_twist = 40.0f * Pi / 180.0f;  // head twist around its own axis (ears flapping)
    float const move_nod    = 0.35f;                 // (s) each up/down motion
    float const hold_nod    = 0.4f;                  // (s) short pause between up/down motions
    float const move_twist  = 0.25f;                 // (s) each twist motion
    float const hold_twist  = 1.5f;                  // (s) pause after a twist (ears oscillation)
    float const hold_rest   = 2.0f;                  // (s) pause in the rest pose

    int const neck     = find_joint(skeleton, "neck");
    int const head     = find_joint(skeleton, "head");
    int const head_end = find_joint(skeleton, "head-end");
    int const arm_L    = find_joint(skeleton, "arm_L");
    int const arm_R    = find_joint(skeleton, "arm_R");

    auto position = [&](int j) { return skeleton.joint_matrix_global[j].get_block_translation(); };

    vec3 const axis_nod   = normalize(position(arm_L) - position(arm_R));     // lateral axis
    vec3 const axis_twist = normalize(position(head_end) - position(head));  // head axis

    pose_structure const rest    = rest_pose(skeleton);
    pose_structure const up      = rotate_joint(skeleton, rest, neck, axis_nod,  angle_nod);
    pose_structure const down    = rotate_joint(skeleton, rest, neck, axis_nod, -angle_nod);
    pose_structure const twist_L = rotate_joint(skeleton, rest, head, axis_twist,  angle_twist);
    pose_structure const twist_R = rotate_joint(skeleton, rest, head, axis_twist, -angle_twist);

    // rest -> up/down oscillation -> rest (pause) -> twist one side -> twist other side -> rest (loop)
    numarray<pose_structure> poses = { rest, up, down, up, down, rest, twist_L, twist_R, rest };
    numarray<float> hold = { hold_rest, hold_nod, hold_nod, hold_nod, hold_nod, hold_rest, hold_twist, hold_twist, 0.0f };
    numarray<float> move = { move_nod, move_nod, move_nod, move_nod, move_nod, move_twist, move_twist, move_twist };
    return build_pose_hold_animation(skeleton, poses, hold, move);
}


skeleton_animation_structure build_snail_head_up_down(skeleton_structure const& skeleton)
{
    // Tunable parameters
    float const angle_up    = 10.0f * Pi / 180.0f;  // head up (small: the eyes must not enter the shell)
    float const angle_down  = 25.0f * Pi / 180.0f;  // head down
    float const twist_up    =  8.0f * Pi / 180.0f;  // twist around the neck combined with up (small: eyes vs shell)
    float const twist_down  = 20.0f * Pi / 180.0f;  // twist around the neck combined with down
    float const tail_lift   = 12.0f * Pi / 180.0f;  // tail goes up while the head goes up
    float const tail_side   = 15.0f * Pi / 180.0f;  // tail swings sideways while the head goes down
    float const move        = 0.35f;                 // (s) each motion
    float const hold        = 2.5f;                  // (s) pause after a motion (eyes oscillation)
    float const hold_rest   = 1.5f;                  // (s) pause in the rest pose

    // Bone_008 is the neck: rotating it moves the head (Bone_009, joint 10) and both eye stalks.
    int const neck  = find_joint(skeleton, "Bone_008");
    int const head  = find_joint(skeleton, "Bone_009");

    auto position = [&](int j) { return skeleton.joint_matrix_global[j].get_block_translation(); };

    vec3 const axis_twist = normalize(position(head) - position(neck));  // neck axis
    // Lateral axis: orthogonal to the neck and to the vertical
    vec3 const axis_lateral = normalize(cross(axis_twist, vec3{0, 1, 0}));

    // Sign: a positive angle around the lateral axis moves the head upward
    vec3 const u = position(head) - position(neck);
    vec3 const u_rotated = rotation_transform::from_axis_angle(axis_lateral, angle_up) * u;
    float const sign = (u_rotated.y > u.y) ? 1.0f : -1.0f;

    // Tail: Bone_005 is the parent of the tail tip (Bone_006, joint 9)
    int const tail     = find_joint(skeleton, "Bone_005");
    int const tail_tip = find_joint(skeleton, "Bone_006");
    vec3 const v = position(tail_tip) - position(tail);
    vec3 const axis_tail_lateral = normalize(cross(v, vec3{0, 1, 0}));
    vec3 const v_rotated = rotation_transform::from_axis_angle(axis_tail_lateral, tail_lift) * v;
    float const sign_tail = (v_rotated.y > v.y) ? 1.0f : -1.0f;

    pose_structure const rest = rest_pose(skeleton);
    pose_structure up   = rotate_joint(skeleton, rotate_joint(skeleton, rest, neck, axis_lateral,  sign * angle_up),
                                       neck, axis_twist,  twist_up);
    pose_structure down = rotate_joint(skeleton, rotate_joint(skeleton, rest, neck, axis_lateral, -sign * angle_down),
                                       neck, axis_twist, -twist_down);
    up   = rotate_joint(skeleton, up,   tail, axis_tail_lateral, sign_tail * tail_lift);
    down = rotate_joint(skeleton, down, tail, vec3{0, 1, 0}, tail_side);

    // rest -> up (pause) -> rest (pause) -> down (pause) -> rest (pause, loop)
    // (going through rest avoids a fast up -> down motion, where the lagging eyes would enter the shell)
    numarray<pose_structure> poses = { rest, up, rest, down, rest };
    numarray<float> hold_time = { hold_rest, hold, hold_rest, hold, 0.0f };
    numarray<float> move_time = { move, move, move, move };
    return build_pose_hold_animation(skeleton, poses, hold_time, move_time);
}


skeleton_animation_structure build_sampled_animation(skeleton_structure const& skeleton,
                                                     std::function<pose_structure(float)> const& pose_at,
                                                     float duration, float dt)
{
    int const N_joints = skeleton.size();

    skeleton_animation_structure anim;
    anim.joint_index.resize(N_joints);
    anim.times.resize(N_joints);
    anim.matrix.resize(N_joints);
    for (int j = 0; j < N_joints; ++j)
        anim.joint_index[j] = j;

    int const N_samples = std::max(1, int(std::ceil(duration / dt)));
    for (int k = 0; k <= N_samples; ++k) {
        float const t = duration * k / float(N_samples);
        pose_structure const pose = pose_at(t);
        for (int j = 0; j < N_joints; ++j) {
            anim.times[j].push_back(t);
            anim.matrix[j].push_back(pose[j]);
        }
    }
    anim.time_max = duration;

    return anim;
}


// Piecewise curve reaching value[k] at time[k], with smooth (ease-in-out) transitions,
// constant before the first and after the last key
static float smooth_curve(float t, std::vector<float> const& time, std::vector<float> const& value)
{
    if (t <= time.front()) return value.front();
    for (size_t k = 0; k + 1 < time.size(); ++k) {
        if (t < time[k + 1]) {
            float const u = (t - time[k]) / (time[k + 1] - time[k]);
            float const alpha = u * u * (3.0f - 2.0f * u);
            return (1 - alpha) * value[k] + alpha * value[k + 1];
        }
    }
    return value.back();
}


// Sign s such that a rotation of s*angle (angle>0) around `axis` moves the tip joint toward `direction`
static float rotation_sign(skeleton_structure const& skeleton, int pivot, int tip, vec3 const& axis, vec3 const& direction)
{
    vec3 const p_pivot = skeleton.joint_matrix_global[pivot].get_block_translation();
    vec3 const u = skeleton.joint_matrix_global[tip].get_block_translation() - p_pivot;
    vec3 const u_rotated = rotation_transform::from_axis_angle(axis, 0.3f) * u;
    return (dot(u_rotated - u, direction) > 0.0f) ? 1.0f : -1.0f;
}


skeleton_animation_structure build_dragon_flight(skeleton_structure const& skeleton, float flap_frequency)
{
    // Tunable parameters
    float const angle_flap     = 15.0f * Pi / 180.0f;  // wing flapping amplitude (small: the oscillations resonate)
    float const duration_min   = 12.0f;                // (s) minimal duration of the loop
    float const glide          = 1.5f;                 // (s) glide (wings at rest) at the end of the loop
    float const angle_shoulder = 35.0f * Pi / 180.0f;  // arms reaching forward
    float const angle_elbow    = 45.0f * Pi / 180.0f;
    float const angle_tail     = 18.0f * Pi / 180.0f;  // tail swing, on each of the 3 first tail joints
    float const angle_tail_up  = 12.0f * Pi / 180.0f;  // tail lift, on each of the 3 first tail joints
    float const angle_head     = 30.0f * Pi / 180.0f;  // head turn (toward the right side, facing the camera)
    float const angle_nod      = 15.0f * Pi / 180.0f;  // head nod (down)
    float const angle_jaw      = 20.0f * Pi / 180.0f;  // mouth opening
    float const angle_hip      = 15.0f * Pi / 180.0f;  // legs kicking forward
    float const angle_knee     = 20.0f * Pi / 180.0f;

    float const f = std::max(flap_frequency, 0.2f);

    auto joint = [&](char const* name) { return find_joint(skeleton, name); };
    auto position = [&](int j) { return skeleton.joint_matrix_global[j].get_block_translation(); };

    int const wing_L = joint("L_Wing1"),  wing_L_tip = joint("L_Wing21");
    int const wing_R = joint("R_Wing23"), wing_R_tip = joint("R_Wing43");
    int const shoulder_L = joint("L_Arm3"),  elbow_L = joint("L_Arm4"),  hand_L = joint("L_Arm6");
    int const shoulder_R = joint("R_Arm18"), elbow_R = joint("R_Arm19"), hand_R = joint("R_Arm21");
    int const tail[3] = { joint("Tatil_1"), joint("Tatil_2"), joint("Tatil_3") };
    int const tail_tip = joint("Tatil_5");
    int const neck = joint("Joint_AAA"), snout = joint("Head_Joint5");
    int const jaw = joint("Jaw_Joints1"), jaw_tip = joint("joint104");
    int const hip_L = joint("L_Leg1"), knee_L = joint("L_Leg2"), foot_L = joint("L_Leg4");
    int const hip_R = joint("R_Leg1"), knee_R = joint("R_Leg2"), foot_R = joint("R_Leg4");

    // Body frame: vertical, forward (toward the head, horizontal), and left (L_ side)
    vec3 const up = {0, 1, 0};
    vec3 forward = position(snout) - position(0);
    forward.y = 0.0f;
    forward = normalize(forward);
    vec3 const left = normalize(position(wing_L) - position(wing_R));

    // Signs of the rotations, so that each motion goes in the intended direction
    float const sign_wing_L = rotation_sign(skeleton, wing_L, wing_L_tip, forward, up);
    float const sign_wing_R = rotation_sign(skeleton, wing_R, wing_R_tip, forward, up);
    float const sign_shoulder_L = rotation_sign(skeleton, shoulder_L, hand_L, left, forward);
    float const sign_shoulder_R = rotation_sign(skeleton, shoulder_R, hand_R, left, forward);
    float const sign_elbow_L = rotation_sign(skeleton, elbow_L, hand_L, left, forward);
    float const sign_elbow_R = rotation_sign(skeleton, elbow_R, hand_R, left, forward);
    float const sign_tail = rotation_sign(skeleton, tail[0], tail_tip, forward, left);
    float const sign_tail_up = rotation_sign(skeleton, tail[0], tail_tip, left, -forward);
    float const sign_head = rotation_sign(skeleton, neck, snout, up, -left);
    float const sign_nod = rotation_sign(skeleton, neck, snout, left, -up);
    float const sign_jaw = rotation_sign(skeleton, jaw, jaw_tip, left, -up);
    float const sign_hip_L = rotation_sign(skeleton, hip_L, foot_L, left, forward);
    float const sign_hip_R = rotation_sign(skeleton, hip_R, foot_R, left, forward);
    float const sign_knee_L = rotation_sign(skeleton, knee_L, foot_L, left, -forward);
    float const sign_knee_R = rotation_sign(skeleton, knee_R, foot_R, left, -forward);

    // The loop lasts an integer number of wing beats. The wings beat all the time, except during
    // a short glide at the end of the loop (with a smooth envelope over half a beat on each side).
    int const N_beats = int(std::ceil(duration_min * f));
    float const duration = N_beats / f;
    float const t_glide = (N_beats - std::max(1, int(std::round(glide * f)))) / f;

    // Secondary motions, overlapping the flapping: quick moves (0.3-0.5 s) and short pauses,
    // so that the hands, the tail and the head oscillate (curves of value -1..1 in time, see smooth_curve)
    std::vector<float> const arm_L_time   = { 1.0f, 1.3f, 2.3f, 2.6f,  6.8f, 7.1f, 8.3f, 8.6f };
    std::vector<float> const arm_L_value  = { 0,    1,    1,    0,     0,    1,    1,    0 };
    std::vector<float> const arm_R_time   = { 2.2f, 2.5f, 3.5f, 3.8f,  6.8f, 7.1f, 8.3f, 8.6f };
    std::vector<float> const arm_R_value  = { 0,    1,    1,    0,     0,    1,    1,    0 };
    std::vector<float> const tail_time    = { 0.5f, 0.9f, 1.9f, 2.5f, 3.5f, 4.1f, 5.1f, 5.5f,  9.2f, 9.6f, 10.6f, 11.0f };
    std::vector<float> const tail_value   = { 0,    1,    1,    -1,   -1,   1,    1,    0,     0,    -1,   -1,    0 };
    std::vector<float> const tail_up_time = { 7.5f, 8.0f, 9.0f, 9.5f };
    std::vector<float> const tail_up_value= { 0,    1,    1,    0 };
    std::vector<float> const head_time    = { 4.0f, 4.4f, 5.6f, 6.0f, 7.0f, 7.4f };
    std::vector<float> const head_value   = { 0,    1,    1,    -0.5f, -0.5f, 0 };
    std::vector<float> const nod_time     = { 5.6f, 6.0f, 7.0f, 7.4f,  9.8f, 10.1f, 10.4f, 10.7f };
    std::vector<float> const nod_value    = { 0,    1,    1,    0,     0,    1,     -0.5f, 0 };
    std::vector<float> const jaw_time     = { 4.2f, 4.5f, 5.0f, 5.3f,  9.8f, 10.1f, 10.8f, 11.1f };
    std::vector<float> const jaw_value    = { 0,    1,    1,    0,     0,    1,     1,     0 };
    std::vector<float> const leg_L_time   = { 3.0f, 3.3f, 3.9f, 4.2f,  8.8f, 9.1f, 9.7f, 10.0f };
    std::vector<float> const leg_L_value  = { 0,    1,    1,    0,     0,    1,    1,    0 };
    std::vector<float> const leg_R_time   = { 5.2f, 5.5f, 6.1f, 6.4f,  9.3f, 9.6f, 10.2f, 10.5f };
    std::vector<float> const leg_R_value  = { 0,    1,    1,    0,     0,    1,    1,     0 };

    pose_structure const rest = rest_pose(skeleton);
    auto pose_at = [&](float t) {
        // Sinusoidal flapping at frequency f, with a smooth envelope at the start and before the glide
        // (starts and stops with zero velocity)
        float flap = 0.0f;
        if (t < t_glide) {
            float const u = std::min(1.0f, std::min(t, t_glide - t) * 2 * f);
            flap = angle_flap * std::sin(2 * Pi * f * t) * u * u * (3.0f - 2.0f * u);
        }
        float const arm_L = smooth_curve(t, arm_L_time, arm_L_value);
        float const arm_R = smooth_curve(t, arm_R_time, arm_R_value);
        float const swing = smooth_curve(t, tail_time, tail_value);
        float const lift = smooth_curve(t, tail_up_time, tail_up_value);
        float const head = smooth_curve(t, head_time, head_value);
        float const nod = smooth_curve(t, nod_time, nod_value);
        float const mouth = smooth_curve(t, jaw_time, jaw_value);
        float const leg_L = smooth_curve(t, leg_L_time, leg_L_value);
        float const leg_R = smooth_curve(t, leg_R_time, leg_R_value);

        pose_structure pose = rest;
        pose = rotate_joint(skeleton, pose, wing_L, forward, sign_wing_L * flap);
        pose = rotate_joint(skeleton, pose, wing_R, forward, sign_wing_R * flap);
        pose = rotate_joint(skeleton, pose, shoulder_L, left, sign_shoulder_L * angle_shoulder * arm_L);
        pose = rotate_joint(skeleton, pose, shoulder_R, left, sign_shoulder_R * angle_shoulder * arm_R);
        pose = rotate_joint(skeleton, pose, elbow_L, left, sign_elbow_L * angle_elbow * arm_L);
        pose = rotate_joint(skeleton, pose, elbow_R, left, sign_elbow_R * angle_elbow * arm_R);
        for (int k = 0; k < 3; ++k) {
            pose = rotate_joint(skeleton, pose, tail[k], forward, sign_tail * angle_tail * swing);
            pose = rotate_joint(skeleton, pose, tail[k], left, sign_tail_up * angle_tail_up * lift);
        }
        pose = rotate_joint(skeleton, pose, neck, up, sign_head * angle_head * head);
        pose = rotate_joint(skeleton, pose, neck, left, sign_nod * angle_nod * nod);
        pose = rotate_joint(skeleton, pose, jaw, left, sign_jaw * angle_jaw * mouth);
        pose = rotate_joint(skeleton, pose, hip_L, left, sign_hip_L * angle_hip * leg_L);
        pose = rotate_joint(skeleton, pose, hip_R, left, sign_hip_R * angle_hip * leg_R);
        pose = rotate_joint(skeleton, pose, knee_L, left, sign_knee_L * angle_knee * leg_L);
        pose = rotate_joint(skeleton, pose, knee_R, left, sign_knee_R * angle_knee * leg_R);
        return pose;
    };

    // Sampling: at least 20 samples per wing beat
    float const dt = std::min(1.0f / 60.0f, 1.0f / (20.0f * f));
    return build_sampled_animation(skeleton, pose_at, duration, dt);
}


skeleton_animation_structure build_cthulhu_head_shake(skeleton_structure const& skeleton)
{
    // Tunable parameters: small angles, the large wings and the long tentacles quickly self-intersect
    float const angle_wing  =  6.0f * Pi / 180.0f;  // wings raised (one slow beat, no resonance)
    float const angle_shake =  6.0f * Pi / 180.0f;  // head shake (left-right)
    float const angle_tentacle = 12.0f * Pi / 180.0f;  // swing of the face tentacles (Bone_003)
    float const angle_nod   =  8.0f * Pi / 180.0f;  // head nod (down)
    float const angle_tail  =  8.0f * Pi / 180.0f;  // tail swing (sideways)
    float const angle_arm   = 12.0f * Pi / 180.0f;  // arms reaching forward
    float const angle_leg   =  7.0f * Pi / 180.0f;  // legs moving forward (slowly)

    auto joint = [&](char const* name) { return find_joint(skeleton, name); };
    auto position = [&](int j) { return skeleton.joint_matrix_global[j].get_block_translation(); };

    int const wing_R = joint("Bone_026"), wing_R_tip = joint("FBone_036");
    int const wing_L = joint("Bone_034"), wing_L_tip = joint("FBone_039");
    int const neck = joint("Bone_002"), head = joint("Bone_003"), tentacle_tip = joint("FBone_040");
    int const tail = joint("Bone_017"), tail_tip = joint("FBone_021");
    int const arm_R = joint("Bone_008"), hand_R = joint("Bone_009");
    int const arm_L = joint("Bone_044"), hand_L = joint("Bone_045");
    int const leg_R = joint("Bone_012"), foot_R = joint("Bone_014");
    int const leg_L = joint("Bone_050"), foot_L = joint("Bone_052");

    // Body frame: vertical, forward (toward the head, horizontal), and lateral (toward the Bone_026 wing)
    vec3 const up = {0, 1, 0};
    vec3 forward = position(head) - position(0);
    forward.y = 0.0f;
    forward = normalize(forward);
    vec3 const lateral = normalize(position(wing_R) - position(wing_L));

    float const sign_wing_R = rotation_sign(skeleton, wing_R, wing_R_tip, forward, up);
    float const sign_wing_L = rotation_sign(skeleton, wing_L, wing_L_tip, forward, up);
    float const sign_shake = rotation_sign(skeleton, neck, head, up, lateral);
    float const sign_nod = rotation_sign(skeleton, neck, head, lateral, -up);
    float const sign_tentacle = rotation_sign(skeleton, head, tentacle_tip, up, lateral);
    float const sign_tail = rotation_sign(skeleton, tail, tail_tip, up, lateral);
    float const sign_arm_R = rotation_sign(skeleton, arm_R, hand_R, lateral, forward);
    float const sign_arm_L = rotation_sign(skeleton, arm_L, hand_L, lateral, forward);
    float const sign_leg_R = rotation_sign(skeleton, leg_R, foot_R, lateral, forward);
    float const sign_leg_L = rotation_sign(skeleton, leg_L, foot_L, lateral, forward);

    // Timeline (s): head, wings, arms and legs move together, with quick moves and pauses
    // (curves of value -1..1, see smooth_curve). Two "gestures" per loop:
    //   1) wings up, arms forward, left leg kick, head nod; then the head shakes ("no") while the
    //      face tentacles swing to one side and back, the wings go slightly down and the right leg kicks;
    //   2) everything together again: wings up, arms forward, both legs, nod and tentacles.
    std::vector<float> const wing_time      = { 0.3f, 0.9f, 2.0f, 2.6f, 3.6f, 4.2f,  5.6f, 6.2f, 7.2f, 7.8f };
    std::vector<float> const wing_value     = { 0, 1, 1, -0.5f, -0.5f, 0,  0, 1, 1, 0 };
    std::vector<float> const arm_time       = { 0.3f, 0.7f, 1.8f, 2.3f,  5.6f, 6.0f, 7.0f, 7.4f };
    std::vector<float> const arm_value      = { 0, 1, 1, 0,  0, 1, 1, 0 };
    // (legs: slower moves, the spikes and claws stretch quickly)
    std::vector<float> const leg_L_time     = { 0.4f, 1.1f, 1.8f, 2.5f,  5.6f, 6.3f, 7.0f, 7.7f };
    std::vector<float> const leg_L_value    = { 0, 1, 1, 0,  0, 1, 1, 0 };
    std::vector<float> const leg_R_time     = { 2.6f, 3.3f, 4.0f, 4.7f,  5.6f, 6.3f, 7.0f, 7.7f };
    std::vector<float> const leg_R_value    = { 0, 1, 1, 0,  0, 1, 1, 0 };
    std::vector<float> const nod_time       = { 0.3f, 0.7f, 1.8f, 2.2f,  5.7f, 6.0f, 7.0f, 7.3f };
    std::vector<float> const nod_value      = { 0, 1, 1, 0,  0, 1, 1, 0 };
    std::vector<float> const shake_time     = { 3.0f, 3.3f, 3.8f, 4.1f };
    std::vector<float> const shake_value    = { 0, 1, -1, 0 };
    // (single moves of ~0.4 s followed by a pause: not faster than the oscillation of the tentacles)
    std::vector<float> const tentacle_time  = { 3.0f, 3.4f, 4.2f, 4.6f,  6.0f, 6.4f, 7.0f, 7.4f };
    std::vector<float> const tentacle_value = { 0, 1, 1, 0,  0, -1, -1, 0 };
    std::vector<float> const tail_time      = { 1.0f, 1.4f, 2.4f, 3.0f, 4.0f, 4.4f };
    std::vector<float> const tail_value     = { 0, 1, 1, -1, -1, 0 };
    float const duration = 9.0f;

    pose_structure const rest = rest_pose(skeleton);
    auto pose_at = [&](float t) {
        float const wing = smooth_curve(t, wing_time, wing_value);
        float const shake = smooth_curve(t, shake_time, shake_value);
        float const tentacle = smooth_curve(t, tentacle_time, tentacle_value);
        float const nod = smooth_curve(t, nod_time, nod_value);
        float const swing = smooth_curve(t, tail_time, tail_value);
        float const arm = smooth_curve(t, arm_time, arm_value);
        float const kick_L = smooth_curve(t, leg_L_time, leg_L_value);
        float const kick_R = smooth_curve(t, leg_R_time, leg_R_value);

        pose_structure pose = rest;
        pose = rotate_joint(skeleton, pose, wing_R, forward, sign_wing_R * angle_wing * wing);
        pose = rotate_joint(skeleton, pose, wing_L, forward, sign_wing_L * angle_wing * wing);
        pose = rotate_joint(skeleton, pose, neck, up, sign_shake * angle_shake * shake);
        pose = rotate_joint(skeleton, pose, neck, lateral, sign_nod * angle_nod * nod);
        pose = rotate_joint(skeleton, pose, head, up, sign_tentacle * angle_tentacle * tentacle);
        pose = rotate_joint(skeleton, pose, tail, up, sign_tail * angle_tail * swing);
        pose = rotate_joint(skeleton, pose, arm_R, lateral, sign_arm_R * angle_arm * arm);
        pose = rotate_joint(skeleton, pose, arm_L, lateral, sign_arm_L * angle_arm * arm);
        pose = rotate_joint(skeleton, pose, leg_L, lateral, sign_leg_L * angle_leg * kick_L);
        pose = rotate_joint(skeleton, pose, leg_R, lateral, sign_leg_R * angle_leg * kick_R);
        return pose;
    };

    return build_sampled_animation(skeleton, pose_at, duration, 1.0f / 60.0f);
}


skeleton_animation_structure build_train_bumps(skeleton_structure const& skeleton)
{
    // Tunable parameters (lengths in the units of the train asset, its height is about 2.6)
    float const duration    = 6.0f;                   // (s) loop
    float const bump_time   = 0.35f;                  // (s) duration of a bump (up and down)
    float const bump_height = 0.6f;                   // height of the largest bump
    float const angle_roll  = 10.0f * Pi / 180.0f;    // roll of the largest bump (around the train axis)
    // Start time, relative height and roll side of each bump
    struct bump { float start; float height; float roll; };
    std::vector<bump> const bumps = { {0.5f, 1.0f, 1.0f}, {2.3f, 0.6f, -1.0f}, {3.6f, 0.8f, 1.0f} };

    int const engine = 0;  // root joint, at the front of the engine
    vec3 const axis_train = {1, 0, 0};
    pose_structure const rest = rest_pose(skeleton);

    auto pose_at = [&](float t) {
        float height = 0.0f, roll = 0.0f;
        for (auto const& b : bumps) {
            float const u = (t - b.start) / bump_time;
            if (u > 0.0f && u < 1.0f) {
                float const profile = std::sin(Pi * u);  // up and down, zero velocity at the top
                height += b.height * profile;
                roll += b.roll * b.height * std::sin(Pi * u) * profile;
            }
        }
        pose_structure pose = rotate_joint(skeleton, rest, engine, axis_train, angle_roll * roll);
        pose[engine] = mat4::build_translation(0.0f, bump_height * height, 0.0f) * pose[engine];
        return pose;
    };

    return build_sampled_animation(skeleton, pose_at, duration, 1.0f / 60.0f);
}
