import sys
from pathlib import Path
import unreal as ue
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_characters import AS,AT,LIB,fbx
for name in ('Slash','Impact'):
    mesh=fbx(f'SourceArt/Effects/{name}.fbx','/Game/Effects',name)
    mesh.set_editor_property('generate_mesh_distance_field',False)
    AS.save_loaded_asset(mesh)
mat=ue.load_asset('/Game/Effects/M_Combat')
if not mat:
    mat=AT.create_asset('M_Combat','/Game/Effects',ue.Material,ue.MaterialFactoryNew())
    mat.set_editor_property("blend_mode",ue.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided",True)
    tint=LIB.create_material_expression(mat,ue.MaterialExpressionVectorParameter)
    tint.set_editor_property("parameter_name",'Tint')
    tint.set_editor_property('default_value',ue.LinearColor(1.2,2.1,2.5,1))
    fade=LIB.create_material_expression(mat,ue.MaterialExpressionScalarParameter)
    fade.set_editor_property("parameter_name",'Fade')
    fade.set_editor_property('default_value',1.)
    LIB.connect_material_property(tint,'',ue.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.connect_material_property(fade,'',ue.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(mat)
    AS.save_loaded_asset(mat)
flash=ue.load_asset('/Game/Effects/M_HitFlash')
if not flash:
    flash=AT.create_asset('M_HitFlash','/Game/Effects',ue.Material,ue.MaterialFactoryNew())
    flash.set_editor_property("blend_mode",ue.BlendMode.BLEND_TRANSLUCENT)
    flash.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
    color=LIB.create_material_expression(flash,ue.MaterialExpressionConstant3Vector)
    color.constant=ue.LinearColor(2.0,1.4,.65)
    alpha=LIB.create_material_expression(flash,ue.MaterialExpressionConstant)
    alpha.r=.6
    LIB.connect_material_property(color,'',ue.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.connect_material_property(alpha,'',ue.MaterialProperty.MP_OPACITY)
    LIB.set_material_usage(flash,ue.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    LIB.recompile_material(flash)
    AS.save_loaded_asset(flash)
print('TRAIN_COMBAT_EFFECTS_IMPORTED')
warning=ue.load_asset('/Game/Effects/M_Telegraph') or AT.create_asset('M_Telegraph','/Game/Effects',ue.Material,ue.MaterialFactoryNew())
# CDO-referenced expressions may be rooted in commandlets; reconnect instead of deleting them.
if AS.get_metadata_tag(warning,'TrainTelegraphVersion') != '2':
    warning.set_editor_property('blend_mode',ue.BlendMode.BLEND_TRANSLUCENT)
    warning.set_editor_property('shading_model',ue.MaterialShadingModel.MSM_UNLIT)
    uv=LIB.create_material_expression(warning,ue.MaterialExpressionTextureCoordinate)
    time=LIB.create_material_expression(warning,ue.MaterialExpressionTime)
    edge=LIB.create_material_expression(warning,ue.MaterialExpressionCustom)
    edge.set_editor_property('output_type',ue.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs=[]
    for name in ('UV','Clock','Radial','StartedAt','Windup'):
        item=ue.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    edge.set_editor_property('inputs',inputs)
    edge.set_editor_property('code',"float2 p=abs(UV-.5)*2; float d=lerp(max(p.x,p.y),length(p),Radial); float t=saturate((Clock-StartedAt)/max(.05,Windup)); float fill=lerp(UV.x,d,Radial); return .035+.16*step(fill,t)+(.5+.22*t)*smoothstep(.91,.98,d);")
    LIB.connect_material_expressions(uv,'',edge,'UV');LIB.connect_material_expressions(time,'',edge,'Clock')
    for name,value in (('Radial',0.),('StartedAt',0.),('Windup',1.)):
        param=LIB.create_material_expression(warning,ue.MaterialExpressionScalarParameter)
        param.set_editor_property('parameter_name',name);param.set_editor_property('default_value',value)
        LIB.connect_material_expressions(param,'',edge,name)
    color=LIB.create_material_expression(warning,ue.MaterialExpressionConstant3Vector);color.constant=ue.LinearColor(2.0,.32,.065)
    LIB.connect_material_property(color,'',ue.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.connect_material_property(edge,'',ue.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(warning);AS.save_loaded_asset(warning)
    AS.set_metadata_tag(warning,'TrainTelegraphVersion','2');AS.save_loaded_asset(warning)
