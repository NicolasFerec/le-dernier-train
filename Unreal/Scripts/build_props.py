"""Blender authoring source for the workshop furniture and industrial environment kit.
Metres; bevelled solids, real pipework, fasteners and readable silhouettes. No runtime mesh generator.
"""
from pathlib import Path
import math, random
import sys
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]/'SourceArt/Props'
ROOT.mkdir(parents=True,exist_ok=True)
random.seed(41)
def mat(name,color,metal=0,rough=.5):
    m=bpy.data.materials.get(name) or bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough
    return m
def clear():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
def finish(o,m,bevel=.015):
    o.data.materials.append(m)
    if bevel:
        mod=o.modifiers.new('Machined edge highlights','BEVEL');mod.width=bevel;mod.segments=3
        o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
    return o
def box(pos,size,m,bevel=.015,rot=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=pos);o=bpy.context.object;o.dimensions=size;o.rotation_euler.z=rot
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,m,bevel)
def cyl(pos,r,depth,m,rot=(0,0,0),vertices=48):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=depth,location=pos,rotation=rot);return finish(bpy.context.object,m,.008)
def sphere(pos,r,m,scale=(1,1,1)):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=16,radius=r,location=pos);o=bpy.context.object;o.scale=scale
    for p in o.data.polygons:p.use_smooth=True
    return finish(o,m,0)
def torus(pos,r,t,m,rot=(0,0,0)):
    bpy.ops.mesh.primitive_torus_add(major_segments=48,minor_segments=12,location=pos,major_radius=r,minor_radius=t,rotation=rot);o=bpy.context.object
    for p in o.data.polygons:p.use_smooth=True
    return finish(o,m,0)
def tube(points,r,m):
    c=bpy.data.curves.new('Bent copper conduit','CURVE');c.dimensions='3D';c.bevel_depth=r;c.bevel_resolution=4
    s=c.splines.new('BEZIER');s.bezier_points.add(len(points)-1)
    for p,v in zip(s.bezier_points,points):p.co=v;p.handle_left_type=p.handle_right_type='AUTO'
    o=bpy.data.objects.new('Copper pipe',c);bpy.context.collection.objects.link(o);o.data.materials.append(m)
def bolt_ring(pos,r,m,count=12,front=False):
    for n in range(count):
        a=n*math.tau/count;v=(pos[0]+r*math.cos(a),pos[1] if front else pos[1]+r*math.sin(a),pos[2]+r*math.sin(a) if front else pos[2])
        cyl(v,.018,.025,m,(math.pi/2,0,0) if front else (0,0,0),6)
def gauge(x,y,z,r=.14):
    cyl((x,y,z),r,.055,brass,(math.pi/2,0,0));cyl((x,y-.035,z),r*.86,.012,ivory,(math.pi/2,0,0))
    for n in range(11):
        a=math.radians(30+n*24);box((x+math.cos(a)*r*.69,y-.045,z+math.sin(a)*r*.69),(.012,.008,.025),iron,.001,0)
    o=box((x+.025,y-.057,z+.025),(.012,.012,r*.9),red,.002);o.rotation_euler.y=-.7
def gear(pos,r,front=False):
    rot=(math.pi/2,0,0) if front else (0,0,0);cyl(pos,r*.82,.055,brass,rot);cyl(pos,r*.22,.095,iron,rot)
    for n in range(16):
        a=math.tau*n/16
        if front:
            o=box((pos[0]+r*.88*math.cos(a),pos[1],pos[2]+r*.88*math.sin(a)),(.07,.06,.1),brass,.008);o.rotation_euler.y=-a
        else:box((pos[0]+r*.88*math.cos(a),pos[1]+r*.88*math.sin(a),pos[2]),(.08,.1,.06),brass,.008,a)
def feet(width=.7,depth=.35,height=.9):
    for x in [-width,width]:
        for y in [-depth,depth]:
            box((x,y,height/2),(.09,.09,height),iron);box((x,y,.055),(.16,.16,.1),brass)
def save(name):
    # Convert curves and bake bevels before combining; retain material slots.
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.convert(target='MESH');bpy.context.view_layer.objects.active=bpy.context.selected_objects[0]
    bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/(name+'.blend')))
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False)

