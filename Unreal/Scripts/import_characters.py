"""Editor commandlet: import Layer meshes and CC0 animation source, then native IK retarget."""
from pathlib import Path
import unreal as ue
ROOT=Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
AS=ue.get_editor_subsystem(ue.EditorAssetSubsystem)
AT=ue.AssetToolsHelpers.get_asset_tools()

def fbx(file,path,name,skeletal=False,animations=False,source_yaw=None):
    force='TrainReimport' in ue.SystemLibrary.get_command_line() and not animations
    if AS.does_asset_exist(path+'/'+name) and not force: return ue.load_asset(path+'/'+name)
    if AS.does_asset_exist(path+'/'+name+'1'):
        AS.rename_asset(path+'/'+name+'1',path+'/'+name)
        return ue.load_asset(path+'/'+name)
    task=ue.AssetImportTask();task.filename=str(ROOT/file);task.destination_path=path;task.destination_name=name
    task.automated=True;task.save=True;task.replace_existing=True;task.replace_existing_settings=True
    opt=ue.FbxImportUI();opt.import_mesh=True;opt.import_as_skeletal=skeletal;opt.import_animations=animations
    opt.import_materials=True;opt.import_textures=False;opt.automated_import_should_detect_type=False
    if skeletal:
        opt.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',True)
    opt.mesh_type_to_import=ue.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else ue.FBXImportType.FBXIT_STATIC_MESH
    if source_yaw is not None:
        rotation=ue.Rotator(yaw=source_yaw)
        opt.skeletal_mesh_import_data.import_rotation=rotation
        opt.anim_sequence_import_data.import_rotation=rotation
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


def retarget(source,target,label):
    source_path=source.get_path_name().rsplit('/',1)[0];target_path=target.get_path_name().rsplit('/',1)[0]
    src=rig(source,source_path,'IK_'+source.get_name());dst=rig(target,target_path,'IK_'+target.get_name())
    name='RTG_'+label;path=target_path+'/'+name
    rt=ue.load_asset(path) if AS.does_asset_exist(path) else AT.create_asset(name,target_path,ue.IKRetargeter,ue.IKRetargetFactory())
    c=ue.IKRetargeterController.get_controller(rt);c.set_ik_rig(ue.RetargetSourceOrTarget.SOURCE,src);c.set_ik_rig(ue.RetargetSourceOrTarget.TARGET,dst)
    c.remove_all_ops();c.add_default_ops();c.auto_map_chains(ue.AutoMapChainType.EXACT,True);c.auto_align_all_bones(ue.RetargetSourceOrTarget.TARGET)
    AS.save_loaded_asset(rt)
    return rt


def bake_clip(source, target, retargeter, source_animation, target_path, name):
    inputs = ue.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [ue.EditorAssetLibrary.find_asset_data(source_animation)]
    inputs.source_mesh = source
    inputs.target_mesh = target
    inputs.ik_retarget_asset = retargeter
    inputs.target_path = target_path
    inputs.overwrite_existing_files = True
    inputs.include_referenced_assets = False
    inputs.search = source_animation.rsplit('.', 1)[-1]
    inputs.replace = name
    outputs = ue.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if len(outputs) != 1 or str(outputs[0].asset_name) != name:
        raise RuntimeError(f'Unexpected output baking {name}: {outputs}')
    clip = outputs[0].get_asset()
    clip.set_editor_property('force_root_lock', True)
    clip.set_editor_property('root_motion_root_lock', ue.RootMotionRootLock.REF_POSE)
    clip.set_editor_property('enable_root_motion', False)
    return clip


def main():
    import sys
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    from retarget_conductor import bake

    cmdline = ue.SystemLibrary.get_command_line()
    if 'TrainAutomatonOnly' in cmdline:
        import_automaton()
        return

    target=fbx('SourceArt/Characters/Conductor/SK_Conductor.fbx','/Game/Characters/Conductor','SK_Conductor',True)
    if 'TrainReimport' in cmdline or not AS.does_asset_exist('/Game/Characters/Conductor/M_Conductor'):
        surface(target,'SourceArt/Characters/Conductor/conductor_color.jpg','Conductor')
    bake(target,fbx,rig,bake_clip,only_dodge='TrainDodgeOnly' in cmdline)
    if 'TrainConductorOnly' in cmdline or 'TrainDodgeOnly' in cmdline:
        return
    import_automaton()
    for name in ['sabre','hammer']:
        mesh=fbx('SourceArt/Weapons/'+name+'.fbx','/Game/Weapons',name)
        surface(mesh,'SourceArt/Weapons/'+name+'_color.jpg',name)

    texture(ROOT/'SourceArt/Portraits/Conductor.png','/Game/UI/Portraits','Conductor')
    AS.save_directory('/Game/Characters',recursive=True)
    print('TRAIN_CHARACTERS_IMPORTED')


def import_automaton():
    source_path = '/Game/Characters/AnimationSource'
    target_path = '/Game/Characters/Automaton'
    source = fbx('SourceArt/Animations/Quaternius/AL_Standard.fbx', source_path, 'SK_Source', True, True)
    enemy = fbx('SourceArt/Characters/Automaton/SK_Automaton.fbx', target_path, 'SK_Automaton', True)
    if 'TrainReimport' in ue.SystemLibrary.get_command_line() or not AS.does_asset_exist(target_path + '/M_Automaton'):
        surface(enemy, 'SourceArt/Characters/Automaton/automaton_color.jpg', 'Automaton')
    rt = retarget(source, enemy, 'SK_Source_SK_Automaton')
    # The unarmed stance matches the automaton's punch and leaves its gauntlets at its sides.
    clips = {'Idle': 'Idle_Loop', 'Walk': 'Walk_Loop', 'Attack': 'Punch_Cross',
             'Hit': 'Hit_Chest', 'Death': 'Death01'}
    assets = AS.list_assets(source_path, recursive=False)
    for name, suffix in clips.items():
        src = next(path for path in assets if path.rsplit('.', 1)[-1] == 'SK_SourceRig_' + suffix)
        clip = bake_clip(source, enemy, rt, src, target_path + '/Animations', name)
        AS.save_loaded_asset(clip)
    print('TRAIN_AUTOMATON_BAKED')


if __name__=='__main__':
    main()
