"""Run with Blender in background. Preserve Layer sources; export engine-ready FBX."""
from pathlib import Path
import math
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def export(path):
    # FBX centimetres with unit bone scales. Retargeting must never depend on a
    # removable Blender armature object carrying a scale of 100.
    bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
    armatures=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    for armature in armatures:
        bpy.context.view_layer.objects.active=armature;armature.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT')
        bones=armature.data.edit_bones
        parents=[b for b in bones if b.parent is None]
        root=bones.new('root');root.head=armature.matrix_world.inverted()@Vector((0,0,0));root.tail=root.head+Vector((0,0,.15));root.use_deform=True
        for bone in parents:bone.parent=root;bone.use_connect=False
        bpy.ops.object.mode_set(mode='OBJECT')
    transforms={o:o.matrix_world.copy() for o in bpy.context.scene.objects if o.type in {'MESH','ARMATURE'} and not o.hide_get()}
    for o,world in transforms.items():o.matrix_world=Matrix.Scale(100,4)@world
    bpy.ops.object.select_all(action='DESELECT')
    for o in transforms:o.select_set(True)
    bpy.context.view_layer.objects.active=next(iter(transforms))
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH','ARMATURE'},
        add_leaf_bones=False, bake_anim=False, axis_forward='-Y', axis_up='Z',
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL', mesh_smooth_type='FACE', path_mode='AUTO', embed_textures=False)

clear()
bpy.ops.import_scene.gltf(filepath=str(ROOT/'SourceArt/Characters/Conductor/conductor.glb'))
# Tripo's source is one metre high and faces -X. Normalise to 1.8 m, facing -Y.
xf = Matrix.Rotation(math.pi/2,4,'Z') @ Matrix.Scale(1.8,4)
transforms={o:o.matrix_world.copy() for o in bpy.context.scene.objects if o.type in {'MESH','ARMATURE'}}
for o,world in transforms.items(): o.matrix_world = xf @ world
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in bpy.context.scene.objects:
    if o.type in {'MESH','ARMATURE'}: o.select_set(True)
bpy.context.view_layer.objects.active=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
export(ROOT/'SourceArt/Characters/Conductor/SK_Conductor.fbx')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SourceArt/Characters/Conductor/conductor.blend'))

clear()
bpy.ops.import_scene.gltf(filepath=str(ROOT/'SourceArt/Characters/Automaton/automaton.glb'))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers)]
points=[o.matrix_world@Vector(p) for o in meshes for p in o.bound_box]
height=max(p.z for p in points)-min(p.z for p in points)
low=min(p.z for p in points)
xf=Matrix.Scale(2.1/height,4) @ Matrix.Translation((0,0,-low))
transforms={o:o.matrix_world.copy() for o in bpy.context.scene.objects if o.type in {'MESH','ARMATURE'}}
for o,world in transforms.items(): o.matrix_world=xf@world
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for o in transforms:o.select_set(True)
bpy.context.view_layer.objects.active=next(o for o in transforms if o.type=='ARMATURE')
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
export(ROOT/'SourceArt/Characters/Automaton/SK_Automaton.fbx')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'SourceArt/Characters/Automaton/automaton.blend'))

for name in ['sabre','hammer']:
    clear(); bpy.ops.import_scene.gltf(filepath=str(ROOT/f'SourceArt/Weapons/{name}.glb'))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    for o in meshes:
        # Keep the original model's axes here; grip is measured and authored in its .blend.
        print('WEAPON_BOUNDS',name,o.name,[tuple(round(v,4) for v in (o.matrix_world@Vector(c))) for c in o.bound_box])
        o.matrix_world=Matrix.Translation((.37 if name=='sabre' else .30,0,.03 if name=='hammer' else 0))@o.matrix_world
    export(ROOT/f'SourceArt/Weapons/{name}.fbx')
print('TRAIN_MODELS_PREPARED')
