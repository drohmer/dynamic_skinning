# Headless test of the add-on on a rigged cylinder (2 bones), as the Cylinder of the applications
#   blender -b --factory-startup --python blender_addon/tests/test_cylinder.py -- <output directory>
# The second bone rotates by 90° in 0.7 s, then stops: the mesh lags during the motion, then oscillates.
# The timer of the add-on is simulated at 60 Hz; renders (Workbench) are saved at a few instants.

import math
import os
import sys

import bpy
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import dynamic_skinning
from dynamic_skinning import blender_link
from make_cylinder import build_cylinder_scene

output = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "/tmp/ds_test"
os.makedirs(output, exist_ok=True)
dynamic_skinning.register()

cylinder, armature = build_cylinder_scene()

# ---- Dynamic skinning ----
blender_link.enable(cylinder)
state = blender_link.states[cylinder.name]

# ---- Render settings (Workbench), camera above the cylinder ----
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x, scene.render.resolution_y = 480, 360
scene.render.image_settings.file_format = 'PNG'
camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
scene.collection.objects.link(camera)
camera.location = (0.5, 1.0, 9.0)
camera.rotation_euler = (0, 0, 0)
scene.camera = camera
world = bpy.data.worlds.new("World")
scene.world = world


def lbs_only_positions():
    """Positions of the evaluated mesh without the dynamic skinning (modifier disabled)."""
    m = cylinder.modifiers[blender_link.MODIFIER]
    m.show_viewport = False
    bpy.context.view_layer.update()
    evaluated = cylinder.evaluated_get(bpy.context.evaluated_depsgraph_get())
    p = np.array([v.co for v in evaluated.data.vertices])
    m.show_viewport = True
    bpy.context.view_layer.update()
    return p


def evaluated_positions():
    evaluated = cylinder.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return np.array([v.co for v in evaluated.data.vertices])


# ---- Animation: bone B rotates by 90° around z in 0.7 s (smoothstep), then holds ----
params = blender_link.parameters(cylinder)
pose_b = armature.pose.bones["B"]
pose_b.rotation_mode = 'XYZ'
captures = [0.35, 0.7, 0.9, 1.1, 1.4, 3.0]
dt = 1 / 60
results = []
for step in range(int(3.0 / dt) + 1):
    t = step * dt
    u = min(t / 0.7, 1.0)
    pose_b.rotation_euler = (0, 0, math.radians(90) * u * u * (3 - 2 * u))
    bpy.context.view_layer.update()
    state.update(cylinder, params, t)
    bpy.context.view_layer.update()
    if any(abs(t - c) < dt / 2 for c in captures):
        offset = np.linalg.norm(evaluated_positions() - lbs_only_positions(), axis=1)
        results.append((t, offset.max()))
        scene.render.filepath = os.path.join(output, f"cylinder_t{t:.2f}.png")
        bpy.ops.render.render(write_still=True)

for t, d in results:
    print(f"DS_TEST t={t:.2f} s  max offset={d:.4f}")
