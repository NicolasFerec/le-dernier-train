"""Normalize Meshy characters to centimetres with a ground root before retargeting."""
from pathlib import Path
import bpy
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent.parent
for name in ('Sentry','Guardian'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(ROOT/'SourceArt/Characters/Faubourgs'/f'{name}.glb'))
    objects=[o for o in bpy.context.scene.objects if o.type=='ARMATURE' or (o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers))]
    for o in list(bpy.context.scene.objects):
        if o not in objects:bpy.data.objects.remove(o,do_unlink=True)
    rig=next(o for o in objects if o.type=='ARMATURE')
    for o in objects:o.animation_data_clear()
    rig.data.pose_position='REST'
    points=[o.matrix_world@Vector(c) for o in objects if o.type=='MESH' for c in o.bound_box]
    low=min(p.z for p in points);height=max(p.z for p in points)-low
    transform=Matrix.Scale(205/height,4)@Matrix.Translation((0,0,-low))
    worlds={o:o.matrix_world.copy() for o in objects}
    for o,world in worlds.items():o.matrix_world=transform@world
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.object.mode_set(mode='EDIT')
    bones=rig.data.edit_bones
    roots=[b for b in bones if b.parent is None]
    root=bones.new('root');root.head=(0,0,0);root.tail=(0,0,15)
    for bone in roots:bone.parent=root;bone.use_connect=False
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.data.pose_position='POSE'
    bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
    bpy.ops.export_scene.fbx(filepath=str(ROOT/'SourceArt/Characters/Faubourgs'/f'{name}Rig.fbx'),
        use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,
        axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL')
    print('NORMALIZED_RIG',name,height)
