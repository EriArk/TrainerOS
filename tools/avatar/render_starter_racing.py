"""Blender review of actual skinned starter/racing assets on approved avatars."""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--male',type=Path,required=True)
parser.add_argument('--female',type=Path,required=True)
parser.add_argument('--collection',type=Path,default=ROOT/'assets/avatar/collections/starter-racing-v1')
parser.add_argument('--quick',action='store_true')
parser.add_argument('--only',default='')
parser.add_argument('--lineup-only',action='store_true')
parser.add_argument('--poses',default='')
parser.add_argument('--save-authoring',action='store_true')
parser.add_argument('--views',default='')
parser.add_argument('--remaining',action='store_true')
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
folder=args.collection.resolve();out=folder/'preview';out.mkdir(exist_ok=True)
manifest=json.loads((folder/'collection.json').read_text())


def clean():
    for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
    for action in list(bpy.data.actions):bpy.data.actions.remove(action)
    bpy.data.orphans_purge(do_recursive=True)


def load(path):
    before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(path))
    return list(set(bpy.data.objects)-before)


def pose(objects,name):
    animated=list(objects)
    for o in objects:
        if o.type=='MESH' and o.data.shape_keys:
            animated.append(o.data.shape_keys)
            for key in o.data.shape_keys.key_blocks[1:]:key.value=0
    for o in animated:
        if not o.animation_data:continue
        ad=o.animation_data
        for track in ad.nla_tracks:track.mute=True
        if name=='Rest':
            ad.action=None
            if isinstance(o,bpy.types.Object) and o.type=='ARMATURE':
                for bone in o.pose.bones:bone.matrix_basis.identity()
        else:
            for track in ad.nla_tracks:
                if name.lower() in track.name.lower() and track.strips:
                    strip=track.strips[0];ad.action=strip.action
                    if hasattr(strip,'action_slot') and strip.action_slot:ad.action_slot=strip.action_slot
                    break
    bpy.context.scene.frame_set(14 if name=='Wave' else 20)


def light(name,location,power,size):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    o=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(o);o.location=location
    o.rotation_euler=(Vector((0,0,.5))-o.location).to_track_quat('-Z','Y').to_euler()


def stage():
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=16 if args.quick else 32;s.cycles.use_denoising=True
    try:
        prefs=bpy.context.preferences.addons['cycles'].preferences;prefs.compute_device_type='CUDA';prefs.get_devices()
        if any(d.type=='CUDA' for d in prefs.devices):
            for d in prefs.devices:d.use=d.type=='CUDA'
            s.cycles.device='GPU'
    except (TypeError,RuntimeError):pass
    s.render.resolution_x=800;s.render.resolution_y=1000;s.render.resolution_percentage=100
    s.world.color=(.35,.35,.35);s.view_settings.view_transform='AgX'
    s.render.image_settings.file_format='PNG';s.render.film_transparent=False
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.013))
    floor=bpy.context.object;floor.name='Studio floor'
    mat=bpy.data.materials.new('Studio');mat.diffuse_color=(.27,.29,.28,1);floor.data.materials.append(mat)
    light('Key',(-2,-3,4),330,4);light('Fill',(3,-1,2),180,3);light('Rim',(0,3,3),250,3)
    data=bpy.data.cameras.new('Review');data.type='ORTHO';data.ortho_scale=1.22
    o=bpy.data.objects.new('Review',data);bpy.context.collection.objects.link(o);s.camera=o


def capture(key,view,angle):
    s=bpy.context.scene;camera=s.camera;angle=math.radians(angle)
    camera.location=(2*math.sin(angle),-2*math.cos(angle),.68)
    camera.rotation_euler=(Vector((0,0,.51))-camera.location).to_track_quat('-Z','Y').to_euler()
    s.render.filepath=str(out/(key+'-'+view+'.png'));bpy.ops.render.render(write_still=True)


