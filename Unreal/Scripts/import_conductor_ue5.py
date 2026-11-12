"""Import the matching Meshy UE5 mesh and clips directly onto one skeleton."""
from pathlib import Path
import sys,json,math
import unreal as ue
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_characters import AS,AT,ROOT,fbx
from import_combat_motion import time_contact
world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
ue.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
PATH='/Game/Characters/ConductorUE5'
SOURCE=ROOT/'SourceArt/Characters/Conductor/UE5Rework/FBX'
target=fbx(str((SOURCE/'SK_ConductorUE5.fbx').relative_to(ROOT)),PATH,'SK_ConductorUE5',True,False,-90)
material=ue.load_asset('/Game/Characters/Conductor/M_Conductor')
ue.TrainWorldAuthoring.assign_surface(target,material)
AS.save_loaded_asset(target);AS.save_loaded_asset(target.skeleton)


def import_clip(name):
 task=ue.AssetImportTask();task.filename=str(SOURCE/(name+'.fbx'))
 task.destination_path=PATH+'/Animations';task.destination_name=name
 task.automated=True;task.save=True;task.replace_existing=True;task.replace_existing_settings=True
 task.factory=ue.FbxFactory()
 opt=ue.FbxImportUI();opt.import_mesh=False;opt.import_animations=True;opt.import_as_skeletal=True
 opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=ue.FBXImportType.FBXIT_ANIMATION
 opt.skeleton=target.skeleton;opt.anim_sequence_import_data.import_rotation=ue.Rotator(yaw=-90)
 opt.anim_sequence_import_data.set_editor_property('animation_length',ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
 task.options=opt;AT.import_asset_tasks([task])
 clips=[ue.load_asset(p) for p in task.imported_object_paths if isinstance(ue.load_asset(p),ue.AnimSequence)]
 if len(clips)!=1:raise RuntimeError(f'Expected one clip for {name}: {task.imported_object_paths}')
 clip=clips[0]
 if clip.get_name()!=name:
  path=PATH+'/Animations/'+name
  if AS.does_asset_exist(path):AS.delete_asset(path)
  if not AS.rename_loaded_asset(clip,path):raise RuntimeError('Cannot rename '+name)
 clip.set_editor_property('force_root_lock',True)
 clip.set_editor_property('root_motion_root_lock',ue.RootMotionRootLock.REF_POSE)
 clip.set_editor_property('enable_root_motion',False)
 return clip


def canonical_root(clip, yaw):
 # Meshy's action pack and its bundled locomotion use different object axes.
 # Bake that difference once into the pelvis, with the same stationary root as
 # the mesh. Leaving an animated root at another yaw breaks force_root_lock.
 api=ue.AnimPoseExtensions;options=ue.AnimPoseEvaluationOptions();world=ue.AnimPoseSpaces.WORLD
 first=api.get_anim_pose_at_time(clip,0.,options)
 root=api.get_ref_bone_pose(first,'root',world)
 root_local=api.get_ref_bone_pose(first,'root',ue.AnimPoseSpaces.LOCAL)
 turn=ue.Rotator(yaw=yaw).quaternion();frames=[]
 count=ue.AnimationLibrary.get_num_keys(clip)
 for i in range(count):
  pose=api.get_anim_pose_at_time(clip,clip.get_play_length()*i/(count-1),options)
  hip=api.get_bone_pose(pose,'pelvis',world)
  hip.translation=ue.MathLibrary.quat_rotate_vector(turn,hip.translation)
  hip.rotation=(turn*hip.rotation).normalized()
  frames.append(ue.MathLibrary.make_relative_transform(hip,root))
 c=clip.controller;c.open_bracket('Canonical UE5 ground root and facing',False)
 try:
  c.set_bone_track_keys('root',[root_local.translation]*count,[root_local.rotation]*count,[root_local.scale3d]*count,False)
  c.set_bone_track_keys('pelvis',[f.translation for f in frames],[f.rotation for f in frames],[f.scale3d for f in frames],False)
 finally:c.close_bracket(False)

def carry_weapon(clip):
 api=ue.AnimPoseExtensions;world=ue.AnimPoseSpaces.WORLD;options=ue.AnimPoseEvaluationOptions()
 first=api.get_anim_pose_at_time(clip,0.,options)
 ref=lambda n:api.get_ref_bone_pose(first,n,world)
 upper,lower,hand=ref('upperarm_r'),ref('lowerarm_r'),ref('hand_r')
 upper_axis=lower.translation-upper.translation;lower_axis=hand.translation-lower.translation
 grip=ue.Quat(.8124045834,-.0487389947,-.5760366176,-.0761913282)
 tracks={n:[] for n in ['upperarm_r','lowerarm_r','hand_r']}
 count=ue.AnimationLibrary.get_num_keys(clip)
 for i in range(count):
  phase=i/(count-1);sway=math.cos(2*math.pi*phase)
  pose=api.get_anim_pose_at_time(clip,clip.get_play_length()*phase,options)
  up=ue.MathLibrary.quat_find_between_vectors(upper_axis,ue.Vector(.08+.10*sway,.28,-1))*upper.rotation
  lo=ue.MathLibrary.quat_find_between_vectors(lower_axis,ue.Vector(.8+.08*sway,.1,-.55))*lower.rotation
  wrist=ue.MathLibrary.make_rot_from_xy(ue.Vector(.8,.25,-.5),ue.Vector(.1,.3,-1)).quaternion()*grip.inversed()
  parent=api.get_bone_pose(pose,'clavicle_r',world).rotation
  qs=[parent.inversed()*up,up.inversed()*lo,lo.inversed()*wrist]
  for n,q in zip(tracks,qs):
   p=api.get_bone_pose(pose,n,ue.AnimPoseSpaces.LOCAL);tracks[n].append((p.translation,q.normalized(),p.scale3d))
 c=clip.controller;c.open_bracket('Armed locomotion carry pose',False)
 try:
  for n,frames in tracks.items():
   a,b,d=map(list,zip(*frames));c.set_bone_track_keys(n,a,b,d,False)
 finally:c.close_bracket(False)

def inplace(clip):
 # Remove authored travel from the pelvis; CharacterMovement owns displacement.
 # Keep the source vertical gait and all joint rotations unchanged.
 api=ue.AnimPoseExtensions;options=ue.AnimPoseEvaluationOptions();world=ue.AnimPoseSpaces.WORLD
 first=api.get_anim_pose_at_time(clip,0.,options)
 ref=api.get_ref_bone_pose(first,'pelvis',world)
 root=api.get_ref_bone_pose(first,'root',world)
 count=ue.AnimationLibrary.get_num_keys(clip)
 drop=0.
 if clip.get_name() in ('Run','Walk'):
  floor=min(api.get_ref_bone_pose(first,b,world).translation.z for b in ('foot_r','foot_l'))
  drop=min(api.get_bone_pose(api.get_anim_pose_at_time(clip,clip.get_play_length()*i/(count-1),options),b,world).translation.z
           for i in range(count) for b in ('foot_r','foot_l'))-floor
 positions=[];rotations=[];scales=[]
 for frame in range(ue.AnimationLibrary.get_num_keys(clip)):
  t=clip.get_play_length()*frame/max(1,ue.AnimationLibrary.get_num_keys(clip)-1)
  pose=api.get_anim_pose_at_time(clip,t,options);hip=api.get_bone_pose(pose,'pelvis',world)
  hip.translation=ue.Vector(ref.translation.x,ref.translation.y,hip.translation.z-drop)
  local=ue.MathLibrary.make_relative_transform(hip,root)
  positions.append(local.translation);rotations.append(local.rotation);scales.append(local.scale3d)
 c=clip.controller;c.open_bracket('Native in-place locomotion',False)
 try:
  if not c.set_bone_track_keys('pelvis',positions,rotations,scales,False):raise RuntimeError('Cannot lock pelvis travel')
 finally:c.close_bracket(False)

report={}
for name in ['Idle','Run','Walk','Light','LightReturn','Heavy','HammerHeavy','Dodge','Hit','Death']:
 clip=import_clip(name)
 canonical_root(clip,0 if name in ('Run','Walk') else -90)
 suffix={'Light':'Right_Hand_Sword_Slash','LightReturn':'Left_Slash','Heavy':'Charged_Slash','HammerHeavy':'Heavy_Hammer_Swing','Dodge':'Roll_Dodge'}.get(name)
 if suffix:time_contact(clip,suffix,name)
 elif name!='Death':inplace(clip)
 if name in ('Run','Walk'):carry_weapon(clip)
 AS.save_loaded_asset(clip)
 if clip.get_editor_property('skeleton')!=target.skeleton:raise RuntimeError('Wrong skeleton for '+name)
 report[name]={'duration':clip.get_play_length(),'keys':ue.AnimationLibrary.get_num_keys(clip),'skeleton':clip.get_editor_property('skeleton').get_path_name()}
 print('TRAIN_CONDUCTOR_DIRECT_CLIP',name,report[name])
if AS.does_asset_exist(PATH+'/Animations/HammerLight'):AS.delete_asset(PATH+'/Animations/HammerLight')
AS.duplicate_asset(PATH+'/Animations/Light',PATH+'/Animations/HammerLight')
AS.save_directory(PATH,recursive=True)
(ROOT/'SourceArt/Characters/Conductor/UE5Rework/import-report.json').write_text(json.dumps(report,indent=2))
print('TRAIN_CONDUCTOR_UE5_IMPORTED',target.get_bounds())
import runpy
runpy.run_path(str(Path(__file__).with_name('validate_conductor_ue5.py')))
