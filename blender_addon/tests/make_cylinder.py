# Rigged cylinder along x (from -2 to 2) with 2 bones, as the Cylinder of the applications:
# bone A (-2 → 0) and bone B (0 → 2, child of A), smooth skinning weights around the middle joint.

import math

import bpy
import bmesh


def build_cylinder_scene(N_ring=60, N_segment=24):
    """Return (cylinder, armature), in the current scene emptied of its objects."""
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj)

    # ---- Cylinder along x, from -2 to 2 ----
    mesh = bpy.data.meshes.new("Cylinder")
    bm = bmesh.new()
    radius = 0.3
    rings = []
    for r in range(N_ring):
        x = -2 + 4 * r / (N_ring - 1)
        rings.append([bm.verts.new((x, radius * math.cos(2 * math.pi * s / N_segment),
                                    radius * math.sin(2 * math.pi * s / N_segment))) for s in range(N_segment)])
    for r in range(N_ring - 1):
        for s in range(N_segment):
            s1 = (s + 1) % N_segment
            bm.faces.new((rings[r][s], rings[r][s1], rings[r + 1][s1], rings[r + 1][s]))
    bm.to_mesh(mesh)
    bm.free()
    cylinder = bpy.data.objects.new("Cylinder", mesh)
    bpy.context.scene.collection.objects.link(cylinder)

    # ---- Armature: bone A (-2 → 0), bone B (0 → 2), child of A ----
    armature_data = bpy.data.armatures.new("Rig")
    armature = bpy.data.objects.new("Rig", armature_data)
    bpy.context.scene.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode='EDIT')
    a = armature_data.edit_bones.new("A")
    a.head, a.tail = (-2, 0, 0), (0, 0, 0)
    b = armature_data.edit_bones.new("B")
    b.head, b.tail = (0, 0, 0), (2, 0, 0)
    b.parent, b.use_connect = a, True
    bpy.ops.object.mode_set(mode='OBJECT')

    # Skinning weights: smooth transition around the middle joint (as in the cylinder of the applications)
    group_a, group_b = cylinder.vertex_groups.new(name="A"), cylinder.vertex_groups.new(name="B")
    for v in mesh.vertices:
        u = (v.co.x + 2) / 4
        w_b = 0.5 * (u / 0.5) ** 3 if u < 0.5 else 1 - 0.5 * (1 - (u - 0.5) / 0.5) ** 3
        group_a.add([v.index], 1 - w_b, 'REPLACE')
        group_b.add([v.index], w_b, 'REPLACE')
    modifier = cylinder.modifiers.new("Armature", 'ARMATURE')
    modifier.object = armature
    return cylinder, armature
