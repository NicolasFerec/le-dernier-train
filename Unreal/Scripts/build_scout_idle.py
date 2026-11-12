"""Build a quiet, planted Scout idle on her actual UE5 skeleton.

The imported catalogue idle staggers the feet by 40–45 cm and rolls the right
boot. Solve the two legs against fixed bind-pose ankle targets instead. These
are baked animation keys; there is no runtime foot-placement system or new rig.
"""
import math
import shutil
from pathlib import Path
import unreal as ue

API=ue.AnimPoseExtensions
WORLD=ue.AnimPoseSpaces.WORLD
LOCAL=ue.AnimPoseSpaces.LOCAL
OPTIONS=ue.AnimPoseEvaluationOptions()
PATH='/Game/Characters/Scout/Animations/Idle'


def length(v):
    return math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z)


def dot(a,b):
    return a.x*b.x+a.y*b.y+a.z*b.z


def build():
    root=Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
    backup=root/'Saved/Checkpoints/ScoutIdleRepair'
    backup.mkdir(parents=True,exist_ok=True)
    file=root/'Content/Characters/Scout/Animations/Idle.uasset'
    if not (backup/file.name).exists():shutil.copy2(file,backup/file.name)
    clip=ue.load_asset(PATH)
    pose=API.get_anim_pose_at_time(clip,0.,OPTIONS)
    names=[str(n) for n in API.get_bone_names(pose)]
    ref={n:API.get_ref_bone_pose(pose,n,WORLD) for n in names}
    local={n:API.get_ref_bone_pose(pose,n,LOCAL) for n in names}
    # Keep the existing hand/finger grip and relaxed arm pose at the beginning
    # of the clip. Spine/head and both legs return to their neutral reference.
    for n in names:
        if any(n.startswith(prefix) for prefix in ('clavicle_','upperarm_','lowerarm_','hand_','thumb_','index_','middle_','ring_','pinky_')):
            local[n].rotation=API.get_bone_pose(pose,n,LOCAL).rotation
    count=ue.AnimationLibrary.get_num_keys(clip)
    tracks={n:[] for n in names}
    for i in range(count):
        breath=math.sin(2*math.pi*i/(count-1))
        hip_shift=ue.Vector(0,0,-1.0+.12*breath)
        hip=ue.Transform()
        hip.translation=ref['pelvis'].translation+hip_shift
        hip.rotation=ref['pelvis'].rotation
        hip.scale3d=ref['pelvis'].scale3d
        changed={'pelvis':ue.MathLibrary.make_relative_transform(hip,ref['root'])}
        rotations={}
        for side in ('l','r'):
            thigh,calf,foot=('thigh_'+side,'calf_'+side,'foot_'+side)
            a0,k0,e0=(ref[n].translation for n in (thigh,calf,foot))
            a=a0+hip_shift;d=e0-a;distance=length(d);axis=d/distance
            upper=length(k0-a0);lower=length(e0-k0)
            assert abs(upper-lower)<distance<upper+lower,(side,'unreachable ankle')
            along=(upper*upper-lower*lower+distance*distance)/(2*distance)
            pole=ue.Vector(1,0,0);pole=pole-axis*dot(pole,axis);pole=pole/length(pole)
            knee=a+axis*along+pole*math.sqrt(max(0.,upper*upper-along*along))
            up=ue.MathLibrary.quat_find_between_vectors(k0-a0,knee-a)*ref[thigh].rotation
            low=ue.MathLibrary.quat_find_between_vectors(e0-k0,e0-knee)*ref[calf].rotation
            rotations[thigh]=hip.rotation.inversed()*up
            rotations[calf]=up.inversed()*low
            rotations[foot]=low.inversed()*ref[foot].rotation
        # Subtle breathing in the upper body, without yawing the pelvis or
        # dragging a foot. Counter-rotate the head to keep the gaze quiet.
        for bone,pitch in (('spine_03',.55*breath),('head',-.3*breath)):
            world=ref[bone].rotation
            delta=world.inversed()*ue.Rotator(pitch=pitch).quaternion()*world
            rotations[bone]=local[bone].rotation*delta
        for n in names:
            p=changed.get(n,local[n]);q=rotations.get(n,p.rotation)
            tracks[n].append((p.translation,q.normalized(),p.scale3d))
    controller=clip.controller
    controller.open_bracket('Scout neutral planted idle',False)
    try:
        for n,frames in tracks.items():
            positions,rotations,scales=map(list,zip(*frames))
            if not controller.set_bone_track_keys(n,positions,rotations,scales,False):
                raise RuntimeError('Could not write Scout idle bone '+n)
    finally:controller.close_bracket(False)
    clip.set_editor_property('force_root_lock',True)
    clip.set_editor_property('root_motion_root_lock',ue.RootMotionRootLock.REF_POSE)
    clip.set_editor_property('enable_root_motion',False)
    ue.get_editor_subsystem(ue.EditorAssetSubsystem).save_loaded_asset(clip)
    print('TRAIN_SCOUT_IDLE_REBUILT',count,'keys',clip.get_play_length(),'seconds')


if __name__=='__main__':
    build()
    import runpy
    runpy.run_path(str(Path(__file__).with_name('validate_scout_idle.py')))
