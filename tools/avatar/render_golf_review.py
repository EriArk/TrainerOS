"""Blender: render actual fitted golf meshes and save an editable wardrobe.
blender --background --python tools/avatar/render_golf_review.py -- --male ...glb --female ...glb --out build/golf-outfits/render
"""
import bpy,sys,argparse,math,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/avatar'))
from golf_hair_fit import fit_positions
import numpy as np
parser=argparse.ArgumentParser();parser.add_argument('--male',type=Path,required=True);parser.add_argument('--female',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--quick',action='store_true');parser.add_argument('--duo-only',action='store_true');parser.add_argument('--sex',default='both');parser.add_argument('--poses',default='');args=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);args.out=(ROOT/args.out).resolve();args.out.mkdir(parents=True,exist_ok=True)
collection=ROOT/'assets/avatar/collections/golf-v1'

def clean():
    for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
    for action in bpy.data.actions:bpy.data.actions.remove(action)
def import_glb(path):
    before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(path));return list(set(bpy.data.objects)-before)
def fit_hair(objects,headwear,restore=False):
    for o in objects:
        if o.type!='MESH' or not o.name.startswith('hair'):continue
        if restore:
            if 'golf_original_positions' in o:
                for vertex,co in zip(o.data.vertices,np.array(o['golf_original_positions']).reshape(-1,3)):vertex.co=co
            continue
        points=np.array([v.co[:] for v in o.data.vertices]);o['golf_original_positions']=points.ravel().tolist()
        # Blender imports glTF (x,y,z) as (x,-z,y).
        fitted=fit_positions(points[:,[0,2,1]]*np.array([1,1,-1]),headwear)
        for vertex,co in zip(o.data.vertices,fitted[:,[0,2,1]]*np.array([1,-1,1])):vertex.co=co
        # Imported split normals belong to the uncompressed surface.
        if o.data.has_custom_normals:
            with bpy.context.temp_override(object=o,active_object=o):bpy.ops.mesh.customdata_custom_splitnormals_clear()
        o.data.update()
def light(name,loc,power,size,target=(0,0,.5)):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;o=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
def stage():
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=24;s.cycles.use_denoising=True
    try:
        prefs=bpy.context.preferences.addons['cycles'].preferences;prefs.compute_device_type='CUDA';prefs.get_devices()
        if any(d.type=='CUDA' for d in prefs.devices):
            for d in prefs.devices:d.use=d.type=='CUDA'
            s.cycles.device='GPU'
    except (TypeError,RuntimeError):pass
    s.render.resolution_x=880;s.render.resolution_y=1080;s.render.resolution_percentage=100
    s.world.color=(.38,.38,.38);s.view_settings.view_transform='AgX'
    s.render.image_settings.file_format='PNG';s.render.film_transparent=False
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.013));floor=bpy.context.object;floor.name='Studio floor'
    mat=bpy.data.materials.new('Warm studio');mat.diffuse_color=(.22,.27,.25,1);floor.data.materials.append(mat)
    light('Key',(-2,-3,4),330,4);light('Fill',(3,-1,2),180,3);light('Rim',(0,3,3),250,3)
    camera=bpy.data.cameras.new('Review');o=bpy.data.objects.new('Review',camera);bpy.context.collection.objects.link(o);s.camera=o;camera.type='ORTHO';camera.ortho_scale=1.30
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
                for b in o.pose.bones:b.matrix_basis.identity()
        else:
            for track in ad.nla_tracks:
                if name.lower() in track.name.lower() and track.strips:
                    strip=track.strips[0];ad.action=strip.action
                    if hasattr(strip,'action_slot') and strip.action_slot:ad.action_slot=strip.action_slot
                    break
    bpy.context.scene.frame_set(14 if name=='Wave' else 20)
def capture(name,angle=0):
    s=bpy.context.scene;camera=s.camera;angle=math.radians(angle)
    camera.location=(2*math.sin(angle),-2*math.cos(angle),.73);camera.rotation_euler=(Vector((0,0,.53))-camera.location).to_track_quat('-Z','Y').to_euler()
    s.render.filepath=str(args.out/(name+'.png'));bpy.ops.render.render(write_still=True)

if not args.quick and not args.duo_only:
    clean();all_items=import_glb(collection/'golf-wardrobe.glb');pose(all_items,'Rest')
    sets=json.loads((collection/'collection.json').read_text())['sets']
    for title,items in sets.items():
        group=bpy.data.collections.new(title+' - separate garments');bpy.context.scene.collection.children.link(group)
        for o in all_items:
            if o.type=='MESH' and o.name in items:
                for current in list(o.users_collection):current.objects.unlink(o)
                group.objects.link(o);o['trainerWearableId']=o.name
                if title=='womens':o.hide_set(True);o.hide_render=True
    bpy.context.scene['authoring_note']='Men visible by default. Unhide Women and hide Men to inspect the second set. Shared rig; no body or extra shorts under skirt.'
    bpy.ops.wm.save_as_mainfile(filepath=str(collection/'golf-wardrobe.blend'))
for sex,base in [('mens',args.male),('womens',args.female)]:
    if args.duo_only:break
    if args.sex!='both' and args.sex!=sex:continue
    clean();body=import_glb(base);fit_hair(body,'golf-cap' if sex=='mens' else 'golf-visor')
    for o in body:
        if o.name in ['shirt','shoes']:o.hide_render=True;o.hide_set(True)
    garments=import_glb(collection/(sex+'-outfit.glb'));stage();pose(body+garments,'Rest')
    if args.poses:
        for action in args.poses.split(','):
            pose(body+garments,action);capture(sex+'-'+action.lower(),22)
        continue
    capture(sex+'-front',0)
    if args.quick:continue
    capture(sex+'-three-quarter',32);capture(sex+'-back',180);capture(sex+'-side',90)
    for action in ['Wave','Arms_Up','Squat','Step','Sit']:
        pose(body+garments,action);capture(sex+'-'+action.lower(),22)
    pose(body+garments,'Rest')
    fit_hair(body,'',restore=True)
    for o in garments:
        if o.type=='MESH' and o.name.startswith(('golf-cap','golf-visor')):o.hide_render=True
    capture(sex+'-without-hat',15)
if args.duo_only:
    clean()
    for sex,base,offset in [('mens',args.male,-.31),('womens',args.female,.31)]:
        body=import_glb(base);fit_hair(body,'golf-cap' if sex=='mens' else 'golf-visor')
        for o in body:
            if o.type=='MESH' and (o.name.split('.')[0] in ['shirt','shoes']):o.hide_render=True
        garments=import_glb(collection/(sex+'-outfit.glb'));objects=body+garments;pose(objects,'Rest')
        for o in objects:
            if o.parent not in objects:o.location.x+=offset
    stage();bpy.context.scene.render.resolution_x=1600;bpy.context.scene.render.resolution_y=1100
    bpy.context.scene.camera.data.ortho_scale=1.62;capture('golf-collection',0)
print('GOLF_REVIEW_RENDERED')
