"""Shared lighting and architectural materials for the train and flooded district."""
import math
from functools import lru_cache
from pathlib import Path
import unreal as ue
from import_characters import AS, AT, ROOT, texture

LIB = ue.MaterialEditingLibrary
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
LEVELS = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
PATH = '/Game/Materials/ArtDirection'


def surface(name, color, roughness=.7, metal=0., image=None, normal=None, scale=600., glow=None):
    existing = ue.load_asset(f'{PATH}/{name}')
    if existing and 'TrainRefreshMaterials' not in ue.SystemLibrary.get_command_line():
        return existing
    mat = existing or AT.create_asset(name, PATH, ue.Material, ue.MaterialFactoryNew())
    rgb = LIB.create_material_expression(mat, ue.MaterialExpressionConstant3Vector)
    rgb.constant = ue.LinearColor(*color)
    LIB.connect_material_property(rgb, '', ue.MaterialProperty.MP_BASE_COLOR)
    if image or normal:
        world = LIB.create_material_expression(mat, ue.MaterialExpressionWorldPosition)
        mask = LIB.create_material_expression(mat, ue.MaterialExpressionComponentMask)
        mask.set_editor_property("r", True)
        mask.set_editor_property("g", True)
        mask.set_editor_property("b", False)
        mask.set_editor_property("a", False)
        divide = LIB.create_material_expression(mat, ue.MaterialExpressionDivide)
        divide.set_editor_property("const_b", scale)
        LIB.connect_material_expressions(world, '', mask, '')
        LIB.connect_material_expressions(mask, '', divide, 'A')
        for filename, is_normal in ((image, False), (normal, True)):
            if not filename:
                continue
            tex = texture(ROOT / 'SourceArt/Environment' / filename, PATH, 'T_' + Path(filename).stem)
            if is_normal:
                tex.compression_settings = ue.TextureCompressionSettings.TC_NORMALMAP
                tex.srgb = False
            tex.set_editor_property('power_of_two_mode',ue.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
            tex.set_editor_property('max_texture_size',1024)
            AS.save_loaded_asset(tex)
            sample = LIB.create_material_expression(mat, ue.MaterialExpressionTextureSample)
            sample.texture = tex
            LIB.connect_material_expressions(divide, '', sample, 'UVs')
            if is_normal:
                sample.sampler_type = ue.MaterialSamplerType.SAMPLERTYPE_NORMAL
                LIB.connect_material_property(sample, 'RGB', ue.MaterialProperty.MP_NORMAL)
            else:
                mult = LIB.create_material_expression(mat, ue.MaterialExpressionMultiply)
                LIB.connect_material_expressions(sample, 'RGB', mult, 'A')
                LIB.connect_material_expressions(rgb, '', mult, 'B')
                LIB.connect_material_property(mult, '', ue.MaterialProperty.MP_BASE_COLOR)
    for prop, val in ((ue.MaterialProperty.MP_ROUGHNESS, roughness), (ue.MaterialProperty.MP_METALLIC, metal)):
        scalar = LIB.create_material_expression(mat, ue.MaterialExpressionConstant)
        scalar.r = val
        LIB.connect_material_property(scalar, '', prop)
    if glow:
        emission = LIB.create_material_expression(mat, ue.MaterialExpressionConstant3Vector)
        emission.constant = ue.LinearColor(*glow)
        LIB.connect_material_property(emission, '', ue.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.set_material_usage(mat, ue.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    LIB.recompile_material(mat)
    AS.save_loaded_asset(mat)
    return mat


@lru_cache(maxsize=1)
def palette():
    return {
        'stone': surface('QuayStone', (.64,.75,.79), .68, image='QuayFlagstone.png', normal='QuayFlagstoneNormal.png', scale=680),
        'steel': surface('WornSteel', (.5,.65,.7), .57, .4, image='WagonSteelPlatePainted.png', normal='WagonSteelPlateNormal.png', scale=650),
        'wall': surface('Limestone', (.23,.29,.29), .85),
        'edge': surface('CutLimestone', (.38,.43,.39), .82),
        'iron': surface('PaintedIron', (.032,.065,.073), .48, .45),
        'brass': surface('WornBrass', (.39,.23,.075), .46, .65),
        'red': surface('OxidePaint', (.24,.052,.028), .68, .16),
        'wood': surface('WarmTimber', (.45,.29,.16), .78, image='WagonBoardsPainted.png', normal='WagonBoardsNormal.png', scale=850),
        'glass': surface('AmberGlass', (.20,.14,.055), .32, glow=(1.3,.58,.16)),
        'blueglass': surface('NightGlass', (.022,.085,.115), .3, glow=(.035,.16,.21)),
        'rug': surface('WovenBurgundy', (.16,.025,.025), .95),
        'dark': surface('DeepShadow', (.012,.022,.027), .95),
    }


def atmosphere(interior=False):
    from lighting import environment
    environment(interior)


def dress_wagon():
    from rebuild_world import mesh, prop, light
    mats = palette()
    # Keep the authored interaction coordinates and tighten the surrounding carriage.
    for a in list(ACTORS.get_all_level_actors()):
        label = a.get_actor_label()
        if label.startswith('Atelier peint —') or label in ('Sol instancié', 'Fondation', 'Tapis du salon', 'Galon de tapis'):
            ACTORS.destroy_actor(a)
            continue
        loc = a.get_actor_location()
        if abs(loc.y) > 400 and abs(loc.y) < 700:
            loc.y *= .68
            a.set_actor_location(loc, False, False)
    mesh('Plancher du train', (0,0,-12), (6000,860,24), mats['wood'])
    mesh('Tapis de circulation', (-930,-80,1), (3900,190,2), mats['rug'], collision=False)
    for y in (-172,12):
        mesh('Bord brodé', (-930,y,2.5), (3900,3,1), mats['brass'], collision=False)
    for x in (-1000,1000):
        for y in (-335,335):
            mesh('Cloison de compartiment', (x,y,120 if y>0 else 32), (42,185,240 if y>0 else 64), mats['iron'])
            mesh('Couvre-joint', (x-24,y,120 if y>0 else 32), (5,188,244 if y>0 else 68), mats['brass'], collision=False)
        mesh('Seuil de passage', (x,0,3), (80,480,6), mats['iron'], collision=False)
    # Storage and work surfaces connect each station to the wall behind it.
    for x in (-2250,-1450,-100,2650):
        mesh('Socle du poste', (x,280,4), (380,225,8), mats['iron'], collision=False)
        mesh('Étagère basse', (x+175,325,42), (115,105,84), mats['wood'])
        for z in (90,155,220):
            mesh('Rayonnage', (x+160,355,z), (150,85,9), mats['wood'], collision=False)
        for side in (-1,1):
            mesh('Montant de rayonnage', (x+160+side*69,382,130), (8,8,230), mats['iron'], collision=False)
        prop('Cargo', (x+153,348,94), scale=.42)
        prop('Cargo', (x+180,360,160), yaw=12, scale=.35)
        light((x,190,210), (255,165,76), intensity=900, radius=420)
    # Steam pipes, tools, upholstery and luggage give the refuge a lived-in scale.
    for x in range(-2750,2800,650):
        mesh('Radiateur', (x,377,42), (115,35,84), mats['iron'], collision=False)
        for dx in range(-45,46,15):
            mesh('Ailette du radiateur', (x+dx,354,44), (7,23,76), mats['brass'], collision=False)
        for z in (275,294):
            mesh('Conduite haute', (x,415,z), (640,12,12), mats['brass'], collision=False)
    for x in (-2600,-610,550):
        mesh('Tapis du compartiment', (x,190,2), (280,190,3), mats['rug'], collision=False)
        prop('Cargo', (x-135,285,0), yaw=75, scale=.55)
    # The practice bay has an inset deck with room for the actual weapon sweeps.
    mesh('Plancher entraînement', (1620,0,1), (1150,620,2), mats['wood'], collision=False)
    for y in (-318,318):
        mesh('Bord de la salle d’armes', (1620,y,4), (1150,10,8), mats['iron'], collision=False)
    model=ue.load_asset('/Game/Props/WorkshopBench')
    if not model:
        task=ue.AssetImportTask()
        task.filename=str(ROOT/'SourceArt/Props/WorkshopBench.glb')
        task.destination_path='/Game/Props/WorkshopBench'
        task.automated=True
        task.save=True
        AT.import_asset_tasks([task])
        model=next(a for a in task.get_objects() if isinstance(a,ue.StaticMesh))
        AS.rename_loaded_asset(model,'/Game/Props/WorkshopBench')
    if model:
        for a in ACTORS.get_all_level_actors():
            if a.get_actor_label()=='Arsenal':
                a.static_mesh_component.set_static_mesh(model)
                a.static_mesh_component.set_editor_property('override_materials',[])
                bounds=model.get_bounds()
                scale=235./(bounds.box_extent.x*2)
                a.set_actor_scale3d(ue.Vector(scale,scale,scale))
                a.set_actor_rotation(ue.Rotator(yaw=180),False)
                a.set_actor_location(ue.Vector(-1450,290,(bounds.box_extent.z-bounds.origin.z)*scale),False,False)
    from lighting import wagon
    wagon()
