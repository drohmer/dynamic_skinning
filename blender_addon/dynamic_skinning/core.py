# Dynamic skinning (oscillation and drag), with numpy only (no dependency on Blender)
#
# Same method as dynamic_skinning_source/ (C++), with the notations of the paper:
#   p_i^DS = p_i^LBS + Σ_j β_ij ψ^bend(F^osc(Ω_j), p_i^LBS - q_j)
#   F^osc(Ω_j)(t) = D Ω_j(t) + Σ_k κ(k Δt) (Ω_j(t - k Δt) - Ω_j(t - (k+1) Δt))
#   κ(t) = A exp(-t/τ) sin(2π f t)
#   ψ^bend(Ω, u) = (R - I) v, R = rotation of axis Ω/‖Ω‖ and angle θ = -γ^bend ‖Ω × u‖,
#                  v = component of u = p - q_j orthogonal to the axis

import math
import numpy as np


HISTORY_DT = 1.0 / 60     # time step Δt of the angular velocity history (independent of the frame rate)
HISTORY_SIZE = 400        # number of stored samples
N_CONVOLUTION = 300       # number of samples of the convolution (5 s)
SMOOTHING_TIME = 0.058    # (s) exponential smoothing of the measured angular velocity
MAX_ANGULAR_VELOCITY = 50.0  # (rad/s) rejects spikes (e.g. jitter of the mouse)
BEND_COUPLING = 0.4       # coupling coefficient of the bend (as in the C++ code)
ACTIVE_THRESHOLD = 1e-2   # ‖F^osc(Ω_j)‖ below which a joint is ignored


class Parameters:
    def __init__(self, f=1.5, tau=0.67, A=0.88, D=0.5, bend=1.0):
        self.f = f        # frequency (Hz)
        self.tau = tau    # damping τ (s)
        self.A = A        # amplitude
        self.D = D        # drag
        self.bend = bend  # magnitude of the bend γ^bend


def quaternion_multiply(a, b):
    """Product of quaternions stored as (w, x, y, z), arrays of shape (..., 4)."""
    aw, ax, ay, az = np.moveaxis(a, -1, 0)
    bw, bx, by, bz = np.moveaxis(b, -1, 0)
    return np.stack([aw * bw - ax * bx - ay * by - az * bz,
                     aw * bx + ax * bw + ay * bz - az * by,
                     aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw], axis=-1)


def oscillating_kernel(params):
    """κ(k Δt) for the samples k of the convolution."""
    t = np.arange(N_CONVOLUTION) * HISTORY_DT
    return params.A * np.exp(-t / max(params.tau, 1e-3)) * np.sin(2 * math.pi * params.f * t)


def hierarchical_weights(parent, vertex_joints, vertex_weights):
    """β_ij = Σ of the skinning weights α_ik of the joints k in the subtree of j.

    parent: (J,) parent index of each joint (-1 for a root)
    vertex_joints, vertex_weights: (N, K) skinning joints and weights α_ij of each vertex (weight 0 when unused)
    Return, for each joint j, (indices of the vertices with β_ij > 0, β_ij).
    """
    N, K = vertex_joints.shape
    J = len(parent)
    beta = np.zeros((N, J))
    for k in range(K):
        joint = vertex_joints[:, k].copy()
        alpha = vertex_weights[:, k]
        # Walk up to the root: each ancestor accumulates the weight α of its descendant
        while True:
            valid = (joint >= 0) & (alpha > 0)
            if not valid.any():
                break
            np.add.at(beta, (np.nonzero(valid)[0], joint[valid]), alpha[valid])
            joint = np.where(valid, parent[np.maximum(joint, 0)], -1)
    result = []
    for j in range(J):
        idx = np.nonzero(beta[:, j] > 1e-6)[0]
        result.append((idx, beta[idx, j]))
    return result


