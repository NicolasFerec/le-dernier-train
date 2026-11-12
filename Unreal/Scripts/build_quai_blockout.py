"""Author only /Game/Maps/Blockout/QuaiBlockout, using engine primitives.
No generated meshes, textures or existing map changes. Dimensions in centimetres.
Run through Unreal's Python commandlet; rebuilds this review map deterministically.
"""
from pathlib import Path
import shutil
import unreal as ue

ROOT = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
LEVEL = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)
AS = ue.EditorAssetLibrary
AT = ue.AssetToolsHelpers.get_asset_tools()
ML = ue.MaterialEditingLibrary
MAP = '/Game/Maps/Blockout/QuaiBlockout'
CUBE = ue.load_asset('/Engine/BasicShapes/Cube.Cube')
CYLINDER = ue.load_asset('/Engine/BasicShapes/Cylinder.Cylinder')


def material(name, rgb, emissive=False):
    path = '/Game/Materials/Blockout/' + name
    m = ue.load_asset(path)
    if m:
        return m
    m = AT.create_asset(name, '/Game/Materials/Blockout', ue.Material, ue.MaterialFactoryNew())
    color = ML.create_material_expression(m, ue.MaterialExpressionConstant3Vector)
    color.constant = ue.LinearColor(*rgb)
    ML.connect_material_property(color, '', ue.MaterialProperty.MP_BASE_COLOR)
    rough = ML.create_material_expression(m, ue.MaterialExpressionConstant)
    rough.r = .85
    ML.connect_material_property(rough, '', ue.MaterialProperty.MP_ROUGHNESS)
    if emissive:
        ML.connect_material_property(color, '', ue.MaterialProperty.MP_EMISSIVE_COLOR)
    ML.recompile_material(m)
    AS.save_loaded_asset(m)
    return m


def actor(cls, name, pos, yaw=0):
    a = ACTORS.spawn_actor_from_class(cls, ue.Vector(*pos), ue.Rotator(yaw=yaw))
    a.set_actor_label(name)
    return a


def box(name, pos, size, mat, collision=True, shape=None):
    a = actor(ue.StaticMeshActor, name, pos)
    a.static_mesh_component.set_static_mesh(shape or CUBE)
    a.static_mesh_component.set_material(0, mat)
    a.set_actor_scale3d(ue.Vector(*(n / 100 for n in size)))
    a.set_actor_enable_collision(collision)
    return a


def slab(name, x0, x1, y0, y1, mat):
    return box(name, ((x0+x1)/2, (y0+y1)/2, -40), (x1-x0, y1-y0, 80), mat)


def rail(name, p, s):
    box(name, (*p, 38), (*s, 76), edge)
    # Invisible continuation prevents dashing into the canal; visible low parapet
    # is deliberately below the hero's waist for the fixed gameplay camera.
    a = box(name + ' — collision', (*p, 170), (*s, 340), edge)
    a.set_actor_hidden_in_game(True)


