# Frames of the preview video: the animation of examples/spot.blend, with and without dynamic skinning.
#   blender -b blender_addon/examples/spot.blend --python render_video.py -- <out dir> [first frame] [last frame]
# Needs the extension installed. Writes <out>/ds/NNNN.png and <out>/lbs/NNNN.png (960x1080, transparent
# background, Workbench) for each frame of the scene; the 60 Hz timer of the add-on is simulated.
# Camera pulled back by DS_ZOOM (environment variable, default 1.1). Progress in <out>/progress.log.
import os
import sys

import bpy
from bl_ext.user_default.dynamic_skinning import blender_link as link
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
scene = bpy.context.scene
first = int(args[1]) if len(args) > 1 else scene.frame_start
last = int(args[2]) if len(args) > 2 else scene.frame_end
for name in ("ds", "lbs"):
    os.makedirs(os.path.join(out, name), exist_ok=True)

fps = scene.render.fps / scene.render.fps_base
obj = next(o for o in scene.objects if o.type == 'MESH' and o.dynamic_skinning.enabled)
state = link.states[obj.name]
params = link.parameters(obj)
modifier = obj.modifiers[link.MODIFIER]

scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x, scene.render.resolution_y = 960, 1080
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
shading = scene.display.shading
shading.color_type, shading.light = 'TEXTURE', 'STUDIO'
shading.show_shadows, shading.shadow_intensity = True, 0.3
shading.show_cavity, shading.cavity_type = True, 'WORLD'
scene.display.render_aa = '16'
for o in scene.objects:
    if o.type == 'ARMATURE':
        o.hide_render = True

corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
center = sum(corners, Vector()) / 8
scene.camera.location = center + (scene.camera.location - center) * float(os.environ.get("DS_ZOOM", "1.1"))

# Simulation at 60 Hz from the start of the animation, render one step out of 60/fps
dt = 1 / 60
step = 0
log = open(os.path.join(out, "progress.log"), "a")
while True:
    t = step * dt
    frame_float = scene.frame_start + t * fps
    if frame_float > last + 1e-6:
        break
    scene.frame_set(int(frame_float), subframe=frame_float % 1.0)
    state.update(obj, params, t)
    frame = round(frame_float)
    if abs(frame_float - frame) < 1e-6 and frame >= first:
        for name, show in (("ds", True), ("lbs", False)):
            modifier.show_render = show
            scene.render.filepath = os.path.join(out, name, f"{frame:04d}.png")
            bpy.ops.render.render(write_still=True)
        modifier.show_render = True
        log.write(f"frame {frame}/{last}\n")
        log.flush()
    step += 1