if args.save_authoring or (not args.quick and not args.only and not args.lineup_only and not args.poses):
    clean();objects=load(folder/'starter-racing-wardrobe.glb');pose(objects,'Rest')
    for title,items in manifest['sets'].items():
        group=bpy.data.collections.new(title);bpy.context.scene.collection.children.link(group)
        for o in objects:
            if o.type=='MESH' and o.name in items:
                for current in list(o.users_collection):current.objects.unlink(o)
                group.objects.link(o);o['trainerWearableId']=o.name
                o.hide_set(title!='starter-sundress');o.hide_render=title!='starter-sundress'
    bpy.context.scene['authoring_note']='Four separate outfit collections. Enable one at a time. Shared rig and correctives; no body duplication, no cloth simulation.'
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/'starter-racing-wardrobe.blend'))

for key in manifest['sets']:
    if args.lineup_only:break
    if args.only and key!=args.only:continue
    clean();female=key in ['starter-sundress','racing-rose'];body=load(args.female if female else args.male)
    for o in body:
        if o.type!='MESH':continue
        if o.name.split('.')[0]=='shirt' or (o.name.split('.')[0]=='shoes' and key!='starter-sundress'):
            o.hide_render=True;o.hide_set(True)
        if 'racing' in key and o.name.startswith('hair'):
            o.hide_render=True;o.hide_set(True)
    clothes=load(folder/(key+'-outfit.glb'));stage();pose(body+clothes,'Rest')
    if args.poses:
        for action in args.poses.split(','):
            pose(body+clothes,action);capture(key,action.lower(),25)
        continue
    if args.views:
        angles={'front':0,'three-quarter':32,'side':90,'back':180}
        for view in args.views.split(','):capture(key,view,angles[view])
        continue
    if not args.remaining:capture(key,'front',0)
    if args.quick:continue
    for view,angle in [('three-quarter',32),('side',90),('back',180)]:
        if not args.remaining or view!='side':capture(key,view,angle)
    for action in ['Arms_Up','Step','Squat','Twist']:
        pose(body+clothes,action);capture(key,action.lower(),25)
    if 'racing' in key:
        pose(body+clothes,'Rest')
        for o in clothes:
            if o.type=='MESH' and 'helmet' in o.name:o.hide_render=True
        for o in body:
            if o.type=='MESH' and o.name.startswith('hair'):o.hide_render=False
        capture(key,'without-helmet',15)

if args.lineup_only:
    clean()
    for key,offset in zip(manifest['sets'],[-.81,-.27,.27,.81]):
        female=key in ['starter-sundress','racing-rose']
        body=load(args.female if female else args.male)
        for o in body:
            if o.type!='MESH':continue
            if o.name.split('.')[0]=='shirt' or (o.name.split('.')[0]=='shoes' and key!='starter-sundress'):
                o.hide_render=True
            if 'racing' in key and o.name.startswith('hair'):o.hide_render=True
        clothes=load(folder/(key+'-outfit.glb'));objects=body+clothes;pose(objects,'Rest')
        for o in objects:
            if o.parent not in objects:o.location.x+=offset
    stage();s=bpy.context.scene
    s.render.resolution_x=2400;s.render.resolution_y=1250
    s.camera.data.ortho_scale=2.52
    capture('four-outfits','lineup',0)

proof={'renderer':'Blender / Cycles','collectionSha256':hashlib.sha256((folder/'collection.json').read_bytes()).hexdigest(),
       'baseAvatarSha256':{'male':hashlib.sha256(args.male.read_bytes()).hexdigest(),'female':hashlib.sha256(args.female.read_bytes()).hexdigest()},
       'shots':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.glob('*.png'))},
       'scope':'Actual asset renders and sampled existing poses. Not gameplay or device acceptance.'}
(folder/'render-evidence.json').write_text(json.dumps(proof,indent=2)+'\n')
print('STARTER_RACING_REVIEW_RENDERED',flush=True)
