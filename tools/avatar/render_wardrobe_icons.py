"""Render wardrobe thumbnails from the shipped meshes, with transparent backgrounds.

blender --background --python tools/avatar/render_wardrobe_icons.py
No geometry or material changes are written back to the avatar pack.
"""
import bpy
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / 'assets/avatar/packs/trainer-avatar-1'
OUT = ROOT / 'assets/avatar/wardrobe-icons'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 20
scene.cycles.use_denoising = True
scene.render.resolution_x = scene.render.resolution_y = 256
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.world.color = (.3, .3, .3)
scene.view_settings.view_transform = 'AgX'
for name, loc, power, size in [('Key', (-2,-3,4), 260, 3), ('Fill', (3,-2,2), 160, 3), ('Rim', (0,2,3), 220, 2)]:
    data=bpy.data.lights.new(name,'AREA'); data.energy=power; data.size=size
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=loc
    obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Icon');camera=bpy.data.objects.new('Icon',data)
scene.collection.objects.link(camera);scene.camera=camera;data.type='ORTHO'

def imported(path):
    before=set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    objects=list(set(bpy.data.objects)-before)
    for obj in objects:
        if obj.animation_data: obj.animation_data_clear()
        if obj.type=='MESH':
            obj.hide_render=True
            if obj.data.shape_keys:
                obj.data.shape_keys.animation_data_clear()
                for key in obj.data.shape_keys.key_blocks[1:]:key.value=0
    return objects

def render(name, objects, angle=25, target=None, size=None):
    for obj in objects:obj.hide_render=False
    bpy.context.view_layer.update()
    points=[]
    deps=bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        evaluated=obj.evaluated_get(deps);mesh=evaluated.to_mesh()
        used={i for p in mesh.polygons for i in p.vertices}
        points.extend(evaluated.matrix_world@mesh.vertices[i].co for i in used)
        evaluated.to_mesh_clear()
    lo=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    hi=Vector(tuple(max(p[i] for p in points) for i in range(3)))
    center=Vector(target) if target else (lo+hi)/2
    data.ortho_scale=size or max(hi.x-lo.x,hi.z-lo.z,hi.y-lo.y)*1.35
    print('ICON_BOUNDS',name,tuple(lo),tuple(hi),data.ortho_scale,flush=True)
    a=math.radians(angle)
    camera.location=center+Vector((2*math.sin(a),-2*math.cos(a),.65))
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(OUT/(name+'.png'))
    bpy.ops.render.render(write_still=True)
    for obj in objects:obj.hide_render=True

base=imported(PACK/'template.glb')
for name,node in [('tank','shirt'),('sandals','shoes')]:
    render(name,[o for o in base if o.type=='MESH' and o.name==node],-25 if name=='sandals' else 18)
for path in sorted(PACK.glob('wear-*.glb')):
    objects=imported(path)
    name=path.stem.removeprefix('wear-')
    render(name,[o for o in objects if o.type=='MESH' and o.name==name],35 if 'shoes' in name else -25 if 'cap' in name or 'visor' in name else 18)
    for obj in objects:bpy.data.objects.remove(obj,do_unlink=True)
for name in ['swept','ponytail','crop','bob']:
    matches=[o for o in base if o.type=='MESH' and o.name=='hair-'+name]
    if matches:render(name,matches,25)
# The basic shorts are part of the approved body mesh. Extract only their
# existing faces for this presentation render; never modify the source body.
body=next(o for o in base if o.type=='MESH' and o.name.startswith('tripo_mesh'))
copy=body.copy();copy.data=body.data.copy();scene.collection.objects.link(copy)
import bmesh
bm=bmesh.new();bm.from_mesh(copy.data)
mask=bpy.data.images.load(str(PACK/'mask-round.png'));pixels=list(mask.pixels);mw,mh=mask.size
uvlayer=bm.loops.layers.uv.active
def shorts(face):
    if not uvlayer:return False
    uv=sum((loop[uvlayer].uv for loop in face.loops),Vector((0,0)))/len(face.loops)
    x=min(mw-1,max(0,int(uv.x*mw)));y=min(mh-1,max(0,int(uv.y*mh)))
    return pixels[(y*mw+x)*4+1]>.5
remove=[f for f in bm.faces if not shorts(f)]
bmesh.ops.delete(bm,geom=remove,context='FACES');bm.to_mesh(copy.data);bm.free()
render('base-shorts',[copy],18)
for style in ['rogue','innocent','sly','calm','sleepy','determined']:
    for variant in ['base','feminine']:
        name=style+'-'+variant
        render(name,[o for o in base if o.type=='MESH' and o.name.startswith('eyes-'+name+'-') and not o.name.endswith('reactions')],0)
for name in ['round','almond']:
    render(name,[o for o in base if o.type=='MESH' and o.name.startswith('eyes-'+name+'-') and not o.name.endswith('reactions')],0)
render('skin',[body],0,target=(0,-.02,.83),size=.38)
print('WARDROBE_ICONS_READY',len(list(OUT.glob('*.png'))))
