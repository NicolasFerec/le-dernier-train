"""Editor commandlet: import Layer meshes and CC0 animation source, then native IK retarget."""
from pathlib import Path
import unreal as ue
ROOT=Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
AS=ue.get_editor_subsystem(ue.EditorAssetSubsystem)
AT=ue.AssetToolsHelpers.get_asset_tools()

def fbx(file,path,name,skeletal=False,animations=False):
    force='TrainReimport' in ue.SystemLibrary.get_command_line() and not animations
    if AS.does_asset_exist(path+'/'+name) and not force: return ue.load_asset(path+'/'+name)
    if AS.does_asset_exist(path+'/'+name+'1'):
        AS.rename_asset(path+'/'+name+'1',path+'/'+name)
        return ue.load_asset(path+'/'+name)
    task=ue.AssetImportTask();task.filename=str(ROOT/file);task.destination_path=path;task.destination_name=name
    task.automated=True;task.save=True;task.replace_existing=True;task.replace_existing_settings=True
    opt=ue.FbxImportUI();opt.import_mesh=True;opt.import_as_skeletal=skeletal;opt.import_animations=animations
    opt.import_materials=True;opt.import_textures=False;opt.automated_import_should_detect_type=False
    opt.mesh_type_to_import=ue.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else ue.FBXImportType.FBXIT_STATIC_MESH
    if not skeletal: opt.static_mesh_import_data.combine_meshes=True
    task.options=opt;AT.import_asset_tasks([task])
    print('IMPORTED',name,task.imported_object_paths)
    assets=[ue.load_asset(p) for p in task.imported_object_paths]
    mesh=next((a for a in assets if isinstance(a,ue.SkeletalMesh if skeletal else ue.StaticMesh)),None)
    if mesh and mesh.get_name()!=name:
        if AS.does_asset_exist(path+'/'+name):AS.delete_asset(path+'/'+name)
        AS.rename_loaded_asset(mesh,path+'/'+name)
    return mesh


LIB=ue.MaterialEditingLibrary

def texture(file,path,name):
    if AS.does_asset_exist(path+'/'+name): return ue.load_asset(path+'/'+name)
    task=ue.AssetImportTask();task.filename=str(file);task.destination_path=path;task.destination_name=name;task.automated=True;task.save=True
    AT.import_asset_tasks([task]);return ue.load_asset(task.imported_object_paths[0])