def lamp(name, x, y, z):
    source = actor(ue.PointLight, name, (x,y,z))
    c = source.point_light_component
    c.set_mobility(ue.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units', ue.LightUnits.LUMENS)
    c.set_editor_property('use_inverse_squared_falloff', True)
    c.set_editor_property('intensity', 1700.)
    c.set_editor_property('attenuation_radius', 720.)
    c.set_editor_property('light_color', ue.Color(r=255,g=193,b=121,a=255))
    c.set_editor_property('cast_shadows', True)
    c.set_editor_property('shadow_bias', .25)
    bulb = box(name + ' — source', (x,y,z), (28,28,45), amber, False)
    bulb.static_mesh_component.set_editor_property('cast_shadow', False)


existing = ROOT / 'Content/Maps/Blockout/QuaiBlockout.umap'
if existing.exists():
    backup = ROOT / 'Saved/Checkpoints/Blockout/QuaiBlockout-before-rebuild.umap'
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(existing, backup)
if AS.does_asset_exist(MAP):
    if not LEVEL.load_level(MAP):
        raise RuntimeError('Could not load existing blockout map')
    for old in ACTORS.get_all_level_actors():
        if not isinstance(old, (ue.WorldSettings, ue.LevelScriptActor, ue.NavigationData, ue.NavMeshBoundsVolume)):
            ACTORS.destroy_actor(old)
elif not LEVEL.new_level(MAP):
    raise RuntimeError('Could not create blockout map')
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', ue.load_class(None, '/Script/LeDernierTrain.TrainBlockoutGameMode'))

paving = material('BO_Paving', (.32,.35,.34))
arrival = material('BO_Arrival', (.46,.40,.28))
service = material('BO_Service', (.25,.37,.38))
brick = material('BO_Building', (.29,.19,.14))
edge = material('BO_Edge', (.17,.22,.25))
metal = material('BO_Metal', (.055,.09,.12))
water = material('BO_Water', (.025,.12,.15))
cargo = material('BO_Cargo', (.45,.30,.16))
amber = material('BO_Amber', (1.,.48,.10), True)
exit_mat = material('BO_Exit', (.12,.55,.39), True)

# Footprint is a union of usable spaces, not a floor under invisible water.
slab('A — Quai du train', -2200,-1250,-1450,1400,arrival)
slab('B — Cour des bagages', -1250,1700,-1150,1250,paving)
slab('C — Passage de service 6 m', 1700,2350,150,750,service)
slab('D — Palier du poste', 2350,3250,-150,1700,service)
slab('D — Rue des vannes, amorce', 2550,3250,1700,2200,service)
box('Eau — canal et drain', (100,-300,-160), (25000,25000,35), water,False)
# Context volumes make the level belong to a district: rails sit on an embankment
# and the exit visibly continues into a street, rather than ending in open water.
box('Remblai ferroviaire',(-2540,-100,-150),(740,6000,180),edge,False)
box('Tunnel — pile ouest',(-2920,2260,300),(180,550,600),brick,False)
box('Tunnel — pile est',(-2160,2260,300),(180,550,600),brick,False)
box('Tunnel — linteau',(-2540,2260,640),(940,550,140),brick,False)
box('Rue — prolongement visuel',(2900,2900,-40),(700,1400,80),service,False)
box('Quartier — immeuble ouest',(2110,2900,330),(880,1400,660),brick,False)
box('Quartier — immeuble est',(3630,2700,400),(760,1800,800),brick,False)
# Structural base: visible edges, never extra traversable floor.
for name,p,s in [('Fondation quai',(-1725,-25,-160),(950,2850,240)),('Fondation cour',(225,50,-160),(2950,2400,240)),('Fondation poste',(2800,775,-160),(900,1850,240))]:
    box(name,p,s,edge)

# The train is a landmark and a protective western boundary, in simple volumes.
box('Train — châssis',(-2540,-10,110),(610,2640,220),metal)
box('Train — wagon refuge',(-2540,650,300),(580,1270,360),edge)
box('Train — locomotive',(-2540,-700,260),(560,1120,300),metal)
box('Train — chaudière',(-2540,-850,440),(280,280,130),metal,True,CYLINDER)
for y in [-1100,-600,100,600,1100]:
    box('Train — fenêtre ambre',(-2240,y,330),(12,140,120),amber,False)
for x in [-2730,-2350]:
    box('Rail continu',(x,0,-30),(16,5200,25),metal,False)
for y in range(-2400,2401,180):
    box('Traverse',(-2540,y,-50),(620,35,30),edge,False)
# Platform shelter has posts and a narrow canopy above the train edge, not player.
for y in [-1200,-300,600,1300]:
    box('Marquise — poteau',(-2140,y,170),(25,25,340),metal)
box('Marquise — toiture partielle',(-2210,50,365),(160,2730,32),metal,False)
box('Billetterie abandonnée',(-1730,-1280,150),(420,300,300),brick)
box('Billetterie — comptoir',(-1730,-1110,115),(250,35,35),cargo)

# Baggage hall defines the northern edge. Solid behind the camera-visible facade.
box('Halle — masse', (150,1580,280),(3100,660,560),brick)
box('Halle — soubassement',(150,1230,45),(3100,65,90),edge)
for x in [-1250,-650,-50,550,1150]:
    box('Halle — contrefort',(x,1200,250),(65,100,500),edge)
    box('Halle — baie',(x+180,1245,310),(190,15,210),metal,False)
box('Halle — porte de fret',(-180,1235,170),(340,25,340),cargo,False)
# Central luggage island makes two honest combat routes, both >= 7m clear.
box('Îlot — socle',(-40,0,22),(1180,420,44),edge)
box('Îlot — bagages bas',(-40,0,83),(1080,320,122),cargo)
for x in [-500,-40,420]:
    box('Îlot — cerclage',(x,0,148),(16,325,10),metal,False)
# Edge storage, never random clutter blocking the combat lanes.
for x,y in [(-1030,1030),(1150,1030)]:
    box('Fret contre halle',(x,y,65),(250,190,130),cargo)

# Low foreground edges and guarded drainage; bridge is a genuine 6m opening.
rail('Parapet canal — cour',(225,-1150),(2950,35))
rail('Parapet canal — quai',(-1725,-1450),(950,35))
rail('Retour quai',(-1250,-1300),(35,300))
rail('Quai — extrémité nord',(-1725,1400),(950,35))
rail('Quai — rive du train',(-2200,-25),(30,2850))
rail('Drain — rive cour sud',(1700,-500),(35,1300))
rail('Drain — rive cour nord',(1700,1000),(35,500))
rail('Passage — bord sud',(2025,150),(650,32))
rail('Passage — bord nord',(2025,750),(650,32))
rail('Poste — rive sud',(2800,-150),(900,35))
rail('Poste — rive ouest sud',(2350,0),(35,300))
rail('Poste — rive ouest nord',(2350,1225),(35,950))
rail('Poste — retour nord',(2450,1700),(200,35))
rail('Poste — rive est',(3250,1025),(35,2350))
rail('Rue — rive ouest',(2550,1950),(35,500))
gate=box('Rue — limite de transition',(2900,2200,180),(700,35,360),edge)
gate.set_actor_hidden_in_game(True)
for x in [2570,3230]:
    box('Rue — portail de transition',(x,2200,180),(45,45,360),metal)
box('Rue — traverse de portail',(2900,2200,360),(700,45,45),metal)
# Cabin is on a non-walkable plinth beside crossing; view stays open.
box('Cabine de signal — socle',(2910,80,140),(550,450,280),brick)
box('Cabine de signal — vitrage',(2910,80,365),(540,440,170),service)
box('Cabine de signal — toit',(2910,80,465),(610,500,30),metal)
box('Signal — mât',(2470,970,310),(35,35,620),metal)
box('Signal — bras',(2540,970,590),(180,30,40),amber)
box('Sortie — zone de regroupement',(2820,1570,2),(450,260,4),exit_mat,False)

# Real light sources and shadows, restrained palette, manual fixed exposure.
sun=actor(ue.DirectionalLight,'Ciel — lumière principale',(0,0,1800))
sun.set_actor_rotation(ue.Rotator(pitch=-52,yaw=-38),False)
sun.light_component.set_mobility(ue.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('intensity',2.1)
sun.light_component.set_editor_property('light_color',ue.Color(r=191,g=211,b=235,a=255))
sun.light_component.set_editor_property('cast_shadows',True)
sun.light_component.set_editor_property('forward_shading_priority',1)
sun.light_component.set_editor_property('contact_shadow_length',8.)
sun.light_component.set_editor_property('contact_shadow_length_in_ws',True)
sky=actor(ue.SkyLight,'Ciel — ambiance',(0,0,1200))
sky.light_component.set_mobility(ue.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('intensity',.45)
cube=ue.load_asset('/Game/Art/WorkshopAmbient')
if cube:
    sky.light_component.set_editor_property('source_type',ue.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property('cubemap',cube)
pp=actor(ue.PostProcessVolume,'Exposition blockout',(0,0,0));pp.unbound=True
s=pp.settings
s.override_auto_exposure_method=True;s.auto_exposure_method=ue.AutoExposureMethod.AEM_MANUAL
s.override_auto_exposure_apply_physical_camera_exposure=True;s.auto_exposure_apply_physical_camera_exposure=False
s.override_auto_exposure_bias=True;s.auto_exposure_bias=0
s.override_motion_blur_amount=True;s.motion_blur_amount=0
s.override_bloom_intensity=True;s.bloom_intensity=.10
s.override_ambient_occlusion_intensity=True;s.ambient_occlusion_intensity=.7
s.override_ambient_occlusion_radius=True;s.ambient_occlusion_radius=40.
pp.settings=s
lamp('Lanterne arrivée',-2070,-500,260)
lamp('Lanterne halle',-650,1120,320)
lamp('Lanterne poste',2600,380,270)

# Four real native start positions; no overlapping capsules in coop.
for i,(x,y) in enumerate([(-1850,-850),(-1570,-850),(-1850,-570),(-1570,-570)]):
    actor(ue.PlayerStart,f'Arrivée équipier {i+1}',(x,y,100),35)
# Native GameMode converts these markers into replicated enemies on the host.
for i,(x,y) in enumerate([(-650,650),(1050,-650),(2830,1130)]):
    a=box(f'Rencontre {i+1} — patrouilleur',(x,y,105),(40,40,100),metal,False)
    a.tags=[ue.Name('Automate')]
    a.set_actor_hidden_in_game(True)
exit_actor=actor(ue.load_class(None,'/Script/LeDernierTrain.TrainStation'),'Vers la rue des vannes',(3030,1570,60))
exit_actor.set_editor_property('kind',3)
exit_actor.static_mesh_component.set_static_mesh(CUBE)
exit_actor.static_mesh_component.set_material(0,exit_mat)
exit_actor.set_actor_scale3d(ue.Vector(.8,.8,1.2))
ue.TrainWorldAuthoring.build_navigation(LEVEL.get_current_level(),ue.Vector(3800,2700,300))
if not LEVEL.save_current_level():
    raise RuntimeError('Could not save blockout')
print('TRAIN_QUAI_BLOCKOUT_SAVED',MAP,'actors',len(ACTORS.get_all_level_actors()))
