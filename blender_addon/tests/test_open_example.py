# Test of an example file with the Blender interface, as a user: the add-on is loaded, the .blend is opened
# (the dynamic skinning restarts by itself), the animation is played and viewport images are saved.
#   DS_AUTOTEST=<directory> blender --factory-startup --python blender_addon/tests/test_open_example.py -- <file.blend>

import os
import sys

import bpy

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dynamic_skinning
from dynamic_skinning import blender_link
import autotest

dynamic_skinning.register()
bpy.ops.wm.open_mainfile(filepath=sys.argv[sys.argv.index("--") + 1])
obj = next(o for o in bpy.data.objects if o.type == 'MESH' and o.dynamic_skinning.enabled)
print(f"DS_OPEN enabled={obj.name} running={obj.name in blender_link.states} "
      f"timer={bpy.app.timers.is_registered(blender_link.timer)}")
scene = bpy.context.scene


def drive(t):
    scene.frame_set(1 + int(t * scene.render.fps))


autotest.run(os.environ["DS_AUTOTEST"], [float(t) for t in os.environ.get("DS_TIMES", "1.5 2.0 2.5").split()], drive, obj)
