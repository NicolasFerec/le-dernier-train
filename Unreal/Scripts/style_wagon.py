"""Apply the painted workshop treatment to the existing wagon level."""

from pathlib import Path
import sys

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_characters import AS, AT, ROOT, texture

LIB = ue.MaterialEditingLibrary
LEVELS = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
ART = ROOT / 'SourceArt/Environment'
CONTENT = '/Game/Art/Wagon'
PREFIX = 'Atelier peint — '


def surface(name, *, color=None, albedo=None, normal=None, roughness=.7, metallic=0., glow=None):
    path = f'{CONTENT}/{name}'
    if AS.does_asset_exist(path):
        return ue.load_asset(path)
    material = AT.create_asset(name, CONTENT, ue.Material, ue.MaterialFactoryNew())

    def sample(file, property_, is_normal=False):
        asset = texture(ART / (file + '.png'), CONTENT, 'T_' + file)
        if is_normal:
            asset.set_editor_property('compression_settings', ue.TextureCompressionSettings.TC_NORMALMAP)
            asset.set_editor_property('srgb', False)
        else:
            asset.set_editor_property('power_of_two_mode', ue.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
        asset.set_editor_property('max_texture_size', 1024)
        asset.set_editor_property('mip_gen_settings', ue.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
        AS.save_loaded_asset(asset)
        node = LIB.create_material_expression(material, ue.MaterialExpressionTextureSample)
        node.texture = asset
        if is_normal:
            node.sampler_type = ue.MaterialSamplerType.SAMPLERTYPE_NORMAL
        LIB.connect_material_property(node, 'RGB', property_)

    def rgb(value, property_):
        node = LIB.create_material_expression(material, ue.MaterialExpressionConstant3Vector)
        node.constant = ue.LinearColor(*value, 1.)
        LIB.connect_material_property(node, '', property_)

    if albedo:
        sample(albedo, ue.MaterialProperty.MP_BASE_COLOR)
    else:
        rgb(color, ue.MaterialProperty.MP_BASE_COLOR)
    if normal:
        sample(normal, ue.MaterialProperty.MP_NORMAL, True)
    if glow:
        rgb(glow, ue.MaterialProperty.MP_EMISSIVE_COLOR)
    for value, property_ in ((roughness, ue.MaterialProperty.MP_ROUGHNESS),
                             (metallic, ue.MaterialProperty.MP_METALLIC)):
        node = LIB.create_material_expression(material, ue.MaterialExpressionConstant)
        node.r = value
        LIB.connect_material_property(node, '', property_)
    LIB.set_material_usage(material, ue.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    LIB.recompile_material(material)
    AS.save_loaded_asset(material)
    return material


def batch(label, material, boxes, cast_shadow=True, yaw=0.):
    mesh = ue.load_asset('/Engine/BasicShapes/Cube')
    transforms = [ue.Transform(location=ue.Vector(*pos), rotation=ue.Rotator(yaw=yaw),
                               scale=ue.Vector(*(v/100 for v in dims)))
                  for pos, dims in boxes]
    actor = ue.TrainWorldAuthoring.add_repeated_mesh(LEVELS.get_current_level(), mesh, material, transforms)
    actor.set_actor_label(PREFIX + label)
    component = actor.get_editor_property('root_component')
    component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    component.set_editor_property('cast_shadow', cast_shadow)
    component.set_editor_property('can_ever_affect_navigation', False)


def apply():
    boards = surface('M_WagonBoards', albedo='WagonBoardsPainted', normal='WagonBoardsNormal', roughness=.83)
    plate = surface('M_WagonPlate', albedo='WagonSteelPlatePainted', normal='WagonSteelPlateNormal',
                    roughness=.66, metallic=.21)
    glass = surface('M_WagonGlass', color=(.045,.14,.28), glow=(.075,.24,.50), roughness=.32, metallic=.12)
    trim = surface('M_WagonTrim', color=(.046,.074,.101), roughness=.72, metallic=.22)
    edge = surface('M_WagonEdge', color=(.125,.20,.27), roughness=.68, metallic=.25)
    leather = surface('M_WagonLeather', color=(.105,.053,.045), roughness=.87)
    brass = surface('M_WagonBrass', color=(.29,.16,.068), roughness=.56, metallic=.36)
    blueprint = surface('M_WagonBlueprint', albedo='WagonValveBlueprint', roughness=.88)
    shadow = ue.load_asset('/Game/Materials/Studio_Iron')

    windows = []
    armory = None
    for actor in ACTORS.get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith(PREFIX) or label in ('Tapis du salon', 'Galon de tapis'):
            ACTORS.destroy_actor(actor)
        elif label == 'Sol instancié':
            actor.get_editor_property('root_component').set_material(0, boards)
        elif label.startswith('Boiserie de fenêtre'):
            actor.static_mesh_component.set_material(0, trim)
            windows.append(actor.get_actor_location().x)
        elif label.startswith('Vitre bleu pétrole'):
            actor.static_mesh_component.set_material(0, glass)
        elif label.startswith(('Filet de cuivre', 'Montant de fenêtre', 'Poutre rivetée')):
            actor.static_mesh_component.set_material(0, edge)
        elif label.startswith(('Tapis d’entraînement', 'Quai du dépôt')):
            actor.static_mesh_component.set_material(0, plate)
        elif label.startswith(('Banquette capitonnée', 'Dossier')):
            actor.static_mesh_component.set_material(0, leather)
        elif label.startswith('Traverse de chemin de fer'):
            actor.static_mesh_component.set_material(0, trim)
        elif label == 'Arsenal' or label.startswith('Cargo'):
            component = actor.get_component_by_class(ue.StaticMeshComponent)
            palette = {'Walnut': boards, 'Iron': trim, 'Brass': brass, 'Oxblood': leather}
            for index, slot in enumerate(component.static_mesh.static_materials):
                material = palette.get(str(slot.material_slot_name).removeprefix('LDT_'))
                if material:
                    component.set_material(index, material)
            if label == 'Arsenal':
                armory = actor
        elif label == 'Lumière diffuse':
            actor.light_component.set_editor_property('intensity', 1.62)
            actor.light_component.set_editor_property('light_color', ue.Color(174, 205, 255, 255))
        elif label == 'Lumière de lecture':
            # The cubemap supplies the broad fill without a second fullscreen light.
            actor.light_component.set_editor_property('intensity', 0.)
        elif label == 'Ciel ambiant':
            actor.light_component.set_editor_property('intensity', 1.15)
            actor.light_component.set_editor_property('light_color', ue.Color(199, 212, 245, 255))
        elif isinstance(actor, ue.PointLight):
            loc = actor.get_actor_location()
            fixture = abs(loc.y - 425) < 1
            at_armory = abs(loc.x + 1450) < 5 or (abs(loc.x + 1510) < 5 and abs(loc.y - 246) < 5)
            if at_armory:
                actor.set_actor_location(ue.Vector(-1510, 246, 247), False, False)
            actor.point_light_component.set_editor_property(
                'light_color', ue.Color(*((118,178,255,255) if fixture else (255,169,94,255))))
            actor.point_light_component.set_editor_property('intensity', 2.8 if fixture else (7.4 if at_armory else 5.8))
            actor.point_light_component.set_editor_property('attenuation_radius', 465 if fixture else 510)

    batch('plaques rivetées', plate,
          [((x, -6, .55), (296, 356, 1.1)) for x in range(-2850, 2851, 300)], cast_shadow=False)
    batch('joints de l’allée', shadow,
          [((0, y, 1.1), (5980, 9, 2.2)) for y in (-189, 177)], cast_shadow=False)

    profiles, highlights = [], []
    for x in windows:
        for z in (133, 231):
            profiles.append(((x, 599, z), (270, 11, 9)))
            highlights.append(((x, 592, z+2), (270, 4, 2)))
        for xx in (x-135, x+135):
            profiles.append(((xx, 599, 182), (9, 11, 108)))
            highlights.append(((xx-2, 592, 182), (2, 4, 108)))
        profiles.append(((x, 602, 183), (260, 7, 5)))
    batch('cadres de fenêtres', trim, profiles)
    batch('arêtes de fenêtres', edge, highlights, cast_shadow=False)
    batch('lisses et flancs', trim, [((0, y, z), (5980, depth, height))
          for y, z, depth, height in ((-619, 25, 24, 28), (611, 65, 20, 13), (616, 253, 20, 19))])
    batch('panneaux bas', plate, [((x, 603, 47), (324, 6, 86)) for x in windows])
    if armory:
        position = armory.get_actor_transform().transform_location(ue.Vector(-16, -8, 112.6))
        batch('plan de l’arsenal', blueprint, [(position.to_tuple(), (83, 54, 1))],
              cast_shadow=False, yaw=armory.get_actor_rotation().yaw + 7)
        lamp = ACTORS.spawn_actor_from_class(ue.StaticMeshActor, ue.Vector(-1510, 246, 224))
        lamp.set_actor_label(PREFIX + 'lampe de l’arsenal')
        lamp.static_mesh_component.set_static_mesh(ue.load_asset('/Game/Props/Lantern'))
        lamp.set_actor_scale3d(ue.Vector(.68,.68,.68))
        lamp.set_actor_enable_collision(False)
        batch('potence de l’arsenal', trim,
              [((-1510, 291, 142), (8, 8, 284)), ((-1510, 268, 282), (8, 50, 8))])

    # The warm pools stay near the stations, while the fixed wall and windows read blue.
    print('TRAIN_WAGON_STYLED')


if __name__ == '__main__':
    if not LEVELS.load_level('/Game/Maps/Wagon'):
        raise RuntimeError('Could not load Wagon')
    apply()
    if not LEVELS.save_current_level():
        raise RuntimeError('Could not save Wagon')
