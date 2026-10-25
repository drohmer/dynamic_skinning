# Demo of the add-on with the Blender interface, on the rigged cylinder
#   blender --python blender_addon/tests/demo_cylinder.py
# Opens the cylinder with the dynamic skinning enabled, the armature in Pose Mode: rotate the bone B (R key)
# to see the oscillations. The parameters are in 3D View > Sidebar (N) > Dyn. Skinning.
#
# With DS_AUTOTEST=<directory>, the bone B is rotated automatically (90° in 0.7 s), viewport images
# are saved at a few instants, then Blender quits (test of the timer of the add-on in the interface).

import math
import os
import sys

import bpy

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dynamic_skinning
from dynamic_skinning import blender_link
from make_cylinder import build_cylinder_scene
import autotest

dynamic_skinning.register()
cylinder, armature = build_cylinder_scene()
blender_link.enable(cylinder)

scene = bpy.context.scene
camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
scene.collection.objects.link(camera)
camera.location, camera.rotation_euler = (0.5, 1.0, 9.0), (0, 0, 0)
scene.camera = camera
bpy.context.view_layer.objects.active = armature
armature.select_set(True)
bpy.ops.object.mode_set(mode='POSE')
armature.pose.bones["B"].rotation_mode = 'XYZ'

output = os.environ.get("DS_AUTOTEST")
if output:
    def drive(t):
        u = min(t / 0.7, 1.0)
        armature.pose.bones["B"].rotation_euler = (0, 0, math.radians(90) * u * u * (3 - 2 * u))

    autotest.run(output, [0.35, 0.7, 0.9, 1.1, 1.4, 3.0], drive, cylinder)
