# Dynamic Skinning for Blender: secondary oscillations of a rigged mesh, driven by the angular velocity of its bones
#
# Usage: select a mesh with an Armature modifier, 3D View > Sidebar (N) > Dyn. Skinning > Enable.
# The mesh then oscillates when its bones move: while posing a bone in Pose Mode, and during the playback.

import bpy

from . import blender_link


class DynamicSkinningSettings(bpy.types.PropertyGroup):
    enabled: bpy.props.BoolProperty(default=False, options={'HIDDEN'})
    # Oscillation kernel κ(t) = A exp(-t/τ) sin(2π f t)
    frequency: bpy.props.FloatProperty(name="Frequency f (Hz)", default=1.5, min=0.0, soft_max=5.0, subtype='NONE',
                                       description="Frequency of the oscillations (Hz)")
    damping: bpy.props.FloatProperty(name="Damping τ (s)", default=0.67, min=0.05, soft_max=5.0,
                                     description="Damping time of the oscillations (s)")
    amplitude: bpy.props.FloatProperty(name="Amplitude A", default=0.88, min=0.0, soft_max=5.0,
                                       description="Amplitude of the oscillations (driven by the changes of velocity)")
    drag: bpy.props.FloatProperty(name="Drag D", default=0.5, min=0.0, soft_max=3.0,
                                  description="Lag during the motion (proportional to the velocity)")
    bend: bpy.props.FloatProperty(name="Bend γ", default=1.0, min=0.0, soft_max=3.0,
                                  description="Magnitude of the bend (multiplies the DS_bend vertex group, 0.5 without group)")


def active_mesh(context):
    obj = context.active_object
    if obj is not None and obj.type == 'MESH':
        return obj
    # Pose Mode: the active object is the armature, look for a selected mesh deformed by it
    if obj is not None and obj.type == 'ARMATURE':
        for o in context.view_layer.objects:
            if o.type == 'MESH' and o.dynamic_skinning.enabled and \
                    any(m.type == 'ARMATURE' and m.object == obj for m in o.modifiers):
                return o
    return None


class DS_OT_enable(bpy.types.Operator):
    bl_idname = "dynamic_skinning.enable"
    bl_label = "Enable"
    bl_description = "Add the dynamic skinning to the mesh (Geometry Nodes modifier after the Armature)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = active_mesh(context)
        if obj is None:
            self.report({'ERROR'}, "Select a mesh with an Armature modifier")
            return {'CANCELLED'}
        try:
            blender_link.enable(obj)
        except ValueError as error:
            self.report({'ERROR'}, str(error))
            return {'CANCELLED'}
        return {'FINISHED'}


class DS_OT_disable(bpy.types.Operator):
    bl_idname = "dynamic_skinning.disable"
    bl_label = "Disable"
    bl_description = "Remove the dynamic skinning from the mesh"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = active_mesh(context)
        if obj is not None:
            blender_link.disable(obj)
        return {'FINISHED'}


class DS_OT_reset(bpy.types.Operator):
    bl_idname = "dynamic_skinning.reset"
    bl_label = "Reset"
    bl_description = "Recompute the weights from the current rig, and stop the oscillations"

    def execute(self, context):
        obj = active_mesh(context)
        if obj is not None and obj.dynamic_skinning.enabled:
            blender_link.enable(obj)
        return {'FINISHED'}


def draw_settings(layout, context):
    obj = active_mesh(context)
    if obj is None:
        layout.label(text="Select a rigged mesh")
        return
    settings = obj.dynamic_skinning
    layout.label(text=obj.name, icon='MESH_DATA')
    if not settings.enabled:
        layout.operator(DS_OT_enable.bl_idname)
        return
    if obj.name not in blender_link.states:
        layout.label(text="Not running: click Reset", icon='ERROR')
    row = layout.row()
    row.operator(DS_OT_disable.bl_idname)
    row.operator(DS_OT_reset.bl_idname)
    column = layout.column()
    column.prop(settings, "frequency")
    column.prop(settings, "damping")
    column.prop(settings, "amplitude")
    column.prop(settings, "drag")
    column.prop(settings, "bend")


# Same settings in the sidebar of the 3D View (N > Dyn. Skinning) and in Properties > Object
class DS_PT_view3d(bpy.types.Panel):
    bl_label = "Dynamic Skinning"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Dyn. Skinning"

    def draw(self, context):
        draw_settings(self.layout, context)


class DS_PT_properties(bpy.types.Panel):
    bl_label = "Dynamic Skinning"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return active_mesh(context) is not None

    def draw(self, context):
        draw_settings(self.layout, context)


classes = (DynamicSkinningSettings, DS_OT_enable, DS_OT_disable, DS_OT_reset, DS_PT_view3d, DS_PT_properties)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.Object.dynamic_skinning = bpy.props.PointerProperty(type=DynamicSkinningSettings)
    bpy.app.handlers.load_post.append(blender_link.on_load)
    # Add-on enabled with a file already open: restart its enabled objects (bpy.data is not available
    # during the registration, hence the timer)
    bpy.app.timers.register(blender_link.restart_enabled, first_interval=0.1)


def unregister():
    for function in (blender_link.timer, blender_link.restart_enabled):
        if bpy.app.timers.is_registered(function):
            bpy.app.timers.unregister(function)
    if blender_link.on_load in bpy.app.handlers.load_post:
        bpy.app.handlers.load_post.remove(blender_link.on_load)
    blender_link.states.clear()
    del bpy.types.Object.dynamic_skinning
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
