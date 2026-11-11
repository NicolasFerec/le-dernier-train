"""Apply atlas materials through the native skeletal-mesh API, then save."""
import unreal as ue
assets=ue.get_editor_subsystem(ue.EditorAssetSubsystem)
for name in ['Conductor','Automaton']:
    mesh=ue.load_asset('/Game/Characters/'+name+'/SK_'+name)
    material=ue.load_asset('/Game/Characters/'+name+'/M_'+name)
    lib=ue.MaterialEditingLibrary
    if name=='Conductor' and 'OutfitTint' not in [str(n) for n in lib.get_vector_parameter_names(material)]:
        tex=lib.create_material_expression(material,ue.MaterialExpressionTextureSample);tex.texture=ue.load_asset('/Game/Characters/Conductor/T_Conductor')
        tint=lib.create_material_expression(material,ue.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','OutfitTint');tint.set_editor_property('default_value',ue.LinearColor(1,1,1,1))
        mul=lib.create_material_expression(material,ue.MaterialExpressionMultiply)
        lib.connect_material_expressions(tex,'RGB',mul,'A');lib.connect_material_expressions(tint,'',mul,'B')
        lib.connect_material_property(mul,'',ue.MaterialProperty.MP_BASE_COLOR)
    ue.MaterialEditingLibrary.set_material_usage(material,ue.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    ue.MaterialEditingLibrary.recompile_material(material)
    assets.save_loaded_asset(material)
    ue.TrainWorldAuthoring.assign_surface(mesh,material)
    assets.save_loaded_asset(mesh)
    print('SURFACE',name,[str(s.material_interface) for s in mesh.get_editor_property('materials')])
for path in ['/Game/Materials/Plancher','/Game/Materials/Tole','/Game/Materials/LDT_Stone']:
    m=ue.load_asset(path)
    ue.MaterialEditingLibrary.set_material_usage(m,ue.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    ue.MaterialEditingLibrary.recompile_material(m);assets.save_loaded_asset(m)
print('TRAIN_SURFACES_APPLIED')
