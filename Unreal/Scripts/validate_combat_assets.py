"""Validate imported assets, including the axis/ground regressions found in gameplay.

Run with UnrealEditor-Cmd -run=pythonscript -script=Scripts/validate_combat_assets.py.
"""
import unreal as ue

POSE = ue.AnimPoseExtensions
WORLD = ue.AnimPoseSpaces.WORLD
OPTIONS = ue.AnimPoseEvaluationOptions()


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


for folder, name, contact, axis in (
    ('ConductorUE5', 'Light', .38, 'x'),
    ('ConductorUE5', 'Heavy', .46, 'x'),
    ('Scout', 'Light', .38, 'x'),
    ('Scout', 'LightReturn', .38, 'x'),
    ('Scout', 'Heavy', .46, 'x'),
    ('Scout', 'DualHeavy', .46, 'x'),
    ('Automaton', 'Attack', .5, 'y'),
    ('Faubourgs/SentryRig', 'Attack', .5, 'y'),
    ('Faubourgs/GuardianRig', 'Attack', .5, 'y'),
):
    clip = ue.load_asset(f'/Game/Characters/{folder}/Animations/{name}')
    check(isinstance(clip, ue.AnimSequence), f'Missing {folder}/{name}')
    pose = POSE.get_anim_pose_at_time(clip, clip.get_play_length()*contact, OPTIONS)
    ue5=folder in ('Scout','ConductorUE5')
    feet=('foot_l','foot_r') if ue5 else ('LeftFoot','RightFoot')
    hand_bone='hand_r' if ue5 else 'RightHand'
    hip_bone='pelvis' if ue5 else 'Hips'
    floor = min(POSE.get_ref_bone_pose(pose, b, WORLD).translation.z for b in feet)
    check(5 < floor < 35, f'{folder}: skeleton reference has floating feet ({floor})')
    hand = POSE.get_bone_pose(pose, hand_bone, WORLD).translation
    hip = POSE.get_bone_pose(pose, hip_bone, WORLD).translation
    check(getattr(hand-hip, axis) > 15, f'{folder}/{name}: contact points away from target')
    for frame in range(21):
        pose = POSE.get_anim_pose_at_time(clip, clip.get_play_length()*frame/20, OPTIONS)
        foot = min(POSE.get_bone_pose(pose, b, WORLD).translation.z for b in feet)
        check(abs(foot-floor) < 5, f'{folder}/{name}: foot leaves ground at sample {frame}: {foot}')
    print('TRAIN_ASSET_OK', folder, name)

bench = ue.load_asset('/Game/Props/WorkshopBench')
check(isinstance(bench, ue.StaticMesh), 'Missing workshop bench')
textures = {str(p.parameter_info.name): p.parameter_value
            for slot in bench.static_materials
            for p in slot.material_interface.texture_parameter_values}
check('NormalTexture' in textures and textures['NormalTexture'], 'Bench normal map is not assigned')
check(textures['NormalTexture'].compression_settings == ue.TextureCompressionSettings.TC_NORMALMAP,
      'Bench normal texture has incorrect compression')
check('MetallicRoughnessTexture' in textures and textures['MetallicRoughnessTexture'],
      'Bench metallic/roughness texture is not assigned')
print('TRAIN_COMBAT_ASSETS_VALIDATED')

normal=ue.load_asset('/Game/Characters/Scout/T_Normal')
check(normal and normal.compression_settings==ue.TextureCompressionSettings.TC_NORMALMAP and not normal.srgb,'Scout normal map is not linear normal data')
check(normal.get_editor_property('flip_green_channel'),'Scout glTF normal must be converted to DirectX tangent convention')
material=ue.load_asset('/Game/Characters/Scout/M_Scout')
normal_input=ue.MaterialEditingLibrary.get_material_property_input_node(material,ue.MaterialProperty.MP_NORMAL)
check(normal_input and normal_input.texture==normal,'Scout normal map is not connected to its material')
for name in ('Idle','Run','Light','LightReturn','Heavy','DualHeavy','Dodge','Hit','Death'):
    clip=ue.load_asset('/Game/Characters/Scout/Animations/'+name)
    check(clip and clip.get_editor_property('skeleton')==ue.load_asset('/Game/Characters/Scout/SK_Scout').skeleton,'Scout animation uses the wrong skeleton: '+name)
print('TRAIN_SCOUT_ASSETS_VALIDATED')

import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('validate_scout_idle.py')))
