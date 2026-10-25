# Automatic test of a demo with the Blender interface (timer of the add-on running as for a user):
# drive(t) moves the rig at the time t (s), viewport images are saved at the given instants, then Blender quits.

import os
import time

import bpy

from dynamic_skinning import blender_link


def run(output, captures, drive, obj, resolution=(480, 360)):
    os.makedirs(output, exist_ok=True)
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = resolution
    for area in bpy.context.window_manager.windows[0].screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
            area.spaces.active.overlay.show_overlays = False
    captures = list(captures)
    start = []

    def step():
        now = time.perf_counter()
        if not start:
            start.append(now)
        t = now - start[0]
        drive(t)
        if captures and t >= captures[0]:
            window = bpy.context.window_manager.windows[0]
            area = next(a for a in window.screen.areas if a.type == 'VIEW_3D')
            with bpy.context.temp_override(window=window, area=area):
                scene.render.filepath = os.path.join(output, f"viewport_t{captures[0]:.2f}.png")
                bpy.ops.render.opengl(write_still=True)
            offset = max(v.vector.length for v in obj.data.attributes[blender_link.ATTRIBUTE].data)
            print(f"DS_AUTOTEST t={t:.2f} s  max offset={offset:.4f}")
            captures.pop(0)
            if not captures:
                bpy.ops.wm.quit_blender()
                return None
        return 1 / 120

    bpy.app.timers.register(step, first_interval=1.0, persistent=True)
