"""Remove the legacy rig, orient the bind mesh toward glTF +Z, preserve texture."""
from pathlib import Path
import math,json
import bpy
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/Conductor/UE5Rework'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'SourceArt/Characters/Conductor/conductor.glb'))
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
rig.data.pose_position='REST'
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
bones={b.name.rsplit(':',1)[-1]:b for b in rig.data.bones}
for name in ['Hips','LeftShoulder','RightShoulder','LeftFoot','LeftToeBase','RightFoot','RightToeBase']:
 print('INPUT_BONE',name,tuple(rig.matrix_world@bones[name].head_local))
front=sum((rig.matrix_world@(bones[s+'ToeBase'].head_local)-rig.matrix_world@(bones[s+'Foot'].head_local) for s in ['Left','Right']),Vector())
front.z=0;front.normalize()
yaw=-math.pi/2-math.atan2(front.y,front.x)
for obj in meshes:
 bpy.context.view_layer.objects.active=obj
 for mod in list(obj.modifiers):
  if mod.type=='ARMATURE':bpy.ops.object.modifier_apply(modifier=mod.name)
 w=obj.matrix_world.copy();obj.parent=None;obj.matrix_world=w
bpy.data.objects.remove(rig,do_unlink=True)
rotation=Matrix.Rotation(yaw,4,'Z')
for obj in meshes:obj.matrix_world=rotation@obj.matrix_world
bpy.context.view_layer.update()
points=[o.matrix_world@Vector(c) for o in meshes for c in o.bound_box]
lo=min(p.z for p in points);hi=max(p.z for p in points)
xmid=(max(p.x for p in points)+min(p.x for p in points))/2
ymid=(max(p.y for p in points)+min(p.y for p in points))/2
xf=Matrix.Scale(1.8/(hi-lo),4)@Matrix.Translation((-xmid,-ymid,-lo))
for obj in meshes:obj.matrix_world=xf@obj.matrix_world
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
for obj in meshes:obj.select_set(True)
bpy.context.view_layer.objects.active=meshes[0]
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.ops.export_scene.gltf(filepath=str(OUT/'conductor-rig-input.glb'),export_format='GLB',use_selection=True,export_animations=False)
scene=bpy.context.scene
scene.render.engine='CYCLES';scene.cycles.samples=16;scene.cycles.use_denoising=True
scene.render.resolution_x=640;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('ReviewWorld');scene.world.color=(.28,.28,.28)
bpy.ops.mesh.primitive_plane_add(size=200);floor=bpy.context.object
m=bpy.data.materials.new('ReviewFloor');m.diffuse_color=(.13,.16,.19,1);floor.data.materials.append(m)
for loc,power,size in [((3,-4,5),700,4),((-3,-1,3),350,3),((0,3,4),500,3)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=size;l.rotation_euler=(Vector((0,0,1))-l.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2.7,-5,2.7));cam=bpy.context.object;cam.rotation_euler=(Vector((0,0,.95))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=2.35;scene.camera=cam
scene.render.filepath=str(OUT/'input-preview.png');bpy.ops.render.render(write_still=True)
(OUT/'input-preparation.json').write_text(json.dumps({'source':'../conductor.glb','forward':'glTF +Z','height_m':1.8,'rotation_z_degrees':math.degrees(yaw),'source_faces':sum(len(o.data.polygons) for o in meshes)},indent=2))
print('TRAIN_CONDUCTOR_RIG_INPUT_READY',OUT)
