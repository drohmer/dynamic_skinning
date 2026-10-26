Dynamic Skinning adds secondary motion to a rigged character, live in the viewport: soft parts lag behind during a motion, then oscillate and settle when the motion stops. The ears of a character keep swinging after a turn of the head, a belly jiggles after a jump, a tail follows the body.

The effect is driven only by the velocity of the bones. There is no simulation to set up, no collision object, no cache, and no change to your rig or to your skinning weights: the add-on works on top of the usual Armature modifier.

## Features

- **One click**: select a mesh deformed by an Armature modifier and click *Enable*.
- **Interactive**: the mesh reacts while you rotate a bone in Pose Mode, and during the playback of the animation.
- **Intuitive parameters**, editable while the mesh moves: frequency, damping and amplitude of the oscillations, drag during the motion, and overall bend strength.
- **Per-vertex control**: paint an optional `DS_bend` vertex group to choose which parts are soft.
- **Non-destructive**: the effect is a Geometry Nodes modifier added after the Armature modifier; *Disable* removes it.
- **Self-contained**: pure Python and NumPy (shipped with Blender), no internet access, no file access.

## How to use

1. Select a mesh deformed by an *Armature* modifier (vertex groups named after the bones).
2. In *Properties > Object > Dynamic Skinning*, or in the 3D View sidebar (*N > Dyn. Skinning*), click *Enable*.
3. Switch the armature to Pose Mode and rotate a bone, or play the animation.

*Reset* recomputes the weights after a change of the rig or of the mesh. The setting is saved in the .blend file: the effect restarts by itself when the file is opened.

Example files (a rigged cylinder and Spot the cow) are available in the [repository](https://github.com/drohmer/dynamic_skinning/tree/main/blender_addon).

## Limitations

This is a first version of the method, meant for interactive posing and previews:

- the effect runs in real time, so the result of a playback depends on the playback speed, and there is no bake yet for a reproducible final render;
- the deformation is recomputed with linear blend skinning: Dual Quaternion skinning and shape keys before the Armature modifier are not taken into account.

## About

The add-on implements the research paper *Dynamic Skinning: Kinematics-Driven Cartoon Effects for Articulated Characters*, D. Rohmer, K. Salem, N. Kalyanasundaram, V. Zordan, IEEE Transactions on Visualization and Computer Graphics, 2026 ([DOI](https://doi.org/10.1109/TVCG.2026.3653317), [project page](https://velocityskinning.com/), [interactive web demo](https://velocityskinning.com/demo/)). Source code, paper and standalone applications: [github.com/drohmer/dynamic_skinning](https://github.com/drohmer/dynamic_skinning).
