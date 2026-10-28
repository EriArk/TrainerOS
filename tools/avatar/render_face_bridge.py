"""Blender close-ups of the actual shared-face repair, under identical lighting."""
import argparse
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

p=argparse.ArgumentParser();p.add_argument('--before',type=Path,required=True);p.add_argument('--after',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--clay',action='store_true')
p.add_argument('--after-only',action='store_true');p.add_argument('--expression')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.before=a.before.resolve();a.after=a.after.resolve();a.output=a.output.resolve()
a.output.mkdir(parents=True,exist_ok=True)

for label,path in [('before',a.before),('after',a.after)]:
    if a.after_only and label=='before':continue
    for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
    for action in list(bpy.data.actions):bpy.data.actions.remove(action)
    bpy.data.orphans_purge(do_recursive=True)
    bpy.ops.import_scene.gltf(filepath=str(path))
    for obj in bpy.data.objects:
        if obj.animation_data:obj.animation_data_clear()
        if obj.type=='ARMATURE':
            for bone in obj.pose.bones:bone.matrix_basis.identity()
        if obj.type=='MESH' and obj.data.shape_keys:
            obj.data.shape_keys.animation_data_clear()
            for key in obj.data.shape_keys.key_blocks[1:]:key.value=0
            if a.expression and a.expression in obj.data.shape_keys.key_blocks:
                obj.data.shape_keys.key_blocks[a.expression].value=1
    if a.clay:
        for mat in bpy.data.materials:
            if mat.name not in ['Face skin transition','Body']:continue
            mat.use_nodes=True;shader=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
            for key in ['Base Color','Normal']:
                for link in list(shader.inputs[key].links):mat.node_tree.links.remove(link)
            shader.inputs['Base Color'].default_value=(.28,.28,.28,1)
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=24;s.cycles.use_denoising=True
    prefs=bpy.context.preferences.addons['cycles'].preferences;prefs.compute_device_type='CUDA';prefs.get_devices()
    for device in prefs.devices:device.use=device.type=='CUDA'
    s.cycles.device='GPU';s.world.color=(.35,.35,.35);s.view_settings.view_transform='AgX'
    s.render.resolution_x=1000;s.render.resolution_y=1000;s.render.resolution_percentage=100
    s.render.image_settings.file_format='PNG'
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.013))
    floor=bpy.context.object;mat=bpy.data.materials.new('Studio');mat.diffuse_color=(.27,.29,.28,1)
    floor.data.materials.append(mat)
    for name,location,power,size in [('Key',(-2,-3,4),330,4),('Fill',(3,-1,2),180,3),('Rim',(0,3,3),250,3)]:
        data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
        obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);obj.location=location
        obj.rotation_euler=(Vector((0,0,.5))-obj.location).to_track_quat('-Z','Y').to_euler()
    data=bpy.data.cameras.new('Face review');data.type='ORTHO';data.ortho_scale=.43
    camera=bpy.data.objects.new('Face review',data);bpy.context.collection.objects.link(camera);s.camera=camera
    for view,angle in [('front',0),('three-quarter',32)]:
        angle=math.radians(angle);camera.location=(2*math.sin(angle),-2*math.cos(angle),.83)
        camera.rotation_euler=(Vector((0,0,.81))-camera.location).to_track_quat('-Z','Y').to_euler()
        s.render.filepath=str(a.output/(label+'-'+view+('-'+a.expression if a.expression else '')+('-clay' if a.clay else '')+'.png'))
        bpy.ops.render.render(write_still=True)
print('FACE_BRIDGE_RENDERED',flush=True)
