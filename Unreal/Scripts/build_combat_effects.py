"""Author simple tapered weapon trails and impact streaks in physical centimetres."""
import bpy, math
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'SourceArt/Effects'
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)

def export(name,verts,faces):
    bpy.ops.object.select_all(action='DESELECT')
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj)
    obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,global_scale=.01,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False)
    bpy.data.objects.remove(obj,do_unlink=True)

verts=[];faces=[]
for i in range(33):
    t=i/32;angle=math.radians(-78+t*156)
    width=math.sin(t*math.pi)**.65*23
    for radius in (156-width,156):verts.append((math.cos(angle)*radius,math.sin(angle)*radius,5))
    if i:faces.append((2*i-2,2*i-1,2*i+1,2*i))
export('Slash',verts,faces)
verts=[];faces=[]
for i in range(9):
    angle=i*math.tau/9
    direction=(math.cos(angle),math.sin(angle))
    perpendicular=(-direction[1],direction[0])
    length=30+(i%3)*15
    start=len(verts)
    for distance,width,z in ((8,0,0),(length*.55,-4,3),(length,0,8),(length*.55,4,3)):
        verts.append((direction[0]*distance+perpendicular[0]*width,direction[1]*distance+perpendicular[1]*width,z))
    faces.append(tuple(range(start,start+4)))
export('Impact',verts,faces)
