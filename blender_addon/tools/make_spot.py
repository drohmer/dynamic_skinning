# Spot the cow (blender_addon/assets/spot/, copied from the assets of the applications) in Blender: importer of the
# asset format of the applications
# (mesh.obj, skeleton_*, rig_joint, rig_weights, texture.png), and the animation of the applications:
# the head nods (neck), pauses, then twists on each side so that the ears oscillate.

import math
import os

import bpy
from mathutils import Matrix, Quaternion, Vector

SPOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "spot")


def read_rows(filename, convert):
    with open(filename) as f:
        return [[convert(x) for x in line.split()] for line in f if line.strip()]


def load_mesh(directory, name):
    """OBJ with the vertices in the order of the file (the skinning weights are given per vertex of the file)."""
    positions, uvs, faces, face_uvs = [], [], [], []
    with open(os.path.join(directory, "mesh.obj")) as f:
        for line in f:
            items = line.split()
            if not items:
                continue
            if items[0] == "v":
                positions.append(tuple(float(x) for x in items[1:4]))
            elif items[0] == "vt":
                uvs.append((float(items[1]), float(items[2])))
            elif items[0] == "f":
                corners = [c.split("/") for c in items[1:]]
                faces.append([int(c[0]) - 1 for c in corners])
                face_uvs.append([int(c[1]) - 1 if len(c) > 1 and c[1] else -1 for c in corners])
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(positions, [], faces)
    if uvs:
        uv_layer = mesh.uv_layers.new(name="UVMap")
        loop = 0
        for corners in face_uvs:
            for c in corners:
                if c >= 0:
                    uv_layer.data[loop].uv = uvs[c]
                loop += 1
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    return mesh


def load_legacy_character(directory, name):
    """Return (mesh object, armature object) linked to the current scene."""
    scene = bpy.context.scene
    mesh = load_mesh(directory, name)
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)

    # Skeleton: local frames (translation, quaternion x y z w) relative to the parent -> global positions
    names = [n[0] for n in read_rows(os.path.join(directory, "skeleton_joint_name"), str)]
    parent = [p for row in read_rows(os.path.join(directory, "skeleton_connectivity_parent"), int) for p in row]
    translation = read_rows(os.path.join(directory, "skeleton_rest_pose_translation"), float)
    rotation = read_rows(os.path.join(directory, "skeleton_rest_pose_rotation"), float)
    G = []
    for k in range(len(names)):
        x, y, z, w = rotation[k]
        local = Matrix.Translation(Vector(translation[k])) @ Quaternion((w, x, y, z)).to_matrix().to_4x4()
        G.append(G[parent[k]] @ local if parent[k] >= 0 else local)
    position = [g.translation.copy() for g in G]
    children = [[c for c in range(len(names)) if parent[c] == k] for k in range(len(names))]
    size = max((p - position[0]).length for p in position)

    # Armature: one bone per joint, from the joint to its first distinct child (the joint is the pivot = bone head)
    armature = bpy.data.objects.new(name + "Rig", bpy.data.armatures.new(name + "Rig"))
    scene.collection.objects.link(armature)
    armature.data.display_type = 'STICK'
    armature.show_in_front = True
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode='EDIT')
    bones = []
    for k, n in enumerate(names):
        bone = armature.data.edit_bones.new(n)
        targets = [position[c] for c in children[k] if (position[c] - position[k]).length > 1e-4]
        if targets:
            tail = sum(targets, Vector()) / len(targets)
        else:   # leaf, or child at the same position: along the y axis of the joint frame
            tail = position[k] + 0.05 * size * (G[k].to_3x3() @ Vector((0, 1, 0))).normalized()
        bone.head, bone.tail = position[k], tail
        if parent[k] >= 0:
            bone.parent = bones[parent[k]]
        bones.append(bone)
    bpy.ops.object.mode_set(mode='OBJECT')

    # Skinning weights α_ij: one vertex group per joint
    groups = [obj.vertex_groups.new(name=n) for n in names]
    joints = read_rows(os.path.join(directory, "rig_joint"), int)
    weights = read_rows(os.path.join(directory, "rig_weights"), float)
    for i, (js, ws) in enumerate(zip(joints, weights)):
        for j, w in zip(js, ws):
            groups[j].add([i], w, 'REPLACE')
    modifier = obj.modifiers.new("Armature", 'ARMATURE')
    modifier.object = armature

    # Texture
    texture = os.path.join(directory, "texture.png")
    if os.path.exists(texture):
        material = bpy.data.materials.new(name)
        material.use_nodes = True
        node = material.node_tree.nodes.new('ShaderNodeTexImage')
        node.image = bpy.data.images.load(texture)
        material.node_tree.links.new(node.outputs["Color"], material.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
        mesh.materials.append(material)
    return obj, armature


def add_spot_animation(armature, fps=30):
    """Head nod (neck) up and down, pause, then head twist on each side (ears oscillation), in a loop."""
    scene = bpy.context.scene
    scene.render.fps = fps
    bones = armature.data.bones
    lateral = (bones["arm_L"].head_local - bones["arm_R"].head_local).normalized()
    head_axis = (bones["head-end"].head_local - bones["head"].head_local).normalized()

    def rotation(bone_name, axis_world, angle):
        """Pose rotation of the bone: angle around an axis given in armature coordinates."""
        axis_local = bones[bone_name].matrix_local.to_3x3().transposed() @ axis_world
        return Quaternion(axis_local, angle)

    nod, twist = math.radians(12), math.radians(40)
    rest = (0, 0)
    poses = [rest, (nod, 0), (-nod, 0), (nod, 0), (-nod, 0), rest, (0, twist), (0, -twist), rest]
    hold = [2.0, 0.4, 0.4, 0.4, 0.4, 2.0, 1.5, 1.5, 0.0]
    move = [0.35, 0.35, 0.35, 0.35, 0.35, 0.25, 0.25, 0.25]
    neck, head = armature.pose.bones["neck"], armature.pose.bones["head"]
    t = 0.0
    for k, (nod_angle, twist_angle) in enumerate(poses):
        for time in (t, t + hold[k]):
            neck.rotation_quaternion = rotation("neck", lateral, nod_angle)
            head.rotation_quaternion = rotation("head", head_axis, twist_angle)
            frame = 1 + time * fps
            neck.keyframe_insert("rotation_quaternion", frame=frame)
            head.keyframe_insert("rotation_quaternion", frame=frame)
        t += hold[k] + (move[k] if k < len(move) else 0)
    scene.frame_start, scene.frame_end = 1, int(1 + t * fps)
    scene.frame_set(1)


def build_spot_scene():
    """Return (spot, armature), in the current scene emptied of its objects."""
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj)
    spot, armature = load_legacy_character(SPOT, "Spot")
    add_spot_animation(armature)
    return spot, armature
