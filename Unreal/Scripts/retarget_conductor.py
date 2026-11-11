"""Bake the conductor's native Unreal sequences from aligned Quaternius sources."""

from math import cos, tau
import unreal as ue

ROOT = '/Game/Characters/Conductor'
AS = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
AT = ue.AssetToolsHelpers.get_asset_tools()
POSE = ue.AnimPoseExtensions
MATH = ue.MathLibrary

# Animation import and bind pose must have the same facing. UAL1 faces +Y,
# UAL2 faces -Y, and both the conductor mesh and Character Movement face +X.
SOURCES = [
    ('UAL1', 'Quaternius/AL_Standard.fbx', -90, {
        'Idle': 'Sword_Idle', 'Run': 'Jog_Fwd_Loop', 'Walk': 'Walk_Loop',
        'Hit': 'Hit_Chest', 'Death': 'Death01', 'Attack': 'Punch_Cross',
        'Heavy': 'Sword_Attack', 'HammerLight': 'Sword_Attack',
    }),
    ('UAL2', 'Quaternius2/UAL2_Standard.fbx', 90, {
        'Light': 'Sword_Regular_A_Rec',
        'HammerHeavy': 'TreeChopping_Loop', 'Dodge': 'Sword_Dash',
    }),
]


def bake(target, import_fbx, make_rig):
    target_rig = make_rig(target, ROOT, 'IK_SK_Conductor')
    for label, file, yaw, clips in SOURCES:
        folder = f'{ROOT}/RetargetSources/{label}'
        source = import_fbx('SourceArt/Animations/' + file, folder, 'SK_' + label, True, True, yaw)
        source_rig = make_rig(source, folder, 'IK_' + label)
        name = f'RTG_{label}_SK_Conductor'
        rt = (ue.load_asset(f'{ROOT}/{name}') if AS.does_asset_exist(f'{ROOT}/{name}') else
              AT.create_asset(name, ROOT, ue.IKRetargeter, ue.IKRetargetFactory()))
        ctrl = ue.IKRetargeterController.get_controller(rt)
        ctrl.set_ik_rig(ue.RetargetSourceOrTarget.SOURCE, source_rig)
        ctrl.set_ik_rig(ue.RetargetSourceOrTarget.TARGET, target_rig)
        ctrl.add_default_ops()
        ctrl.auto_map_chains(ue.AutoMapChainType.EXACT, True)
        ctrl.auto_align_all_bones(ue.RetargetSourceOrTarget.TARGET)
        AS.save_loaded_asset(rt)
        assets = AS.list_assets(folder)
        used = set()
        for dest, suffix in clips.items():
            src = next(p for p in assets if p.rsplit('.', 1)[-1].endswith('_' + suffix))
            used.add(src)
            inputs = ue.IKRetargetBatchOperationInputs()
            inputs.assets_to_retarget = [ue.EditorAssetLibrary.find_asset_data(src)]
            inputs.source_mesh = source
            inputs.target_mesh = target
            inputs.ik_retarget_asset = rt
            inputs.target_path = ROOT + '/Animations'
            inputs.overwrite_existing_files = True
            inputs.include_referenced_assets = False
            inputs.search = src.rsplit('.', 1)[-1]
            inputs.replace = dest
            outputs = ue.IKRetargetBatchOperation.run_batch_retarget(inputs)
            if len(outputs) != 1 or str(outputs[0].asset_name) != dest:
                raise RuntimeError(f'Unexpected output baking {dest}: {outputs}')
            clip = outputs[0].get_asset()
            clip.set_editor_property('force_root_lock', True)
            clip.set_editor_property('root_motion_root_lock', ue.RootMotionRootLock.REF_POSE)
            clip.set_editor_property('enable_root_motion', False)
            if dest in ('Run', 'HammerHeavy', 'Dodge'):
                pose_weapon_arm(clip, dest)
            AS.save_loaded_asset(clip)
        # Keep the editable source poses and rig for these clips without copying
        # the entire animation pack a second time into this character's folder.
        for path in assets:
            if path not in used and isinstance(ue.load_asset(path), ue.AnimSequence):
                AS.delete_asset(path)
    print('TRAIN_CONDUCTOR_BAKED')


def dodge_source_phase(phase):
    # The movement impulse ends halfway through the montage. Trim the prolonged
    # crouch in the source so the forward step is followed by an immediate recovery.
    keys = ((0., 0.), (.18, .075), (.48, .25), (.65, .70), (1., 1.))
    for (start, first), (end, last) in zip(keys, keys[1:]):
        if phase <= end:
            return first + (last - first) * (phase - start) / (end - start)
    return 1.