class DynamicSkinning:
    """State of the dynamic skinning of one rigged mesh."""

    def __init__(self, parent, vertex_joints, vertex_weights, bend_weight, reference_length):
        """
        parent: (J,) parent index of each joint
        vertex_joints, vertex_weights: (N, K) skinning weights α_ij
        bend_weight: (N,) per-vertex bend weight γ_i^bend (0.5 by default, as in the C++ code)
        reference_length: half size of the model; the bend angle is computed on the model scaled to [-1, 1]
        """
        self.parent = np.asarray(parent)
        self.beta = hierarchical_weights(self.parent, vertex_joints, vertex_weights)
        self.bend_weight = np.asarray(bend_weight, dtype=np.float64)
        self.reference_length = reference_length
        self.reset()

    def reset(self):
        J = len(self.parent)
        self.history = np.zeros((J, HISTORY_SIZE, 3))  # smoothed Ω_j (local frame), newest last
        self.smoothed_velocity = np.zeros((J, 3))
        self.last_rotation = None
        self.last_time = None
        self.last_history_time = None

    def record(self, local_rotation, time):
        """Add the local rotations (J, 4) (w, x, y, z) of the joints at the given time (s)."""
        q = local_rotation / np.linalg.norm(local_rotation, axis=1, keepdims=True)
        if self.last_rotation is None:
            self.last_rotation, self.last_time, self.last_history_time = q, time, time
            return
        dt = time - self.last_time
        if dt < 1e-4:
            return
        # Shortest path: q and -q are the same rotation
        q = np.where((np.sum(q * self.last_rotation, axis=1) < 0)[:, None], -q, q)

        # Angular velocity (axis × angle / dt) of the rotation from the last sample
        delta = quaternion_multiply(q, self.last_rotation * np.array([1, -1, -1, -1]))
        s = np.linalg.norm(delta[:, 1:], axis=1)
        angle = 2 * np.arctan2(s, delta[:, 0])
        axis = np.where((s > 1e-6)[:, None], delta[:, 1:] / np.maximum(s, 1e-12)[:, None], 0.0)
        velocity = axis * (angle / dt)[:, None]
        speed = np.linalg.norm(velocity, axis=1, keepdims=True)
        velocity *= np.minimum(1.0, MAX_ANGULAR_VELOCITY / np.maximum(speed, 1e-12))

        alpha = math.exp(-dt / SMOOTHING_TIME)
        self.smoothed_velocity = alpha * self.smoothed_velocity + (1 - alpha) * velocity
        self.last_rotation, self.last_time = q, time

        # History at fixed time step: the current velocity is pushed once per elapsed Δt
        n_steps = int((time - self.last_history_time) / HISTORY_DT)
        if n_steps > 0:
            self.last_history_time += n_steps * HISTORY_DT
            n_steps = min(n_steps, HISTORY_SIZE)
            self.history = np.roll(self.history, -n_steps, axis=1)
            self.history[:, -n_steps:] = self.smoothed_velocity[:, None, :]

    def filtered_angular_velocity(self, params, parent_rotation):
        """F^osc(Ω_j) in world coordinates, (J, 3).

        parent_rotation: (J, 3, 3) world rotation of the parent of each joint (world rotation for a root,
                         whose local rotation is already in world coordinates: identity)
        """
        h = self.history
        delta = h[:, -1:-N_CONVOLUTION - 1:-1] - h[:, -2:-N_CONVOLUTION - 2:-1]   # Ω(t - kΔt) - Ω(t - (k+1)Δt)
        F_local = params.D * h[:, -1] + np.einsum('k,jkc->jc', oscillating_kernel(params), delta)
        return np.einsum('jab,jb->ja', parent_rotation, F_local)

    def deformation(self, params, F, joint_position, p_lbs):
        """Offset Σ_j β_ij ψ^bend(F_j, p_i - q_j) of the vertices, (N, 3).

        F: (J, 3) filtered angular velocity (world);  joint_position: (J, 3) q_j;  p_lbs: (N, 3) LBS positions
        """
        offset = np.zeros_like(p_lbs)
        F_norm = np.linalg.norm(F, axis=1)
        gamma = BEND_COUPLING * params.bend * self.bend_weight
        for j in np.nonzero(F_norm >= ACTIVE_THRESHOLD)[0]:
            idx, beta = self.beta[j]
            if len(idx) == 0:
                continue
            w = F[j] / F_norm[j]                         # axis ω of the rotation
            u = p_lbs[idx] - joint_position[j]
            v = u - (u @ w)[:, None] * w                 # component of u orthogonal to ω
            # θ = -γ ‖Ω × u‖, with u measured on the model scaled to [-1, 1]
            theta = -gamma[idx] * F_norm[j] * np.linalg.norm(np.cross(w, u), axis=1) / self.reference_length
            # (R - I) v with Rodrigues' formula (v ⊥ ω):  sin θ (ω × v) + (cos θ - 1) v
            bend = np.sin(theta)[:, None] * np.cross(w, v) + (np.cos(theta) - 1)[:, None] * v
            offset[idx] += beta[:, None] * bend
        return offset
