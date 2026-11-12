"""Import the Scout PBR rig and retarget the Meshy action pack to its ground root."""
import sys
from pathlib import Path
import unreal as ue
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_characters import AS,AT,LIB,ROOT,fbx,texture,retarget,bake_clip
from import_combat_motion import time_contact,WINDOWS
PATH='/Game/Characters/Scout'
SOURCE=ROOT/'SourceArt/Characters/Scout'
target=fbx('SourceArt/Characters/Scout/SK_Scout.fbx',PATH,'SK_Scout',True,False,-90)
mat=ue.load_asset(PATH+'/M_Scout') or AT.create_asset('M_Scout',PATH,ue.Material,ue.MaterialFactoryNew())
for name in ('BaseColor','Normal','ORM'):
    file=next(SOURCE.glob(name+'.*'))
    tex=texture(file,PATH,'T_'+name)
    tex.set_editor_property('max_texture_size',2048)
    if name=='Normal':
        tex.compression_settings=ue.TextureCompressionSettings.TC_NORMALMAP;tex.srgb=False
        tex.set_editor_property('flip_green_channel',True)
    elif name=='ORM':
        tex.compression_settings=ue.TextureCompressionSettings.TC_MASKS;tex.srgb=False
    AS.save_loaded_asset(tex)
    sample=LIB.create_material_expression(mat,ue.MaterialExpressionTextureSample);sample.texture=tex
    if name=='Normal':
        sample.sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL
        LIB.connect_material_property(sample,'RGB',ue.MaterialProperty.MP_NORMAL)
    elif name=='ORM':
        sample.sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_MASKS
        LIB.connect_material_property(sample,'G',ue.MaterialProperty.MP_ROUGHNESS)
        LIB.connect_material_property(sample,'B',ue.MaterialProperty.MP_METALLIC)
    else:LIB.connect_material_property(sample,'RGB',ue.MaterialProperty.MP_BASE_COLOR)
LIB.set_material_usage(mat,ue.MaterialUsage.MATUSAGE_SKELETAL_MESH);LIB.recompile_material(mat)
ue.TrainWorldAuthoring.assign_surface(target,mat);AS.save_loaded_asset(mat);AS.save_loaded_asset(target);AS.save_loaded_asset(target.skeleton)
texture(SOURCE/'Reference.png','/Game/UI/Portraits','Scout')
source_path=PATH+'/Source'
source=fbx('SourceArt/Characters/Scout/FingerAnimations/animation_fbx.fbx',source_path,'SK_Actions',True,True,-90)
rt=retarget(source,target,'ScoutActions')
# Keep the resting soles on the same plane as the timed combat clips.
WINDOWS['Idle']=(1,47,121,121)
sequences=[p for p in AS.list_assets(source_path) if isinstance(ue.load_asset(p),ue.AnimSequence)]
print('SCOUT_SOURCE_CLIPS',[(p,ue.load_asset(p).get_play_length(),ue.AnimationLibrary.get_num_keys(ue.load_asset(p))) for p in sequences])
clips={'Idle':'Idle','Light':'Right_Hand_Sword_Slash','LightReturn':'Left_Slash','Heavy':'Thrust_Slash','Dodge':'Roll_Dodge','Death':'Dead','Hit':'Face_Punch_Reaction','DualHeavy':'Double_Blade_Spin'}
for dest,suffix in clips.items():
    matches=[p for p in sequences if p.rsplit('.',1)[-1].endswith(suffix)]
    if len(matches)!=1:raise RuntimeError(f'Ambiguous {suffix}: {matches}')
    clip=bake_clip(source,target,rt,matches[0],PATH+'/Animations',dest)
    if suffix in WINDOWS:time_contact(clip,suffix,dest)
    AS.save_loaded_asset(clip)
for action,file in [('Run','running_fbx.fbx'),('Walk','walking_fbx.fbx')]:
    folder=PATH+'/Source'+action
    src=fbx('SourceArt/Characters/Scout/UE5Rig/'+file,folder,'SK_'+action,True,True,-90)
    rt=retarget(src,target,'Scout'+action)
    seq=next(p for p in AS.list_assets(folder) if isinstance(ue.load_asset(p),ue.AnimSequence))
    clip=bake_clip(src,target,rt,seq,PATH+'/Animations',action);AS.save_loaded_asset(clip)
from build_scout_idle import build as build_idle
build_idle()
print('TRAIN_SCOUT_IMPORTED',target.get_bounds())
