"""Rebuild editable maps from the project's own art and modelled furniture.
Run in UnrealEditor-Cmd with -run=pythonscript. Existing maps are backed up in Saved/Checkpoints.
"""
from pathlib import Path
import shutil,math,sys
import unreal as ue
ROOT=Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
sys.path.insert(0,str(ROOT/'Scripts'))
from import_characters import AS, AT, fbx, texture
from style_wagon import apply as style_wagon
LEVELS=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
ACTORS=ue.get_editor_subsystem(ue.EditorActorSubsystem)
LIB=ue.MaterialEditingLibrary
for file in (ROOT/'Content/Maps').glob('*.umap'):
    backup=ROOT/'Saved/Checkpoints/Maps'/file.name;backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists():shutil.copy2(file,backup)

props={p.stem:fbx(str(p.relative_to(ROOT)),'/Game/Props',p.stem) for p in (ROOT/'SourceArt/Props').glob('*.fbx')}
palette={
 'Brass':((.45,.22,.075),.4,.52),'Iron':((.07,.10,.12),.3,.65),
 'Walnut':((.2,.085,.036),0,.65),'Oxblood':((.28,.045,.055),0,.6),
 'Ivory':((.75,.65,.43),.05,.55),'EnamelRed':((.46,.075,.035),.15,.38),
 'Verdigris':((.055,.31,.27),.4,.45),'AmberGlass':((1,.43,.085),.1,.3)}

