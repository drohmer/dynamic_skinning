# Link between Blender and the dynamic skinning (core.py): reads the rig and the pose, writes the offsets
#
# The offset of each vertex is stored in the point attribute "ds_offset" of the mesh, and added to the
# position by a Geometry Nodes modifier placed after the Armature modifier.

import time
import bpy
import numpy as np

from . import core


ATTRIBUTE = "ds_offset"
NODE_GROUP = "DS_ApplyOffset"
MODIFIER = "Dynamic Skinning"
BEND_GROUP = "DS_bend"      # optional vertex group: per-vertex bend weight γ^bend
DEFAULT_BEND_WEIGHT = 0.5   # γ^bend of the vertices when there is no DS_bend group (as in the C++ code)
TIMER_PERIOD = 1.0 / 60


# Runtime state of each enabled mesh object (not saved in the .blend file), by object name
states = {}


class RigState:
    def __init__(self, obj):
        modifier = next((m for m in obj.modifiers if m.type == 'ARMATURE' and m.object), None)
        if modifier is None:
            raise ValueError("The mesh has no Armature modifier with an armature")
        self.armature_name = modifier.object.name
        armature = modifier.object
        mesh = obj.data

        # Joints = deforming bones (a joint rotates its bone around the head)
        bones = list(armature.pose.bones)
        self.bone_names = [b.name for b in bones]
        index = {b.name: k for k, b in enumerate(bones)}
        parent = np.array([index[b.parent.name] if b.parent else -1 for b in bones])
        self.parent = parent
        self.inverse_rest = np.array([np.array(b.bone.matrix_local.inverted()) for b in bones])  # B_j^-1

        # Skinning weights α_ij from the vertex groups named as deforming bones (normalized, as the Armature modifier)
        group_to_joint = {g.index: index[g.name] for g in obj.vertex_groups
                          if g.name in index and bones[index[g.name]].bone.use_deform}
        bend_group = obj.vertex_groups.get(BEND_GROUP)
        N = len(mesh.vertices)
        joints, weights = [[] for _ in range(N)], [[] for _ in range(N)]
        # (with a DS_bend group, the vertices outside the group are not bent)
        bend_index = bend_group.index if bend_group is not None else -1
        bend_weight = np.full(N, 0.0 if bend_group is not None else DEFAULT_BEND_WEIGHT)
        for i, v in enumerate(mesh.vertices):
            for g in v.groups:
                if g.group in group_to_joint and g.weight > 0:
                    joints[i].append(group_to_joint[g.group])
                    weights[i].append(g.weight)
                if g.group == bend_index:
                    bend_weight[i] = g.weight
        K = max(1, max(len(j) for j in joints))
        self.vertex_joints = np.full((N, K), -1)
        self.vertex_weights = np.zeros((N, K))
        for i in range(N):
            self.vertex_joints[i, :len(joints[i])] = joints[i]
            self.vertex_weights[i, :len(weights[i])] = weights[i]
        total = self.vertex_weights.sum(axis=1, keepdims=True)
        self.vertex_weights /= np.where(total > 0, total, 1.0)
        self.has_weight = total[:, 0] > 0

        # Rest positions p_i^0 (mesh coordinates)
        p0 = np.zeros(3 * N, dtype=np.float32)
        mesh.vertices.foreach_get("co", p0)
        self.p0 = p0.reshape(N, 3)

        # Per joint: the vertices it influences, their weights α_ij and rest positions (for the LBS, see update)
        self.joint_vertices = []
        for j in range(len(bones)):
            rows, columns = np.nonzero(self.vertex_joints == j)
            self.joint_vertices.append((core.as_slice(rows), self.vertex_weights[rows, columns].astype(np.float32)[:, None],
                                        self.p0[rows]))

        # Half size of the model (world coordinates): the bend is computed on the model scaled to [-1, 1]
        W_mesh = np.array(obj.matrix_world)
        p_world = self.p0 @ W_mesh[:3, :3].T + W_mesh[:3, 3]
        size = 0.5 * np.max(p_world.max(axis=0) - p_world.min(axis=0))
        self.ds = core.DynamicSkinning(parent, self.vertex_joints, self.vertex_weights, bend_weight, max(size, 1e-6))
        self.last_written_zero = False

    def update(self, obj, params, current_time):
        """Read the pose, update the angular velocities, and write the offsets. Return False if the rig is gone."""
        armature = bpy.data.objects.get(self.armature_name)
        if armature is None or armature.type != 'ARMATURE':
            return False
        if obj.mode == 'EDIT' or armature.mode == 'EDIT':
            return True
        bones = [armature.pose.bones.get(name) for name in self.bone_names]
        if any(b is None for b in bones) or len(obj.data.vertices) != len(self.p0):
            return False

        # Joint frames: armature space M_j, world G_j = W_armature M_j
        W_armature = armature.matrix_world
        M = [b.matrix for b in bones]
        G = [W_armature @ m for m in M]
        J = len(bones)
        local_rotation = np.zeros((J, 4))
        parent_rotation = np.tile(np.eye(3), (J, 1, 1))
        for j in range(J):
            p = self.parent[j]
            if p >= 0:
                L = M[p].inverted() @ M[j]                    # local frame (relative to the parent)
                parent_rotation[j] = np.array(G[p].to_3x3().normalized())
            else:
                L = G[j]                                      # root: world frame
            local_rotation[j] = np.array(L.to_3x3().normalized().to_quaternion())
        joint_position = np.array([g.translation for g in G])

        self.ds.record(local_rotation, current_time)
        F = self.ds.filtered_angular_velocity(params, parent_rotation)

        # Nothing to deform: write zeros once, then leave the mesh untouched
        if np.all(np.linalg.norm(F, axis=1) < core.ACTIVE_THRESHOLD):
            if not self.last_written_zero:
                write_offset(obj, np.zeros_like(self.p0))
                self.last_written_zero = True
            return True

        # LBS positions (world): p_i = Σ_j α_ij S_j p_i^0  with  S_j = W_armature M_j B_j^-1 W_armature^-1 W_mesh
        # Computed per joint, on the vertices it influences: one (n x 3) (3 x 3) product per joint
        to_armature = np.array(W_armature.inverted() @ obj.matrix_world)
        skinning = (np.array([np.array(g) for g in G]) @ self.inverse_rest @ to_armature).astype(np.float32)
        p_lbs = np.zeros_like(self.p0)
        if not self.has_weight.all():   # vertices without weight: not deformed by the Armature modifier
            W_mesh = np.array(obj.matrix_world, dtype=np.float32)
            p_lbs[~self.has_weight] = self.p0[~self.has_weight] @ W_mesh[:3, :3].T + W_mesh[:3, 3]
        for S, (idx, alpha, p0) in zip(skinning, self.joint_vertices):
            if len(alpha):
                p_lbs[idx] += alpha * (p0 @ S[:3, :3].T + S[:3, 3])

        offset_world = self.ds.deformation(params, F, joint_position, p_lbs)
        to_mesh = np.array(obj.matrix_world.inverted().to_3x3())
        write_offset(obj, offset_world @ to_mesh.T.astype(np.float32))
        self.last_written_zero = False
        return True