clear()
brass=mat('LDT_Brass',(.48,.25,.095),.7,.35);iron=mat('LDT_Iron',(.065,.105,.125),.65,.46)
wood=mat('LDT_Walnut',(.2,.085,.036),0,.55);wine=mat('LDT_Oxblood',(.28,.045,.055),0,.5)
ivory=mat('LDT_Ivory',(.75,.65,.43),.05,.55);red=mat('LDT_EnamelRed',(.46,.075,.035),.15,.38)
teal=mat('LDT_Verdigris',(.055,.31,.27),.4,.35);glass=mat('LDT_AmberGlass',(1,.43,.085),.1,.25)

def turbine():
    horizontal=(0,math.pi/2,0)
    box((0,0,.18),(3.9,2.25,.36),iron,.08)
    box((0,0,.39),(3.55,1.91,.12),brass,.045)
    for x in [-1.13,1.13]:
        box((x,0,.67),(.52,1.65,.56),iron,.05)
    cyl((0,0,1.16),.74,2.5,red,horizontal)
    for x in [-1.24,-.9,-.58,-.26,.06,.38,.7,1.16]:
        torus((x,0,1.16),.73,.035,iron,horizontal)
    for x in [-1.25,1.25]:
        cyl((x,0,1.16),.86,.17,iron,horizontal)
        torus((x*1.09,0,1.16),.76,.049,brass,horizontal)
        for i in range(12):
            a=math.tau*i/12
            cyl((x*1.13,.76*math.cos(a),1.16+.76*math.sin(a)),.032,.07,brass,horizontal,8)
    cyl((-1.66,0,1.16),.37,.62,brass,horizontal);cyl((1.67,0,1.16),.28,.8,iron,horizontal)
    for x in [-.7,.7]:
        tube([(x,.52,1.6),(x,.94,1.52),(x,1.02,.79),(x,1.25,.55)],.085,brass)
        gauge(x,-.71,1.58,.13)
    for x in [-.48,0,.48]:box((x,-.66,1.05),(.23,.06,.095),glass,.018)
    save('Turbine');clear()

def cargo():
    box((0,0,.47),(1.45,1.12,.88),wood,.055)
    for x in [-.59,.59]:
        box((x,0,.46),(.095,1.16,.92),iron,.015)
        for z in [.15,.77]:cyl((x,-.59,z),.031,.025,brass,(math.pi/2,0,0),8)
    box((0,0,.93),(1.51,1.17,.08),wood,.027)
    for y in [-.39,0,.39]:box((0,y,.975),(1.36,.02,.008),iron,.001)
    for x in [-.52,.52]:box((x,0,.045),(.16,1.1,.09),iron,.01)
    box((0,-.568,.52),(.29,.027,.22),ivory,.012)
    box((0,-.586,.56),(.23,.014,.026),red,.002)
    save('Cargo');clear()

if '--set-dressing' in sys.argv:
    turbine();cargo();print('TRAIN_PROPS_AUTHORED');sys.exit(0)

# ARSENAL: riveted workshop bench, drawers, pegboard, tool silhouettes and vice.
feet(1,.38,1);box((0,0,1.05),(2.35,1.05,.14),wood,.035)
for x in [-1.08,1.08]:box((x,0,1.13),(.055,1.06,.04),brass)
box((0,.43,1.8),(2.2,.12,1.4),iron,.03)
for x in [-1,1]:box((x,.35,1.8),(.07,.05,1.4),brass)
for x in [-.9,-.3,.3,.9]:
    box((x,.32,1.55),(.04,.045,.08),brass)
    box((x,.29,1.9),(.045,.06,.65),brass,.01,(-.25 if x<0 else .25))
    if x>0:box((x,.29,2.15),(.28,.15,.14),iron)
for x in [-.72,.72]:
    box((x,0,.64),(.55,.7,.6),wine)
    for z in [.46,.69,.88]:box((x,-.37,z),(.44,.025,.04),brass)
box((.7,-.3,1.23),(.35,.27,.2),iron);cyl((.7,-.5,1.22),.035,.25,brass,(math.pi/2,0,0))
gear((-.55,-.12,1.17),.22);tube([(-.8,.38,1.2),(-.8,.38,2.4),(.9,.38,2.4)],.025,brass)
save('Arsenal');clear()

