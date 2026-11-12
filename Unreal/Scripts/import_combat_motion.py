"""Retarget the licensed Meshy combat set onto the existing game skeletons."""
import sys
from pathlib import Path
import unreal as ue
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_characters import AS, AT, fbx, rig, retarget, bake_clip

SOURCE = 'SourceArt/Animations/Meshy/Combat.fbx'
CLIPS = {
    'Conductor': {'Light':'Right_Hand_Sword_Slash','LightReturn':'Left_Slash','Heavy':'Charged_Slash',
                  'HammerLight':'Right_Hand_Sword_Slash','HammerHeavy':'Heavy_Hammer_Swing','Dodge':'Roll_Dodge'},
    'Automaton': {'Attack':'Boxing_Guard_Prep_Straight_Punch','Hit':'Face_Punch_Reaction'},
    'Faubourgs/SentryRig': {'Attack':'Punch_Forward_with_Both_Fists','Hit':'Face_Punch_Reaction'},
    'Faubourgs/GuardianRig': {'Attack':'Heavy_Hammer_Swing','Hit':'Face_Punch_Reaction'},
}


# Source frames around preparation, contact and recovery, measured from the hand trajectory.
WINDOWS = {
    'Right_Hand_Sword_Slash': (3,17,41,46),
    'Left_Slash': (8,24,60,96),
    'Thrust_Slash': (8,26,38,91),
    'Double_Blade_Spin': (8,21,49,171),
    'Charged_Slash': (12,36,64,67),
    'Heavy_Hammer_Swing': (18,47,56,56),
    'Roll_Dodge': (4,28,53,56),
    'Boxing_Guard_Prep_Straight_Punch': (30,55,86,120),
    'Punch_Forward_with_Both_Fists': (17,43,76,105),
    'Face_Punch_Reaction': (9,23,55,86),
}


def time_contact(clip,suffix,dest):
    pose_api=ue.AnimPoseExtensions
    options=ue.AnimPoseEvaluationOptions()
    space=ue.AnimPoseSpaces.LOCAL
    start,contact,end,total=WINDOWS[suffix]
    contact_phase=.5 if dest=='Attack' else .46 if 'Heavy' in dest else .38
    if dest in ('Dodge','Hit'): contact_phase=.5
    count=ue.AnimationLibrary.get_num_keys(clip)
    length=clip.get_play_length()
    first=pose_api.get_anim_pose_at_time(clip,0.,options)
    bones=pose_api.get_bone_names(first)
    tracks={bone:[] for bone in bones}
    mannequin='pelvis' in {str(b) for b in bones}
    hips='pelvis' if mannequin else 'Hips'
    feet=('foot_r','foot_l') if mannequin else ('RightFoot','LeftFoot')
    roll_contacts=('head','foot_r','foot_l','hand_r','hand_l') if mannequin else ('Head','RightFoot','LeftFoot','RightHand','LeftHand')
    root_pose=pose_api.get_ref_bone_pose(first,'root',space)
    root_world=pose_api.get_ref_bone_pose(first,'root',ue.AnimPoseSpaces.WORLD)
    hip_ref=pose_api.get_ref_bone_pose(first,hips,ue.AnimPoseSpaces.WORLD)
    floor=min(pose_api.get_ref_bone_pose(first,b,ue.AnimPoseSpaces.WORLD).translation.z for b in feet)
    for i in range(count):
        phase=i/(count-1)
        frame=(start+(contact-start)*phase/contact_phase if phase<=contact_phase
               else contact+(end-contact)*(phase-contact_phase)/(1-contact_phase))
        pose=pose_api.get_anim_pose_at_time(clip,length*(frame-1)/(total-1),options)
        hip=pose_api.get_bone_pose(pose,hips,ue.AnimPoseSpaces.WORLD)
        contact_bones=roll_contacts if dest=='Dodge' else feet
        lowest=min(pose_api.get_bone_pose(pose,b,ue.AnimPoseSpaces.WORLD).translation.z for b in contact_bones)
        hip.translation=ue.Vector(hip_ref.translation.x,hip_ref.translation.y,hip.translation.z+floor-lowest)
        hip_local=ue.MathLibrary.make_relative_transform(hip,root_world)
        for bone in bones:
            local=pose_api.get_bone_pose(pose,bone,space)
            if str(bone)=='root':
                local=root_pose
            elif str(bone)==hips:
                local=hip_local
            tracks[bone].append((local.translation,local.rotation,local.scale3d))
    ctrl=clip.controller
    ctrl.open_bracket('Align contact with gameplay',False)
    try:
        for bone,frames in tracks.items():
            positions,rotations,scales=map(list,zip(*frames))
            if not ctrl.set_bone_track_keys(bone,positions,rotations,scales,False):
                raise RuntimeError(f'Could not write {clip.get_name()}:{bone}')
    finally:ctrl.close_bracket(False)


