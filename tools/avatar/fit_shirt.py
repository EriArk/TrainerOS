"""Blender CLI: fit the downloaded tank, preserving its real garment openings.
Arguments after --: source.blend source-coords.npz output.glb.
"""
import bpy,bmesh,numpy as np,sys
from pathlib import Path
from mathutils import Matrix
source,coords_path,output=sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.open_mainfile(filepath=source)
coords=np.load(coords_path);items=[]
for ob in list(bpy.context.scene.objects):
    if ob.type!='MESH' or ob.name not in coords:
        bpy.data.objects.remove(ob,do_unlink=True);continue
    v=coords[ob.name].copy();ob.parent=None;ob.matrix_world=Matrix.Identity(4);ob.data=ob.data.copy()
    v[:,0]*=.00070;v[:,1]=(v[:,1]-8)*.00082;v[:,2]=(v[:,2]-1017.58)/596.28*.265+.427
    shoulder=np.clip((v[:,2]-.59)/.08,0,1)
    v[:,0]*=1-.17*shoulder;v[:,1]*=1-.48*shoulder;v[:,2]-=.007*shoulder
    ob.data.vertices.foreach_set('co',v.astype('f').ravel());items.append(ob)
for ob in bpy.context.scene.objects:ob.select_set(False)
for ob in items:ob.select_set(True)
bpy.context.view_layer.objects.active=items[0];bpy.ops.object.join();ob=bpy.context.object;ob.name='TankTop'
bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
triangles=sum(len(p.vertices)-2 for p in ob.data.polygons)
mod=ob.modifiers.new('Lightweight garment','DECIMATE');mod.ratio=min(1,4000/triangles);bpy.ops.object.modifier_apply(modifier=mod.name)
bm=bmesh.new();bm.from_mesh(ob.data)
for _ in range(8):bmesh.ops.smooth_vert(bm,verts=[v for v in bm.verts if not v.is_boundary],factor=.3,use_axis_x=True,use_axis_y=True,use_axis_z=True)
for _ in range(4):
    moves={}
    for vertex in bm.verts:
        neighbors=[e.other_vert(vertex) for e in vertex.link_edges if e.is_boundary]
        if len(neighbors)==2:moves[vertex]=vertex.co*.6+(neighbors[0].co+neighbors[1].co)*.2
    for vertex,point in moves.items():vertex.co=point
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
mat=bpy.data.materials.new('ShirtColor');mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.82,.37,.25,1);bs.inputs['Roughness'].default_value=.85
ob.data.materials.clear();ob.data.materials.append(mat)
for polygon in ob.data.polygons:polygon.material_index=0;polygon.use_smooth=True
ob.data.normals_split_custom_set([(0,0,0)]*len(ob.data.loops))
Path(output).parent.mkdir(parents=True,exist_ok=True)
bpy.ops.export_scene.gltf(filepath=output,export_format='GLB',use_selection=True,export_animations=False,export_skins=False)
