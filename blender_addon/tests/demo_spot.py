# Demo of the add-on with the Blender interface, on Spot the cow (assets/spot/ of the applications)
#   blender --factory-startup --python blender_addon/tests/demo_spot.py
# Press Space to play the animation (the head nods, then twists): the ears oscillate.
# In Pose Mode, the bones can also be rotated by hand (R key).
# The parameters are in 3D View > Sidebar (N) > Dyn. Skinning.
#
# With DS_AUTOTEST=<directory>, the animation is played, viewport images are saved at a few instants,
# then Blender quits.

import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dynamic_skinning
from dynamic_skinning import blender_link
from make_spot import build_spot_scene
import autotest

dynamic_skinning.register()
spot, armature = build_spot_scene()
blender_link.enable(spot)

# Camera looking at the head (3/4 front view), textured solid shading
scene = bpy.context.scene
corners = [spot.matrix_world @ Vector(c) for c in spot.bound_box]
center = sum(corners, Vector()) / 8
size = max((c - center).length for c in corners)
camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
scene.collection.objects.link(camera)
camera.location = center + size * Vector(eval(os.environ.get("DS_CAMERA_DIRECTION", "(1.5, 2.3, 0.8)")))
camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
scene.camera = camera
for area in bpy.context.window_manager.windows[0].screen.areas:
    if area.type == 'VIEW_3D':
        area.spaces.active.shading.color_type = 'TEXTURE'
bpy.context.view_layer.objects.active = armature
bpy.ops.object.mode_set(mode='POSE')

output = os.environ.get("DS_AUTOTEST")
if output:
    def drive(t):
        scene.frame_set(1 + int(t * scene.render.fps))

    autotest.run(output, [0.5, 2.3, 2.9, 4.5, 6.0, 6.3, 6.7, 7.2, 8.3], drive, spot)
