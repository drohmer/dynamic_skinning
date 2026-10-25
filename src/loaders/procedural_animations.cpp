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
    int   const N_flap         = 4;                    // number of wing beats
    float const hold_glide     = 1.5f;                 // (s) pause after the flapping (wing tips oscillation)
    float const angle_shoulder = 25.0f * Pi / 180.0f;  // arms reaching forward
    float const angle_elbow    = 35.0f * Pi / 180.0f;
    float const move_arm       = 0.3f;                 // (s) each arm motion
    float const hold_arm       = 1.2f;                 // (s) pause after an arm motion (hands oscillation)
    float const angle_tail     = 15.0f * Pi / 180.0f;  // tail swing, on each of the 3 first tail joints
    float const move_tail      = 0.35f;                // (s) each tail motion
    float const hold_tail      = 1.2f;                 // (s) pause after a tail motion
    float const hold_end       = 1.0f;                 // (s) pause in the rest pose before looping

    float const f = std::max(flap_frequency, 0.2f);

    auto joint = [&](char const* name) { return find_joint(skeleton, name); };
    auto position = [&](int j) { return skeleton.joint_matrix_global[j].get_block_translation(); };

    int const wing_L = joint("L_Wing1"),  wing_L_tip = joint("L_Wing21");
    int const wing_R = joint("R_Wing23"), wing_R_tip = joint("R_Wing43");
    int const shoulder_L = joint("L_Arm3"),  elbow_L = joint("L_Arm4"),  hand_L = joint("L_Arm6");
    int const shoulder_R = joint("R_Arm18"), elbow_R = joint("R_Arm19"), hand_R = joint("R_Arm21");
    int const tail[3] = { joint("Tatil_1"), joint("Tatil_2"), joint("Tatil_3") };
    int const tail_tip = joint("Tatil_5");

    // Body frame: vertical, forward (toward the head, horizontal), and left (L_ side)
    vec3 const up = {0, 1, 0};
    vec3 forward = position(joint("Head_Joint1")) - position(0);
    forward.y = 0.0f;
    forward = normalize(forward);
    vec3 const left = normalize(position(wing_L) - position(wing_R));

    // Wings flap around the forward axis, arms rotate around the lateral axis, the tail swings sideways
    float const sign_wing_L = rotation_sign(skeleton, wing_L, wing_L_tip, forward, up);
    float const sign_wing_R = rotation_sign(skeleton, wing_R, wing_R_tip, forward, up);
    float const sign_shoulder_L = rotation_sign(skeleton, shoulder_L, hand_L, left, forward);
    float const sign_shoulder_R = rotation_sign(skeleton, shoulder_R, hand_R, left, forward);
    float const sign_elbow_L = rotation_sign(skeleton, elbow_L, hand_L, left, forward);
    float const sign_elbow_R = rotation_sign(skeleton, elbow_R, hand_R, left, forward);
    float const sign_tail = rotation_sign(skeleton, tail[0], tail_tip, forward, left);

    // Timeline: flapping -> glide (pause) -> arms forward (pause) -> back (pause)
    //           -> tail on one side (pause) -> other side (pause) -> rest (pause, loop)
    float const t_flap = N_flap / f;
    float const t_arm = t_flap + hold_glide;
    std::vector<float> const arm_time  = { t_arm, t_arm + move_arm, t_arm + move_arm + hold_arm, t_arm + 2 * move_arm + hold_arm };
    std::vector<float> const arm_value = { 0, 1, 1, 0 };
    float const t_tail = arm_time.back() + hold_arm;
    std::vector<float> tail_time = { t_tail };
    tail_time.push_back(tail_time.back() + move_tail);
    tail_time.push_back(tail_time.back() + hold_tail);
    tail_time.push_back(tail_time.back() + 1.5f * move_tail);  // larger motion: from one side to the other
    tail_time.push_back(tail_time.back() + hold_tail);
    tail_time.push_back(tail_time.back() + move_tail);
    std::vector<float> const tail_value = { 0, 1, 1, -1, -1, 0 };
    float const duration = tail_time.back() + hold_end;

    pose_structure const rest = rest_pose(skeleton);
    auto pose_at = [&](float t) {
        // Sinusoidal flapping at frequency f, with a smooth envelope over the first and last half beats
        // (starts and stops with zero velocity)
        float flap = 0.0f;
        if (t < t_flap) {
            float const u = std::min(1.0f, std::min(t, t_flap - t) * 2 * f);
            flap = angle_flap * std::sin(2 * Pi * f * t) * u * u * (3.0f - 2.0f * u);
        }
        float const arm = smooth_curve(t, arm_time, arm_value);
        float const swing = smooth_curve(t, tail_time, tail_value);

        pose_structure pose = rest;
        pose = rotate_joint(skeleton, pose, wing_L, forward, sign_wing_L * flap);
        pose = rotate_joint(skeleton, pose, wing_R, forward, sign_wing_R * flap);
        pose = rotate_joint(skeleton, pose, shoulder_L, left, sign_shoulder_L * angle_shoulder * arm);
        pose = rotate_joint(skeleton, pose, shoulder_R, left, sign_shoulder_R * angle_shoulder * arm);
        pose = rotate_joint(skeleton, pose, elbow_L, left, sign_elbow_L * angle_elbow * arm);
        pose = rotate_joint(skeleton, pose, elbow_R, left, sign_elbow_R * angle_elbow * arm);
        for (int k = 0; k < 3; ++k)
            pose = rotate_joint(skeleton, pose, tail[k], forward, sign_tail * angle_tail * swing);
        return pose;
    };

    // Sampling: at least 20 samples per wing beat
    float const dt = std::min(1.0f / 60.0f, 1.0f / (20.0f * f));
    return build_sampled_animation(skeleton, pose_at, duration, dt);
}
