"""Reject staggered/sliding feet, backward knees and a discontinuous idle loop."""
import json,math
from pathlib import Path
import unreal as ue

P=ue.AnimPoseExtensions;W=ue.AnimPoseSpaces.WORLD;O=ue.AnimPoseEvaluationOptions()
clip=ue.load_asset('/Game/Characters/Scout/Animations/Idle')
mesh=ue.load_asset('/Game/Characters/Scout/SK_Scout')
assert clip.get_editor_property('skeleton')==mesh.skeleton,'Scout idle uses another skeleton'
poses=[P.get_anim_pose_at_time(clip,clip.get_play_length()*i/120,O) for i in range(121)]
def xyz(v):return [v.x,v.y,v.z]
def norm(v):return math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z)
def dot(a,b):return a.x*b.x+a.y*b.y+a.z*b.z
max_drift=0.;max_stagger=0.;angles=[];head=[]
for pose in poses:
    at=lambda b:P.get_bone_pose(pose,b,W).translation
    ref=lambda b:P.get_ref_bone_pose(pose,b,W).translation
    for side in ('l','r'):
        for bone in ('foot_'+side,'ball_'+side):
            error=norm(at(bone)-ref(bone));max_drift=max(max_drift,error)
            assert error<.1,(bone,'idle must keep both soles planted',error)
        thigh,knee,foot=(at(b+'_'+side) for b in ('thigh','calf','foot'))
        a=thigh-knee;b=foot-knee
        bend=180-math.degrees(math.acos(max(-1,min(1,dot(a,b)/(norm(a)*norm(b))))))
        angles.append(bend)
        assert 10<bend<28,(side,'knee flexion',bend)
        assert knee.x>max(thigh.x,foot.x)+3,(side,'knee must bend forward')
    stagger=abs(at('foot_l').x-at('foot_r').x);max_stagger=max(max_stagger,stagger)
    assert stagger<5,('staggered resting feet',stagger)
    assert abs(at('pelvis').x-(at('foot_l').x+at('foot_r').x)/2)<10,'pelvis outside support'
    head.append(at('head'))
for bone in P.get_bone_names(poses[0]):
    a=P.get_bone_pose(poses[0],bone,W);b=P.get_bone_pose(poses[-1],bone,W)
    assert norm(a.translation-b.translation)<.01,(bone,'loop translation seam')
    assert abs((a.rotation*b.rotation.inversed()).w)>.99999,(bone,'loop rotation seam')
assert max(norm(h-head[0]) for h in head)>.05,'Idle is a frozen pose'
report={'asset':clip.get_path_name(),'samples':121,'max_sole_drift_cm':max_drift,
        'max_feet_stagger_cm':max_stagger,'knee_bend_degrees':[min(angles),max(angles)],
        'loop_continuity':True,'breathing':True}
root=Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
(root/'SourceArt/Characters/Scout/idle-validation.json').write_text(json.dumps(report,indent=2))
print('TRAIN_SCOUT_IDLE_VALIDATED',json.dumps(report))