# WARDROBE: full length freestanding mirror with brass crest and coat rack.
feet(.6,.3,.3);box((0,0,1.33),(1.25,.23,2.25),wood,.09)
box((0,-.14,1.35),(1.01,.03,1.97),teal,.12)
for x in [-.55,.55]:box((x,-.18,1.35),(.055,.05,2.05),brass,.015)
for z in [.31,2.4]:box((0,-.18,z),(1.12,.05,.055),brass,.015)
gear((0,-.13,2.54),.23,True);sphere((0,0,2.8),.07,brass)
for x in [-.9,.9]:
    cyl((x,0,1.15),.035,2.25,brass);sphere((x,0,2.33),.07,brass)
    tube([(x,0,1.9),(x-.2,0,2.02),(x-.23,-.1,2.15)],.03,brass)
    box((x,0,.1),(.5,.55,.12),iron)
save('Wardrobe');clear()

# PASSIVES: armillary orrery, gearing and a book on a sloping lectern.
cyl((0,0,.12),.62,.24,iron);cyl((0,0,.28),.49,.12,brass);cyl((0,0,.77),.13,.9,brass)
cyl((0,0,1.25),.42,.1,iron);sphere((0,0,1.85),.22,teal)
for angle in [0,.8,-.8]:torus((0,0,1.85),.61,.035,brass,(angle,math.pi/2,0))
torus((0,0,1.85),.75,.03,brass,(0,0,0));sphere((.64,0,1.85),.08,glass)
gear((0,0,1.32),.35);gear((.28,0,.38),.18)
o=box((0,-.55,1.1),(.9,.55,.09),wood);o.rotation_euler.x=.35
o=box((0,-.6,1.17),(.7,.45,.06),ivory);o.rotation_euler.x=.35
for x in [-.17,.17]:box((x,-.61,1.22),(.018,.31,.008),brass,.001)
save('Orrery');clear()

# INVITATIONS: telegraph, radio dials, antenna, coiled conductor and gramophone bell.
feet(.7,.34,.85);box((0,0,.94),(1.65,.95,.16),wood,.035);box((0,.1,1.3),(1.35,.6,.6),iron,.05)
for x in [-.4,0,.4]:gauge(x,-.22,1.4,.13)
for x in [-.48,-.16,.16,.48]:cyl((x,-.26,1.08),.045,.06,brass,(math.pi/2,0,0))
box((-.3,-.33,1.08),(.46,.25,.03),brass)
o=box((-.3,-.34,1.15),(.3,.07,.04),iron);o.rotation_euler.y=.2
tube([(.45,.2,1.6),(.55,.2,1.9),(.8,.2,2.05)],.045,brass)
bpy.ops.mesh.primitive_cone_add(vertices=48,radius1=.1,radius2=.32,depth=.4,location=(.86,.2,2.16),rotation=(0,.7,0));finish(bpy.context.object,brass,.01)
for n in range(9):torus((-.52,.17,1.65+n*.035),.12,.014,brass)
cyl((-.52,.17,2.03),.025,.5,iron);sphere((-.52,.17,2.29),.05,teal)
save('Telegraph');clear()

# DEPARTURE: map table, pressure dials, lever and route lamps.
feet(.85,.5,.95);o=box((0,0,1.03),(2,1.35,.15),wood,.04);o.rotation_euler.x=.12
box((0,-.05,1.14),(1.65,1,.025),ivory)
for y in [-.35,0,.35]:tube([(-.7,y,1.16),(-.25,y+.12,1.16),(.2,y-.15,1.16),(.65,y+.08,1.16)],.009,brass)
for x,y in [(-.55,-.35),(0,.12),(.57,.35)]:cyl((x,y,1.18),.045,.025,teal)
box((0,.52,1.5),(1.95,.22,.6),iron)
for x in [-.6,0,.6]:gauge(x,.38,1.56,.15)
tube([(.88,-.35,1.1),(.88,-.35,1.5),(.88,-.53,1.64)],.025,brass);sphere((.88,-.53,1.64),.07,red)
save('Departure');clear()