def write_offset(obj, offset):
    mesh = obj.data
    attribute = mesh.attributes.get(ATTRIBUTE)
    if attribute is None:
        attribute = mesh.attributes.new(ATTRIBUTE, 'FLOAT_VECTOR', 'POINT')
    attribute.data.foreach_set("vector", offset.astype(np.float32, copy=False).ravel())
    mesh.update_tag()


def offset_node_group():
    """Geometry Nodes group: position += named attribute ds_offset."""
    group = bpy.data.node_groups.get(NODE_GROUP)
    if group is not None:
        return group
    group = bpy.data.node_groups.new(NODE_GROUP, 'GeometryNodeTree')
    group.interface.new_socket("Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
    group.interface.new_socket("Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')
    nodes = group.nodes
    node_input = nodes.new('NodeGroupInput')
    node_output = nodes.new('NodeGroupOutput')
    attribute = nodes.new('GeometryNodeInputNamedAttribute')
    attribute.data_type = 'FLOAT_VECTOR'
    attribute.inputs["Name"].default_value = ATTRIBUTE
    set_position = nodes.new('GeometryNodeSetPosition')
    node_input.location, attribute.location = (-400, 0), (-400, -150)
    set_position.location, node_output.location = (-150, 0), (100, 0)
    group.links.new(node_input.outputs["Geometry"], set_position.inputs["Geometry"])
    group.links.new(attribute.outputs["Attribute"], set_position.inputs["Offset"])
    group.links.new(set_position.outputs["Geometry"], node_output.inputs["Geometry"])
    return group


def enable(obj):
    states[obj.name] = RigState(obj)
    write_offset(obj, np.zeros((len(obj.data.vertices), 3)))
    if obj.modifiers.get(MODIFIER) is None:
        modifier = obj.modifiers.new(MODIFIER, 'NODES')   # at the end of the stack: after the Armature
        modifier.node_group = offset_node_group()
    obj.dynamic_skinning.enabled = True
    ensure_timer()


def disable(obj):
    states.pop(obj.name, None)
    modifier = obj.modifiers.get(MODIFIER)
    if modifier is not None:
        obj.modifiers.remove(modifier)
    attribute = obj.data.attributes.get(ATTRIBUTE)
    if attribute is not None:
        obj.data.attributes.remove(attribute)
    obj.dynamic_skinning.enabled = False


def parameters(obj):
    s = obj.dynamic_skinning
    return core.Parameters(f=s.frequency, tau=s.damping, A=s.amplitude, D=s.drag, bend=s.bend)


def update_all(current_time):
    for name in list(states):
        obj = bpy.data.objects.get(name)
        if obj is None or obj.type != 'MESH' or not obj.dynamic_skinning.enabled:
            states.pop(name)
            continue
        if not states[name].update(obj, parameters(obj), current_time):
            states.pop(name)


def timer():
    if not states:
        return None   # stops the timer
    update_all(time.perf_counter())
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == 'VIEW_3D':
                area.tag_redraw()
    return TIMER_PERIOD


def ensure_timer():
    if not bpy.app.timers.is_registered(timer):
        bpy.app.timers.register(timer, first_interval=TIMER_PERIOD)


def restart_enabled():
    """Restart the objects of the current file that were enabled (after loading a file, or when the add-on
    is enabled with a file already open). Also used as a one-shot timer: returns None."""
    states.clear()
    for obj in bpy.data.objects:
        if obj.type == 'MESH' and obj.dynamic_skinning.enabled:
            try:
                states[obj.name] = RigState(obj)
            except ValueError:
                obj.dynamic_skinning.enabled = False
    if states:
        ensure_timer()
    return None


@bpy.app.handlers.persistent
def on_load(_):
    restart_enabled()