def material(name,color,metal=0,rough=.6,emission=0,image=None,tint=None,unlit=False):
    path='/Game/Materials/'+name;m=ue.load_asset(path)
    if m:return m
    m=AT.create_asset(name,'/Game/Materials',ue.Material,ue.MaterialFactoryNew())
    if image:
        node=LIB.create_material_expression(m,ue.MaterialExpressionTextureSample);node.texture=image;pin='RGB'
        if tint:
            shade=LIB.create_material_expression(m,ue.MaterialExpressionConstant3Vector);shade.constant=ue.LinearColor(*tint)
            product=LIB.create_material_expression(m,ue.MaterialExpressionMultiply)
            LIB.connect_material_expressions(node,pin,product,'A');LIB.connect_material_expressions(shade,'',product,'B')
            node,pin=product,''
    else:
        node=LIB.create_material_expression(m,ue.MaterialExpressionConstant3Vector);node.constant=ue.LinearColor(*color);pin=''
    if unlit:m.set_editor_property('shading_model',ue.MaterialShadingModel.MSM_UNLIT)
    LIB.connect_material_property(node,pin,ue.MaterialProperty.MP_EMISSIVE_COLOR if unlit else ue.MaterialProperty.MP_BASE_COLOR)
    for prop,val in [(ue.MaterialProperty.MP_METALLIC,metal),(ue.MaterialProperty.MP_ROUGHNESS,rough)]:
        n=LIB.create_material_expression(m,ue.MaterialExpressionConstant);n.r=val;LIB.connect_material_property(n,'',prop)
    if emission:
        n=LIB.create_material_expression(m,ue.MaterialExpressionConstant3Vector);n.constant=ue.LinearColor(*(c*emission for c in color));LIB.connect_material_property(n,'',ue.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(m);AS.save_loaded_asset(m);return m

materials={n:material('Studio_'+n,*p,emission=.9 if n=='AmberGlass' else 0) for n,p in palette.items()}
for mesh in props.values():
    for i,slot in enumerate(mesh.static_materials):
        name=str(slot.material_slot_name).removeprefix('LDT_')
        if name in materials:mesh.set_material(i,materials[name])
    AS.save_loaded_asset(mesh)

art=ROOT/'SourceArt/Environment';art.mkdir(exist_ok=True)
texture(art/'depot-panoramique.png','/Game/Art','depot_panoramique')
wood=material('Plancher',(.25,.14,.06),image=texture(art/'floor.png','/Game/Art','Floor'))
tread=texture(art/'steel.png','/Game/Art','Steel')
steel=material('Tole',(.1,.15,.17),.4,image=tread)
dark=materials['Iron'];copper=materials['Brass'];wine=materials['Oxblood'];teal=materials['Verdigris'];amber=materials['AmberGlass']
foundry=material('LDT_TurbineDeck',(0,0,0),.12,.75,image=tread,tint=(1.7,1.0,.62))
inset=material('LDT_AccessPlate',(.072,.094,.104),.3,.7)
hazard=material('LDT_Hazard',(.40,.21,.06),.08,.7)
exterior=material('LDT_DepotBed',(.033,.052,.072),unlit=True)
concrete=material('LDT_DepotPlatform',(.083,.113,.145),.05,.85)
shapes={s:ue.load_asset('/Engine/BasicShapes/'+s+'.'+s) for s in ['Cube','Cylinder','Sphere']}

def actor(cls,label,pos,rot=0):
    a=ACTORS.spawn_actor_from_class(cls,ue.Vector(*pos),ue.Rotator(yaw=rot));a.set_actor_label(label);return a

def mesh(label,pos,size,mat,shape='Cube',yaw=0,collision=True):
    a=actor(ue.StaticMeshActor,label,pos,yaw);c=a.static_mesh_component;c.set_static_mesh(shapes[shape]);c.set_material(0,mat)
    a.set_actor_scale3d(ue.Vector(*(v/100 for v in size)));a.set_actor_enable_collision(collision);return a

def prop(name,pos,yaw=0,scale=1,collision=False):
    a=actor(ue.StaticMeshActor,name,pos,yaw);a.static_mesh_component.set_static_mesh(props[name]);a.set_actor_scale3d(ue.Vector(scale,scale,scale));a.set_actor_enable_collision(collision);return a

def light(pos,color=(255,193,122),intensity=250,radius=500):
    a=actor(ue.PointLight,'Éclairage de proximité',pos);c=a.point_light_component;c.set_mobility(ue.ComponentMobility.MOVABLE)
    c.set_editor_property('use_inverse_squared_falloff',False);c.set_editor_property('light_falloff_exponent',3.0)
    c.set_editor_property('intensity',intensity*.008);c.set_editor_property('attenuation_radius',radius);c.set_editor_property('light_color',ue.Color(*color,255));c.set_editor_property('cast_shadows',False)

def label(key,text,pos,yaw=0,size=32,color=(220,183,125)):
    a=actor(ue.TextRenderActor,text,pos,yaw);c=a.text_render;c.set_editor_property('text',ue.Text(text));c.set_editor_property('world_size',size);c.set_editor_property('text_render_color',ue.Color(*color,255))
    a.tags=[ue.Name(f'TrainSign.{key}')]
    c.set_editor_property('horizontal_alignment',ue.HorizTextAligment.EHTA_CENTER)
    return a

def station(kind,pos,model,yaw=0):
    a=actor(ue.load_class(None,'/Script/LeDernierTrain.TrainStation'),['Vestiaire','Arsenal','Passifs','Départ','Équipage'][kind],pos,yaw)
    a.set_editor_property('kind',kind);a.static_mesh_component.set_static_mesh(props[model]);return a

def new_map(name):
    path='/Game/Maps/'+name
    if AS.does_asset_exist(path):
        LEVELS.load_level(path)
        for a in ACTORS.get_all_level_actors():
            if not isinstance(a,(ue.WorldSettings,ue.LevelScriptActor,ue.NavigationData,ue.NavMeshBoundsVolume)):ACTORS.destroy_actor(a)
    else:LEVELS.new_level(path)
    sky=actor(ue.DirectionalLight,'Lumière diffuse',(0,0,1000));sky.set_actor_rotation(ue.Rotator(pitch=-58,yaw=-25),False)
    sky.light_component.set_mobility(ue.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('forward_shading_priority',1)
    sky.light_component.set_editor_property('intensity',1.8);sky.light_component.set_editor_property('light_color',ue.Color(255,231,204,255))
    sky.light_component.set_editor_property('cast_shadows',True)
    fill=actor(ue.DirectionalLight,'Lumière de lecture',(0,0,1000));fill.set_actor_rotation(ue.Rotator(pitch=-48,yaw=45),False)
    fill.light_component.set_mobility(ue.ComponentMobility.MOVABLE)
    fill.light_component.set_editor_property('intensity',1.2);fill.light_component.set_editor_property('light_color',ue.Color(218,229,248,255));fill.light_component.set_editor_property('cast_shadows',False)
    ambient=actor(ue.SkyLight,'Ciel ambiant',(0,0,1200));ambient.light_component.set_mobility(ue.ComponentMobility.MOVABLE);ambient.light_component.set_editor_property('intensity',.8);ambient.light_component.set_editor_property('light_color',ue.Color(220,222,226,255))
    ambient.light_component.set_editor_property('source_type',ue.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    ambient.light_component.set_editor_property('cubemap',texture(art/'WorkshopAmbient.hdr','/Game/Art','WorkshopAmbient'))
    pp=actor(ue.PostProcessVolume,'Palette et exposition',(0,0,0));pp.unbound=True
    settings=pp.settings
    settings.override_auto_exposure_method=True;settings.auto_exposure_method=ue.AutoExposureMethod.AEM_MANUAL
    settings.override_auto_exposure_bias=True;settings.auto_exposure_bias=0
    settings.override_auto_exposure_apply_physical_camera_exposure=True;settings.auto_exposure_apply_physical_camera_exposure=False
    settings.override_bloom_intensity=True;settings.bloom_intensity=.18
    settings.override_vignette_intensity=True;settings.vignette_intensity=.2
    settings.override_motion_blur_amount=True;settings.motion_blur_amount=0
    settings.override_color_saturation=True;settings.color_saturation=ue.Vector4(1.06,1.03,.98,1)
    pp.settings=settings

def floor(x,y,mat):
    mesh('Fondation',(0,0,-30),(x*2,y*2,50),dark)
    transforms=[]
    for px in range(-x+100,x,200):
        for py in range(-y+100,y,200):
            transforms.append(ue.Transform(location=ue.Vector(px,py,-3),scale=ue.Vector(2,2,.06)))
    LIB.set_material_usage(mat,ue.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    AS.save_loaded_asset(mat)
    ue.TrainWorldAuthoring.add_repeated_mesh(LEVELS.get_current_level(),shapes['Cube'],mat,transforms)

def bounds(x,y,mat):
    # Far walls show the set. Near parapets keep the whole playfield readable.
    mesh('Mur arrière',(0,y,130),(x*2,30,280),mat)
    mesh('Mur droit',(x,0,130),(30,y*2,280),mat)
    for pos,size in [((0,-y,12),(x*2,26,40)),((-x,0,12),(26,y*2,40))]:mesh('Garde-corps bas',pos,size,mat)
    # Invisible walls use native collision, not a hand-written clamp in movement.
    for pos,size in [((0,-y,150),(x*2,16,300)),((-x,0,150),(16,y*2,300))]:
        a=mesh('Limite praticable',pos,size,mat);a.set_actor_hidden_in_game(True)

new_map('Wagon');floor(3000,650,wood);bounds(3000,650,dark)
mesh('Sol du dépôt sous le train',(0,0,-195),(14000,8000,50),exterior,collision=False)
for y in [-490,490]:mesh('Rail du dépôt',(0,y,-112),(13000,12,30),copper)
for x in range(-6200,6400,130):mesh('Traverse de chemin de fer',(x,0,-136),(42,1200,25),wood)
for side in [-1,1]:
    mesh('Quai du dépôt',(0,side*1470,-175),(7600,1040,110),concrete,collision=False)
    mesh('Rebord du quai',(0,side*966,-113),(7600,32,28),dark,collision=False)
    mesh('Filet de sécurité du quai',(0,side*1004,-118),(7600,15,6),hazard,collision=False)
    for x in range(-3600,3650,300):mesh('Joint de dalle du quai',(x,side*1470,-119),(3,1000,4),dark,collision=False)
for x in range(-2700,2800,900):
    for y in [-560,560]:
        wheel=mesh('Roue sous le wagon',(x,y,-72),(160,160,32),dark,'Cylinder');wheel.set_actor_rotation(ue.Rotator(roll=90),False)
actor(ue.PlayerStart,'Arrivée',(-2580,-170,95),0)
for x in range(-2800,2900,350):
    mesh('Boiserie de fenêtre',(x,627,100),(310,22,180),wine)
    mesh('Vitre bleu pétrole',(x,611,182),(260,9,98),teal,collision=False)
    for z in [125,237]:mesh('Filet de cuivre',(x,602,z),(290,10,7),copper,collision=False)
    for side in [-1,1]:mesh('Montant de fenêtre',(x+side*104,601,182),(5,5,102),copper,collision=False)
    prop('Lantern',(x,555,220),scale=.65)
    if (x+2800)%700==0:light((x,425,220),intensity=205,radius=535)
for x in [-2900,-2000,-1000,0,1000,2000,2900]:
    # Repeated arches along the back edge leave the foreground open.
    prop('Arch',(x,590,0),yaw=90,scale=1)
    for y in [-610,610]:mesh('Poutre rivetée',(x,y,110),(30,35,230),dark)
for x in [-1000,1000]:
    for d in [-30,30]:mesh('Seuil entre wagons',(x+d,0,2),(16,1230,10),copper,collision=False)
    for y in [-510,510]:mesh('Joint de passage',(x,y,24),(65,160,50),dark)
mesh('Tapis du salon',(-700,-80,1),(4100,250,3),wine,collision=False)
for y in [-207,47]:mesh('Galon de tapis',(-700,y,3),(4100,5,3),copper,collision=False)
for x in [-2600,-600,550]:
    mesh('Banquette capitonnée',(x,475,33),(210,100,65),wine)
    mesh('Dossier',(x,520,92),(210,25,115),wine)
    for edge in [-100,100]:mesh('Accoudoir',(x+edge,475,65),(15,105,80),wood)
station(0,(-2250,300,0),'Wardrobe',135);station(1,(-1450,300,0),'Arsenal',135)
station(2,(-100,320,0),'Orrery',135);station(4,(-500,-150,0),'Telegraph',135)
station(3,(2650,0,0),'Departure',135)
for x,y in [(-1450,280),(-500,-150),(-100,320),(2650,0)]:light((x,y-100,225),intensity=165,radius=375)
for x,y in [(-2860,425),(-1135,515),(750,515),(2250,495),(2740,490)]:prop('Cargo',(x,y,0),yaw=165,scale=.82)
for key,text,x in [('Wardrobe','VESTIAIRE',-2250),('Arsenal','ARSENAL',-1450),
                   ('Passives','ATELIER DES PASSIFS',-100),('Practice','ENTRAÎNEMENT',1550)]:
    label(key,text,(x,580,270),-90,27)
for x,y in [(1350,-230),(1600,150),(1850,-230)]:
    actor(ue.load_class(None,'/Script/LeDernierTrain.TrainDummy'),'Mannequin',(x,y,0),135)
    mesh('Tapis d’entraînement',(x,y,1),(180,180,3),dark,collision=False)
for x in [-2700,-1800,-900,0,900,1800,2700]:
    for y in [-570,570]:
        mesh('Conduite de chauffage',(x,y,32),(690,9,9),copper,collision=False)
style_wagon()
ue.TrainWorldAuthoring.build_navigation(LEVELS.get_current_level(),ue.Vector(3000,650,300))
LEVELS.save_current_level()

for name in ['Pompage','Fonderie']:
    pumping=name=='Pompage'
    new_map(name);floor(2400,1800,steel if pumping else foundry);bounds(2400,1800,dark)
    mesh('Assise industrielle',(0,0,-185),(8000,6900,50),exterior,collision=False)
    actor(ue.PlayerStart,'Entrée de la salle',(-2100,-1200,95),35)
    station(3,(2100,1350,0),'ExitGate',-90)
    for x,y in [(-1000,900),(200,1150),(1200,-650)]:
        prop('Boiler' if pumping else 'Turbine',(x,y,0),yaw=135 if pumping else 0,scale=1.08)
        # Collision deliberately follows the tank footprint rather than its pipe silhouette.
        obstacle=mesh('Machine — collision',(x,y,125),(190,190,250) if pumping else (415,220,250),dark,'Cylinder' if pumping else 'Cube');obstacle.set_actor_hidden_in_game(True)
        mesh('Dalle de service',(x,y,2),(370 if pumping else 525,310,5),inset,collision=False)
        for end in [-1,1]:mesh('Marquage de service',(x,y+end*150,5),(335 if pumping else 490,5,3),hazard,collision=False)
        light((x-20,y-160,180),(148,222,245) if pumping else (255,160,85),intensity=255,radius=560)
    for x in range(-2100,2200,600):
        prop('Arch',(x,1690,0),yaw=90,scale=1.2);prop('Lantern',(x,1650,210),scale=.8)
        light((x,1550,250),(255,189,110) if name=='Pompage' else (255,153,92),intensity=240,radius=650)
    for y in range(-1500,1700,500):
        prop('Lantern',(2280,y,170),scale=.7);light((2150,y,230),intensity=200,radius=600)
    # A painted route and flush grates mark a readable traversal through the much larger room.
    for x,y in [(-1800,-1100),(-1000,-800),(-100,-350),(800,350),(1600,900)]:
        mesh('Trappe de maintenance',(x,y,1),(590,168,4),inset,collision=False)
        for yy in [-82,82]:mesh('Rive de trappe',(x,y+yy,4),(590,7,3),copper,collision=False)
        for xx in range(-255,270,34):mesh('Traverse de grille',(x+xx,y,4),(10,142,3),dark,collision=False)
    for x,y in [(-1000,-500),(-350,350),(500,-1000),(950,650),(1500,200),(1600,1100)]:
        a=mesh('Point d’apparition automate',(x,y,108),(12,12,12),teal,collision=False);a.tags=[ue.Name('Automate')]
    for x,y in [(-2100,700),(-2000,1450),(550,1560),(1750,-1500)]:
        prop('Boiler' if pumping else 'Turbine',(x,y,0),yaw=135 if pumping else 90,scale=.65)
    for x,y in [(-2240,-760),(-1450,1570),(1220,1560),(1980,-1120),(2170,600)]:
        prop('Cargo',(x,y,0),yaw=132,scale=1)
        prop('Cargo',(x+75,y+75,98),yaw=162,scale=.67)
    for x in range(-1800,2100,450):mesh('Conduite au pied du mur',(x,1740,48),(440,22,22),copper,collision=False)
    key,text=('Pumping','STATION DE POMPAGE') if pumping else ('Turbines','HALLE DES TURBINES')
    label(key,text,(0,1765,305),-90,54)
    ue.TrainWorldAuthoring.build_navigation(LEVELS.get_current_level(),ue.Vector(2400,1800,300))
    LEVELS.save_current_level()
AS.save_directory('/Game/Maps',recursive=True)
LEVELS.load_level('/Game/Maps/Menu')
print('TRAIN_WORLD_REBUILT')
