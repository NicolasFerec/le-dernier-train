"""Measure the new rig's clips and grip; fail on skeleton/unit regressions."""
import json
from pathlib import Path
import unreal as u
P=u.AnimPoseExtensions;W=u.AnimPoseSpaces.WORLD;O=u.AnimPoseEvaluationOptions()
ROOT=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
PATH='/Game/Characters/ConductorUE5'
mesh=u.load_asset(PATH+'/SK_ConductorUE5')
report={}
def v(p):return [p.x,p.y,p.z]
for name in ['Idle','Run','Walk','Light','LightReturn','Heavy','HammerLight','HammerHeavy','Dodge','Hit','Death']:
 clip=u.load_asset(PATH+'/Animations/'+name)
 assert clip and clip.get_editor_property('skeleton')==mesh.skeleton,name+' wrong skeleton'
 poses=[P.get_anim_pose_at_time(clip,clip.get_play_length()*i/60,O) for i in range(61)]
 names={str(b) for b in P.get_bone_names(poses[0])}
 assert {'root','pelvis','spine_03','hand_r','hand_l','foot_r','foot_l'}<=names,names
 ref=lambda b:P.get_ref_bone_pose(poses[0],b,W)
 floor=min(ref(b).translation.z for b in ['foot_r','foot_l'])
 feet=[min(P.get_bone_pose(p,b,W).translation.z for b in ['foot_r','foot_l']) for p in poses]
 hip=[P.get_bone_pose(p,'pelvis',W).translation for p in poses]
 points=[]
 for i in [0,23,28,60]:
  p=poses[i];h=P.get_bone_pose(p,'hand_r',W);points.append({'phase':i/60,'hand':v(h.translation),'rotation':str(h.rotation.rotator())})
 report[name]={'length':clip.get_play_length(),'bones':len(names),'floor':floor,'foot_min':min(feet),'foot_max':max(feet),'hip_x_span':max(t.x for t in hip)-min(t.x for t in hip),'hip_y_span':max(t.y for t in hip)-min(t.y for t in hip),'hand_samples':points}
 assert 4<floor<35,(name,'bad units/ground',floor)
 for pose in poses:
  root=P.get_bone_pose(pose,'root',W);refroot=P.get_ref_bone_pose(pose,'root',W)
  q=root.rotation*refroot.rotation.inversed()
  assert abs(q.w)>.9999,(name,'root facing differs from bind pose')
 if name in ['Idle','Run','Walk']:
  for pose in poses:
   forward=P.get_bone_pose(pose,'headfront',W).translation-P.get_bone_pose(pose,'head',W).translation
   assert forward.x>7,(name,'body does not face movement direction',forward)
 if name in ['Light','LightReturn','Heavy','HammerHeavy']:
  assert max(abs(f-floor) for f in feet)<5,(name,'ground contact',feet)
 if name=='Run':
  samples=[]
  for b in ['foot_r','foot_l']:
   xyz=[P.get_bone_pose(p,b,W).translation for p in poses]
   for a,c in zip(xyz,xyz[1:]):
    speed=-(c.x-a.x)/(clip.get_play_length()/60)
    if min(a.z,c.z)<floor+6 and speed>20:samples.append(speed)
  report[name]['planted_stride_speed_cm_s']=sorted(samples)[len(samples)//2] if samples else None
 print('TRAIN_UE5_ASSET',name,report[name])
# Preserve the anatomical grip basis from the original bind mesh, expressed in
# the new hand's local space. Partial-finger rigs have no index/pinky landmarks.
old=u.load_asset('/Game/Characters/Conductor/Animations/Idle')
a=P.get_anim_pose_at_time(old,0.,O);b=P.get_anim_pose_at_time(u.load_asset(PATH+'/Animations/Idle'),0.,O)
oh=P.get_ref_bone_pose(a,'RightHand',W);nh=P.get_ref_bone_pose(b,'hand_r',W)
point=lambda n:u.MathLibrary.inverse_transform_location(oh,P.get_ref_bone_pose(a,n,W).translation)
across=point('RightHandIndex1')-point('RightHandPinky1');finger=point('RightHandMiddle1')
worldaxes=oh.rotation*u.MathLibrary.make_rot_from_xy(across,finger).quaternion()
local=nh.rotation.inversed()*worldaxes
middle=u.MathLibrary.inverse_transform_location(nh,P.get_ref_bone_pose(b,'middle_01_r',W).translation)
report['grip']={'rotation_xyzw':[local.x,local.y,local.z,local.w],'center_local':v(middle*.65),'rotator':str(local.rotator())}
print('TRAIN_UE5_GRIP',report['grip'])
(ROOT/'SourceArt/Characters/Conductor/UE5Rework/validation.json').write_text(json.dumps(report,indent=2))
print('TRAIN_CONDUCTOR_UE5_ASSETS_VALIDATED')
