# SPDX-License-Identifier: GPL-3.0-or-later
extends SceneTree
const Presenter = preload("../avatar_presenter.gd")
var failed := false
func check(value: bool, label: String) -> void:
	if not value: failed=true;printerr(label)
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var scene := Node3D.new();root.add_child(scene)
	var instance := MeshInstance3D.new();scene.add_child(instance)
	var mesh := ArrayMesh.new();mesh.add_blend_shape("Corrective")
	var arrays := [];arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX]=PackedVector3Array([Vector3.ZERO,Vector3.RIGHT,Vector3.UP])
	arrays[Mesh.ARRAY_NORMAL]=PackedVector3Array([Vector3.BACK,Vector3.BACK,Vector3.BACK])
	arrays[Mesh.ARRAY_TEX_UV]=PackedVector2Array([Vector2.ZERO,Vector2.RIGHT,Vector2.UP])
	arrays[Mesh.ARRAY_COLOR]=PackedColorArray([Color(0.25,0.5,0.75,0.5),Color.WHITE,Color.BLACK])
	arrays[Mesh.ARRAY_BONES]=PackedInt32Array([0,0,0,0,0,0,0,0,0,0,0,0])
	arrays[Mesh.ARRAY_WEIGHTS]=PackedFloat32Array([1,0,0,0,1,0,0,0,1,0,0,0])
	arrays[Mesh.ARRAY_INDEX]=PackedInt32Array([0,1,2])
	var shape := [];shape.resize(Mesh.ARRAY_MAX)
	shape[Mesh.ARRAY_VERTEX]=PackedVector3Array([Vector3(0,0,0.1),Vector3.RIGHT,Vector3.UP])
	shape[Mesh.ARRAY_NORMAL]=arrays[Mesh.ARRAY_NORMAL]
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES,arrays,[shape])
	var material := StandardMaterial3D.new();material.vertex_color_use_as_albedo=true
	mesh.surface_set_material(0,material);instance.mesh=mesh
	var before := RenderingServer.mesh_get_surface(mesh.get_rid(),0)
	check(Presenter._prepare_vertex_colors(scene),"Color adaptation failed")
	var after := RenderingServer.mesh_get_surface(mesh.get_rid(),0)
	for key in before:
		if key!="attribute_data": check(before[key]==after[key],"Changed non-color buffer: "+str(key))
	var colors: PackedColorArray = mesh.surface_get_arrays(0)[Mesh.ARRAY_COLOR]
	var expected := Color(64/255.0,128/255.0,191/255.0,128/255.0)
	if DisplayServer.get_name()!="headless" and RenderingServer.get_current_rendering_method()=="gl_compatibility": expected=expected.linear_to_srgb()
	check(absf(colors[0].r-expected.r)<0.006 and absf(colors[0].g-expected.g)<0.006 and absf(colors[0].b-expected.b)<0.006,"Wrong vertex color space")
	check(absf(colors[0].a-0.5)<0.006,"Alpha changed")
	check(mesh.surface_get_arrays(0)[Mesh.ARRAY_TEX_UV]==arrays[Mesh.ARRAY_TEX_UV],"UV changed")
	print("VERTEX_COLORS ",RenderingServer.get_current_rendering_method()," success=",not failed)
	quit(1 if failed else 0)
