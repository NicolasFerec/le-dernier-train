"""Normalize Meshy's UE5 rig and export animations without a second retarget.
Every clip keeps its delivered bone rotations; only units and a stationary ground
root change here. Unreal handles the in-place/contact edit in the import script.
"""
from pathlib import Path
import bpy, json, math
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/Conductor/UE5Rework'
FBX=OUT/'FBX';FBX.mkdir(exist_ok=True)
report=[]

def load(file):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.context.scene.render.fps=30
 bpy.ops.import_scene.gltf(filepath=str(file))
 arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
 meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==arm for m in o.modifiers)]
 actions=list(bpy.data.actions)
 for a in actions:a.use_fake_user=True
 arm.animation_data_clear();arm.data.pose_position='REST';bpy.context.view_layer.update()
 points=[o.matrix_world@Vector(c) for o in meshes for c in o.bound_box]
 low=min(p.z for p in points);height=max(p.z for p in points)-low
 scale=180./height;xf=Matrix.Scale(scale,4)@Matrix.Translation((0,0,-low))
 objects=[arm,*meshes];worlds={o:o.matrix_world.copy() for o in objects}
 for o,w in worlds.items():o.matrix_world=xf@w
 bpy.context.view_layer.update();bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=arm
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 # Armature transform application scales rest bones, but animation translation
 # curves remain in metres unless converted explicitly.
 for a in actions:
  for fc in a.fcurves:
   if fc.data_path.startswith('pose.bones[') and fc.data_path.endswith('.location'):
    for k in fc.keyframe_points:
     k.co.y*=scale;k.handle_left.y*=scale;k.handle_right.y*=scale
 bpy.ops.object.mode_set(mode='EDIT')
 roots=[b for b in arm.data.edit_bones if b.parent is None]
 root=arm.data.edit_bones.new('root');root.head=(0,0,0);root.tail=(0,0,15)
 for b in roots:b.parent=root;b.use_connect=False
 bpy.ops.object.mode_set(mode='OBJECT');arm.data.pose_position='POSE';arm.name='Armature'
 bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
 report.append({'source':str(file.relative_to(OUT)),'height_before_cm':height*100,'scale':scale,'bones':len(arm.data.bones),'animations':[a.name for a in actions]})
 return arm,meshes,actions

def export(name,animation):
 bpy.ops.export_scene.fbx(filepath=str(FBX/(name+'.fbx')),use_selection=True,
  object_types={'ARMATURE','MESH'} if not animation else {'ARMATURE'},add_leaf_bones=False,
  bake_anim=animation,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,
  bake_anim_force_startend_keying=True,bake_anim_simplify_factor=0,
  axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL')

arm,meshes,actions=load(OUT/'Rig/rigged_character.glb');export('SK_ConductorUE5',False)
for filename,clip_names in [('Rig/Walk.glb',{'retarget_clip':'Walk'}),('Rig/Run.glb',{'retarget_clip':'Run'}),('Actions.glb',{
 'Idle_02':'Idle','Right_Hand_Sword_Slash':'Light','Left_Slash':'LightReturn',
 'Charged_Slash':'Heavy','Heavy_Hammer_Swing':'HammerHeavy','Roll_Dodge':'Dodge',
 'Hit_Reaction':'Hit','Dead':'Death'})]:
 arm,meshes,actions=load(OUT/filename)
 for action in actions:
  if action.name not in clip_names:raise RuntimeError('Unexpected animation '+action.name)
  dest=clip_names[action.name];arm.animation_data_create();arm.animation_data.action=action
  if action.slots:arm.animation_data.action_slot=action.slots[0]
  bpy.context.scene.frame_start=math.floor(action.frame_range[0]);bpy.context.scene.frame_end=math.ceil(action.frame_range[1])
  bpy.context.scene.frame_set(bpy.context.scene.frame_start)
  export(dest,True)
  print('CONDUCTOR_UE5_CLIP',dest,tuple(action.frame_range))
(OUT/'normalization.json').write_text(json.dumps(report,indent=2))
print('TRAIN_CONDUCTOR_UE5_PREPARED')
