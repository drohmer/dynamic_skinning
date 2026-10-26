# Renders of the Spot example with the installed extension, at chosen instants of its animation.
#   blender -b <spot.blend> --python render_spot.py -- <out dir> <engine> <res_x> <res_y> <times...|scout>
# For each instant: <out>/ds_<t>.png (dynamic skinning) and <out>/lbs_<t>.png (modifier disabled).
# The 60 Hz timer of the add-on is simulated: the animation is stepped at dt = 1/60 s.
import os
import sys

import bpy
import numpy as np
from bl_ext.user_default.dynamic_skinning import blender_link as link

args = sys.argv[sys.argv.index("--") + 1:]
out, engine, rx, ry = args[0], args[1], int(args[2]), int(args[3])
os.makedirs(out, exist_ok=True)

scene = bpy.context.scene
fps = scene.render.fps / scene.render.fps_base
duration = (scene.frame_end - scene.frame_start) / fps
captures = [float(t) for t in args[4:]] if args[4] != "scout" else list(np.arange(0.25, duration, 0.25))

obj = next(o for o in scene.objects if o.type == 'MESH' and o.dynamic_skinning.enabled)
state = link.states[obj.name]
params = link.parameters(obj)
print(f"DS_INFO {obj.name} fps={fps} duration={duration:.2f}s engine={engine}")

scene.render.engine = engine
scene.render.resolution_x, scene.render.resolution_y = rx, ry
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
if engine == 'BLENDER_WORKBENCH':
    scene.display.shading.color_type = 'TEXTURE'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.show_shadows = True
    scene.display.shading.shadow_intensity = 0.3
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'WORLD'
    scene.display.render_aa = '32'
else:
    scene.eevee.taa_render_samples = 64
    scene.render.use_high_quality_normals = True
    scene.view_settings.view_transform = 'AgX'
# Camera pulled back from the center of the mesh (DS_ZOOM > 1: wider), shifted (DS_SHIFT_X, in frame widths)
from mathutils import Vector
corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
center = sum(corners, Vector()) / 8
camera = scene.camera
camera.location = center + (camera.location - center) * float(os.environ.get("DS_ZOOM", "1"))
camera.data.shift_x = float(os.environ.get("DS_SHIFT_X", "0"))
camera.data.shift_y = float(os.environ.get("DS_SHIFT_Y", "0"))
# The armature is not rendered (only the mesh)
for o in scene.objects:
    if o.type == 'ARMATURE':
        o.hide_render = True

modifier = obj.modifiers[link.MODIFIER]
dt = 1 / 60
step, done = 0, set()
while captures and step * dt <= max(captures) + dt:
    t = step * dt
    scene.frame_set(scene.frame_start + int(t * fps), subframe=(t * fps) % 1.0)
    state.update(obj, params, t)
    for c in captures:
        if c not in done and abs(t - c) < dt / 2:
            done.add(c)
            for name, show in (("ds", True), ("lbs", False)):
                modifier.show_render = show
                scene.render.filepath = os.path.join(out, f"{name}_{c:05.2f}.png")
                bpy.ops.render.render(write_still=True)
            modifier.show_render = True
            print(f"DS_RENDER t={c:.2f}", flush=True)
    step += 1