# PRACTICE: target with steel shoulder plates, timber post, spring and anchor bolts.
cyl((0,0,.07),.46,.14,iron);bolt_ring((0,0,.15),.35,brass)
cyl((0,0,.59),.065,1,wood);box((0,0,1.25),(.65,.2,.75),iron,.06)
for x in [-.4,.4]:sphere((x,0,1.45),.18,brass,(1,.8,.8))
for r,m,y in [(.3,brass,-.13),(.25,ivory,-.16),(.18,red,-.18),(.1,ivory,-.2),(.045,red,-.22)]:cyl((0,y,1.3),r,.045,m,(math.pi/2,0,0))
sphere((0,0,1.85),.18,iron);gauge(0,-.14,1.87,.1)
for n in range(7):torus((0,0,.24+n*.035),.1,.015,brass)
save('TrainingDummy');clear()

# Large pump/boiler: cast body, flanged pressure pipes, access hatch and handwheel.
cyl((0,0,.17),1.05,.34,iron);cyl((0,0,1.35),.88,2.1,teal)
for z in [.4,1.1,2.35]:
    torus((0,0,z),.9,.055,brass);bolt_ring((0,0,z),.88,brass,16)
sphere((0,0,2.38),.88,iron,(1,1,.35));cyl((0,0,2.8),.25,.55,brass)
for x in [-.7,.7]:
    tube([(x,0,2.5),(x*1.8,0,2.4),(x*1.8,0,.7),(x*1.5,-.8,.3)],.12,brass)
    torus((x*1.8,0,1.6),.145,.035,iron)
cyl((0,-.91,1.15),.42,.14,iron,(math.pi/2,0,0));bolt_ring((0,-1,1.15),.34,brass,12,True)
torus((0,-1.07,1.15),.29,.035,brass,(math.pi/2,0,0))
for a in [0,math.pi/3,2*math.pi/3]:o=box((0,-1.07,1.15),(.53,.045,.035),brass);o.rotation_euler.y=a
gauge(0,-.85,2,.23);save('Boiler');clear()

# Repeated arch and lamp used to give the long train a structural rhythm.
for x in [-.8,.8]:
    box((x,0,1.25),(.18,.24,2.5),iron,.025)
    for z in [.25,.6,1,1.4,1.8,2.2]:cyl((x,-.13,z),.035,.025,brass,(math.pi/2,0,0),8)
tube([(-.8,0,2.3),(-.65,0,2.65),(0,0,2.9),(.65,0,2.65),(.8,0,2.3)],.11,iron)
tube([(-.8,-.12,2.3),(-.65,-.12,2.65),(0,-.12,2.9),(.65,-.12,2.65),(.8,-.12,2.3)],.025,brass)
save('Arch');clear()
cyl((0,0,.08),.22,.16,brass);cyl((0,0,.34),.14,.4,glass)
for a in range(6):
    t=a*math.tau/6;cyl((.16*math.cos(t),.16*math.sin(t),.35),.018,.48,iron)
cyl((0,0,.61),.22,.08,brass);sphere((0,0,.68),.14,iron,(1,1,.45));torus((0,0,.82),.08,.018,brass,(math.pi/2,0,0))
save('Lantern');clear()
for x in [-1,1]:
    box((x,0,1.45),(.28,.55,2.9),iron,.04)
    for z in [.2,.65,1.1,1.6,2.1,2.6]:cyl((x,-.29,z),.04,.035,brass,(math.pi/2,0,0),8)
box((0,0,2.9),(2.3,.55,.3),iron,.04)
tube([(-1,-.32,2.4),(-.7,-.32,2.9),(0,-.32,3.05),(.7,-.32,2.9),(1,-.32,2.4)],.05,brass)
for x in [-1.23,1.23]:
    gear((x,-.05,2.2),.32,True);sphere((x,-.2,1.4),.13,glass)
for x in [-.65,.65]:box((x,0,1.38),(.65,.15,2.55),teal,.025)
box((0,0,1.4),(.05,.2,2.6),brass)
save('ExitGate');clear()
turbine();cargo()
print('TRAIN_PROPS_AUTHORED')