def surface(mesh,file,name):
    t=texture(ROOT/file,mesh.get_path_name().rsplit('/',1)[0],'T_'+name)
    path=mesh.get_path_name().rsplit('/',1)[0];m=ue.load_asset(path+'/M_'+name)
    if not m:m=AT.create_asset('M_'+name,path,ue.Material,ue.MaterialFactoryNew())
    # Imported materials are replaced by a project-owned atlas surface.
    # Leave any previous nodes in place on reimport (native deletion can touch rooted imports).
    tex=LIB.create_material_expression(m,ue.MaterialExpressionTextureSample);tex.texture=t
    LIB.connect_material_property(tex,'RGB',ue.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(ue.MaterialProperty.MP_ROUGHNESS,.62),(ue.MaterialProperty.MP_METALLIC,.15)]:
        v=LIB.create_material_expression(m,ue.MaterialExpressionConstant);v.r=value;LIB.connect_material_property(v,'',prop)
    LIB.recompile_material(m);AS.save_loaded_asset(m)
    if isinstance(mesh,ue.SkeletalMesh):
        LIB.set_material_usage(m,ue.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        LIB.recompile_material(m);AS.save_loaded_asset(m)
        ue.TrainWorldAuthoring.assign_surface(mesh,m)
    else:mesh.set_material(0,m)
    AS.save_loaded_asset(mesh)


def rig(mesh,path,name):
    r=ue.load_asset(path+'/'+name) if AS.does_asset_exist(path+'/'+name) else AT.create_asset(name,path,ue.IKRigDefinition,ue.IKRigDefinitionFactory())
    c=ue.IKRigController.get_controller(r);c.set_skeletal_mesh(mesh)
    auto=c.apply_auto_generated_retarget_definition();print('AUTO_CHAINS',name,auto,[str(a.chain_name) for a in c.get_retarget_chains()])
    AS.save_loaded_asset(r);return r


def retarget(source,target,anims,label):
    source_path=source.get_path_name().rsplit('/',1)[0];target_path=target.get_path_name().rsplit('/',1)[0]
    src=rig(source,source_path,'IK_'+source.get_name());dst=rig(target,target_path,'IK_'+target.get_name())
    name='RTG_'+label;path=target_path+'/'+name
    rt=ue.load_asset(path) if AS.does_asset_exist(path) else AT.create_asset(name,target_path,ue.IKRetargeter,ue.IKRetargetFactory())
    c=ue.IKRetargeterController.get_controller(rt);c.set_ik_rig(ue.RetargetSourceOrTarget.SOURCE,src);c.set_ik_rig(ue.RetargetSourceOrTarget.TARGET,dst)
    c.add_default_ops();c.auto_map_chains(ue.AutoMapChainType.FUZZY,True);c.auto_align_all_bones(ue.RetargetSourceOrTarget.TARGET)
    AS.save_loaded_asset(rt)
    inputs=ue.IKRetargetBatchOperationInputs();inputs.assets_to_retarget=[ue.EditorAssetLibrary.find_asset_data(a.get_path_name()) for a in anims]
    inputs.source_mesh=source;inputs.target_mesh=target;inputs.ik_retarget_asset=rt
    inputs.target_path=target_path+'/Animations';inputs.overwrite_existing_files=True
    out=ue.IKRetargetBatchOperation.run_batch_retarget(inputs)
    print('RETARGETED',label,len(out));return target_path+'/Animations'

source=fbx('SourceArt/Animations/Quaternius/AL_Standard.fbx','/Game/Characters/AnimationSource','SK_Source',True,True)
source2=fbx('SourceArt/Animations/Quaternius2/UAL2_Standard.fbx','/Game/Characters/AnimationSource2','SK_Source2',True,True)
target=fbx('SourceArt/Characters/Conductor/SK_Conductor.fbx','/Game/Characters/Conductor','SK_Conductor',True)
enemy=fbx('SourceArt/Characters/Automaton/SK_Automaton.fbx','/Game/Characters/Automaton','SK_Automaton',True)
surface(target,'SourceArt/Characters/Conductor/conductor_color.jpg','Conductor')
surface(enemy,'SourceArt/Characters/Automaton/automaton_color.jpg','Automaton')
for name in ['sabre','hammer']:
    mesh=fbx('SourceArt/Weapons/'+name+'.fbx','/Game/Weapons',name)
    surface(mesh,'SourceArt/Weapons/'+name+'_color.jpg',name)

clips1={'Idle':'Sword_Idle','Run':'Jog_Fwd_Loop','Walk':'Walk_Loop','Dodge':'Roll','Hit':'Hit_Chest','Death':'Death01','Attack':'Punch_Cross','HammerLight':'Sword_Attack'}
clips2={'Light':'Sword_Regular_A','Heavy':'Sword_Regular_C','HammerHeavy':'TreeChopping_Loop'}
for mesh in [target,enemy]:
    for src,clips in [(source,clips1),(source2,clips2)]:
        folder=src.get_path_name().rsplit('/',1)[0]
        anims=[ue.load_asset(p) for p in AS.list_assets(folder,recursive=True) if isinstance(ue.load_asset(p),ue.AnimSequence)]
        selected={name:next(a for a in anims if a.get_name().endswith('_'+suffix)) for name,suffix in clips.items()}
        out=retarget(src,mesh,list(selected.values()),src.get_name()+'_'+mesh.get_name())
        for name,srcanim in selected.items():
            dest=out+'/'+name
            if AS.does_asset_exist(dest):AS.delete_asset(dest)
            asset=AS.duplicate_asset(out+'/'+srcanim.get_name(),dest)
            asset.set_editor_property('force_root_lock',True)
            asset.set_editor_property('root_motion_root_lock',ue.RootMotionRootLock.REF_POSE)
            asset.set_editor_property('enable_root_motion',False)
            AS.save_loaded_asset(asset)
    print('MESH_BOUNDS',mesh.get_name(),mesh.get_bounds())
portrait=texture(ROOT/'SourceArt/Portraits/Conductor.png','/Game/UI/Portraits','Conductor')
AS.save_directory('/Game/Characters',recursive=True)
print('TRAIN_CHARACTERS_IMPORTED')
