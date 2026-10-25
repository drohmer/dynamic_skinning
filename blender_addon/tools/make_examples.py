# Generate the example files blender_addon/examples/cylinder.blend and spot.blend (dynamic skinning enabled)
#   blender -b --factory-startup --python blender_addon/tools/make_examples.py
# The files open with the dynamic skinning running when the add-on is installed (otherwise the mesh is static).

import math
import os
import sys

import bpy
from mathutils import Vector

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(TOOLS, ".."))
sys.path.insert(0, TOOLS)
import dynamic_skinning
from dynamic_skinning import blender_link
from make_cylinder import build_cylinder_scene
from make_spot import build_spot_scene

OUTPUT = os.path.join(TOOLS, "..", "examples")
os.makedirs(OUTPUT, exist_ok=True)
dynamic_skinning.register()


def finalize(obj, armature, camera_direction, filename):
    scene = bpy.context.scene
    for o in [o for o in bpy.data.objects if o.type == 'CAMERA']:
        bpy.data.objects.remove(o)
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    center = sum(corners, Vector()) / 8
    size = max((c - center).length for c in corners)
    camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    scene.collection.objects.link(camera)
    camera.location = center + size * Vector(camera_direction)
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = camera
    # 3D views: through the camera, textured solid shading
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                area.spaces.active.shading.color_type = 'TEXTURE'
                area.spaces.active.region_3d.view_perspective = 'CAMERA'
    blender_link.enable(obj)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode='POSE')
    for image in bpy.data.images:
        if image.filepath:
            image.pack()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUTPUT, filename), compress=True)
    blender_link.disable(obj)


# Cylinder: bone B keyframed (90° in 0.7 s, hold, back), in a loop
cylinder, armature = build_cylinder_scene()
scene = bpy.context.scene
scene.render.fps = 30
bone = armature.pose.bones["B"]
bone.rotation_mode = 'XYZ'
for frame, angle in [(1, 0), (31, 0), (52, 90), (112, 90), (133, 0), (193, 0)]:
    bone.rotation_euler = (0, 0, math.radians(angle))
    bone.keyframe_insert("rotation_euler", frame=frame)
scene.frame_start, scene.frame_end = 1, 192
scene.frame_set(1)
finalize(cylinder, armature, (0.1, 0.25, 4.3), "cylinder.blend")

# Spot: head nod and twist (animation of the applications)
spot, armature = build_spot_scene()
finalize(spot, armature, (1.5, 2.3, 0.8), "spot.blend")