def main():
    for folder, clips in CLIPS.items():
        name=folder.rsplit('/',1)[-1]
        path=f'/Game/Characters/{folder}'
        source_name='ConductorForward' if name=='Conductor' else name
        source_yaw=-90 if name=='Conductor' else 0
        source_path=f'/Game/Characters/CombatSources/{source_name}'
        source=fbx(SOURCE,source_path,'SK_MeshyCombat',True,True,source_yaw)
        target=ue.load_asset(f'{path}/SK_{name}')
        if name.endswith('Rig'):
            target=fbx(f'SourceArt/Characters/Faubourgs/{name}.fbx',path,f'SK_{name}',True)
            original=ue.load_asset(f'/Game/Characters/Faubourgs/{name[:-3]}/SK_{name[:-3]}')
            ue.TrainWorldAuthoring.assign_surface(target,original.materials[0].material_interface)
            AS.save_loaded_asset(target)
            AS.save_loaded_asset(target.skeleton)
        rt=retarget(source,target,f'MeshyCombat_{name}')
        assets=AS.list_assets(source_path)
        print('MESHY_CLIPS',name,[p for p in assets if isinstance(ue.load_asset(p),ue.AnimSequence)])
        for dest,suffix in clips.items():
            src=next(p for p in assets if p.rsplit('.',1)[-1].endswith(suffix))
            clip=bake_clip(source,target,rt,src,f'{path}/Animations',dest)
            time_contact(clip,suffix,dest)
            AS.save_loaded_asset(clip)
        for locomotion in ('Run','Walk'):
            motion_path=f'/Game/Characters/CombatSources/{source_name}{locomotion}'
            motion=fbx(f'SourceArt/Animations/Meshy/{locomotion}.fbx',motion_path,'SK_Motion',True,True,source_yaw)
            motion_rt=retarget(motion,target,f'Meshy{locomotion}_{name}')
            sequence=next(p for p in AS.list_assets(motion_path) if isinstance(ue.load_asset(p),ue.AnimSequence))
            clip=bake_clip(motion,target,motion_rt,sequence,f'{path}/Animations',locomotion)
            AS.save_loaded_asset(clip)
        if name.endswith('Rig'):
            old=ue.load_asset('/Game/Characters/Automaton/SK_Automaton')
            old_rt=retarget(old,target,f'Automaton_{name}')
            for action in ('Idle','Death'):
                clip=bake_clip(old,target,old_rt,f'/Game/Characters/Automaton/Animations/{action}.{action}',f'{path}/Animations',action)
                AS.save_loaded_asset(clip)
    # A relaxed upright idle reads better at the gameplay camera than a permanent crouch.
    idle_path='/Game/Characters/CombatSources/Upright'
    source=fbx('SourceArt/Animations/Quaternius/AL_Standard.fbx',idle_path,'SK_Upright',True,True,-90)
    target=ue.load_asset('/Game/Characters/Conductor/SK_Conductor')
    rt=retarget(source,target,'Upright_Conductor')
    clip=bake_clip(source,target,rt,idle_path+'/SK_UprightRig_Idle_Loop.SK_UprightRig_Idle_Loop','/Game/Characters/Conductor/Animations','Idle')
    AS.save_loaded_asset(clip)
    print('TRAIN_MESHY_COMBAT_IMPORTED')

if __name__=='__main__': main()