def pose_weapon_arm(clip, action):
    """Retain the source footwork; author the arm carrying the actual game prop."""
    options = ue.AnimPoseEvaluationOptions()
    ref_pose = POSE.get_anim_pose_at_time(clip, 0., options)
    space = ue.AnimPoseSpaces.WORLD
    ref = lambda bone: POSE.get_ref_bone_pose(ref_pose, bone, space)
    wrist = ref('RightHand')
    fingers = MATH.inverse_transform_location(wrist, ref('RightHandMiddle1').translation)
    across = (MATH.inverse_transform_location(wrist, ref('RightHandIndex1').translation)
              - MATH.inverse_transform_location(wrist, ref('RightHandPinky1').translation))
    # Match the hand basis used by ATrainPawn::ApplyLoadout. Weapon X is tipward.
    grip_inverse = MATH.make_rot_from_xy(across, fingers).quaternion().inversed()
    upper = ref('RightArm')
    lower = ref('RightForeArm')
    upper_axis = lower.translation - upper.translation
    lower_axis = wrist.translation - lower.translation

    arm_bones = ('RightArm', 'RightForeArm', 'RightHand')
    if action == 'Dodge':
        bones = POSE.get_bone_names(ref_pose)
    else:
        bones = arm_bones if action == 'Run' else ('RightHand',)
    tracks = {bone: [] for bone in bones}
    count = ue.AnimationLibrary.get_num_keys(clip)
    for i in range(count):
        phase = i / (count - 1)
        source_phase = dodge_source_phase(phase) if action == 'Dodge' else phase
        pose = POSE.get_anim_pose_at_time(clip, clip.get_play_length() * source_phase, options)
        world = lambda bone: POSE.get_bone_pose(pose, bone, space).rotation
        if action in ('Run', 'Dodge'):
            # Small carry swing, with the elbow at the side and the tip outside
            # the silhouette throughout the cycle. Left arm and legs stay animated.
            sway = cos(tau * phase) if action == 'Run' else -.4
            upper_q = MATH.quat_find_between_vectors(upper_axis, ue.Vector(-.28 + .16*sway, .42, -1)) * upper.rotation
            lower_q = MATH.quat_find_between_vectors(lower_axis, ue.Vector(.32 + .14*sway, .16, -1)) * lower.rotation
            hand_q = MATH.make_rot_from_xy(ue.Vector(-1, .27, -.23 + .10*sway),
                                          ue.Vector(.26, .1, -1)).quaternion() * grip_inverse
            rotations = (world('RightShoulder').inversed() * upper_q,
                         upper_q.inversed() * lower_q, lower_q.inversed() * hand_q)
            arm_rotations = dict(zip(arm_bones, rotations))
        else:
            # This source is a chopping loop. Adapt the wrist for a hammer head
            # that actually descends and reaches forward at the gameplay hit.
            keys = ((0., (-.75, .1, .65), (.65, 0, .7)),
                    (.20, (.2, .06, 1), (1, 0, -.2)),
                    (.375, (1, 0, -.48), (.5, .15, 1)),
                    (.55, (.75, .28, -.57), (.55, .05, .85)),
                    (.78, (-.64, .55, .35), (.44, .1, .9)),
                    (1., (-.75, .1, .65), (.65, 0, .7)))
            for (a, x1, y1), (b, x2, y2) in zip(keys, keys[1:]):
                if phase <= b:
                    q1 = MATH.make_rot_from_xy(ue.Vector(*x1), ue.Vector(*y1)).quaternion()
                    q2 = MATH.make_rot_from_xy(ue.Vector(*x2), ue.Vector(*y2)).quaternion()
                    hand_q = MATH.quat_slerp(q1, q2, (phase - a) / (b - a)) * grip_inverse
                    break
            arm_rotations = {'RightHand': world('RightForeArm').inversed() * hand_q}
        for bone in bones:
            local = POSE.get_bone_pose(pose, bone, ue.AnimPoseSpaces.LOCAL)
            rotation = arm_rotations.get(str(bone), local.rotation)
            if action == 'Dodge' and str(bone) in arm_rotations:
                hold = min(1., phase * 9., (1. - phase) * 5.)
                rotation = MATH.quat_slerp(local.rotation, rotation, hold)
            tracks[bone].append((local.translation, rotation.normalized(), local.scale3d))

    controller = clip.controller
    controller.open_bracket('Author conductor weapon pose', False)
    try:
        for bone, frames in tracks.items():
            translations, rotations, scales = map(list, zip(*frames))
            if not controller.set_bone_track_keys(bone, translations, rotations, scales, False):
                raise RuntimeError(f'Failed writing {clip.get_name()}:{bone}')
    finally:
        controller.close_bracket(False)
