# Test of the case: a .blend is opened first, then the add-on is enabled (the dynamic skinning must restart).
# A bone is then rotated by hand (as with the R key), and viewport images are saved.
#   DS_AUTOTEST=<directory> blender --factory-startup --python blender_addon/tests/test_open_then_enable.py -- spot.blend

import math
import os
import sys

import bpy
from mathutils import Quaternion

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

bpy.ops.wm.open_mainfile(filepath=sys.argv[sys.argv.index("--") + 1])

import dynamic_skinning
from dynamic_skinning import blender_link
import autotest

dynamic_skinning.register()   # add-on enabled after the file was opened
obj = next(o for o in bpy.data.objects if o.type == 'MESH' and o.dynamic_skinning.enabled)
armature = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
head = armature.pose.bones["head"]


def drive(t):
    if t > 0.5:
        print_once()
    u = min(max(t - 0.6, 0) / 0.3, 1.0)   # the head turns by 40° in 0.3 s, then stops
    head.rotation_quaternion = Quaternion((0, 0, 1), math.radians(40) * u)


printed = []


def print_once():
    if not printed:
        printed.append(True)
        print(f"DS_RESTART running={obj.name in blender_link.states} timer={bpy.app.timers.is_registered(blender_link.timer)}")


autotest.run(os.environ["DS_AUTOTEST"], [0.55, 0.8, 1.0, 1.2, 1.4], drive, obj)
