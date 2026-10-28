# Import validation only; this does not install clothing in the live wardrobe.
extends SceneTree
var folder := ""
var failed := false
var mesh_converter := GLTFDocumentExtensionConvertImporterMesh.new()
func _initialize() -> void:
	# Headless runtime import must register render-mesh conversion explicitly;
	# otherwise generated nodes remain ImporterMeshInstance3D, not render meshes.
	GLTFDocument.register_gltf_document_extension(mesh_converter)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--collection="): folder=arg.trim_prefix("--collection=")
	call_deferred("run")
func check(ok: bool, message: String) -> void:
	if not ok: failed=true;printerr(message)
func run() -> void:
	var manifest = JSON.parse_string(FileAccess.get_file_as_string(folder+"/collection.json"))
	var passed: Array = []
	for id in manifest.items:
		var document := GLTFDocument.new();var state := GLTFState.new()
		check(document.append_from_file(folder+"/"+manifest.items[id].file,state)==OK,"GLB import "+id)
		var scene := document.generate_scene(state)
		check(scene!=null,"scene "+id)
		if scene==null: continue
		root.add_child(scene)
		var skeletons=scene.find_children("*","Skeleton3D",true,false)
		check(skeletons.size()==1 and skeletons[0].get_bone_count()==54,"shared skeleton "+id)
		var meshes=scene.find_children("*","MeshInstance3D",true,false)
		check(meshes.size()==1,"separate wearable node "+id)
		if meshes.size()!=1:
			scene.queue_free();await process_frame;continue
		check(meshes[0].skin!=null and meshes[0].mesh.get_blend_shape_count()==46,"skin and correctives "+id)
		var players=scene.find_children("*","AnimationPlayer",true,false)
		check(players.size()==1,"animation player "+id)
		if players.size()!=1:
			scene.queue_free();await process_frame;continue
		for action in ["Wave","Arms_Up","Step","Squat","Sit"]:
			check(players[0].has_animation(action),"animation "+id+" "+action)
			if players[0].has_animation(action):
				players[0].play(action);players[0].seek(0.8,true)
		passed.append(id);scene.queue_free();await process_frame
	var result={"passed":not failed,"wearables":passed,"engine":Engine.get_version_info().string,"scope":"Headless GLB/skin/corrective/animation import; no gameplay or device installation."}
	var file=FileAccess.open(folder+"/godot-import.json",FileAccess.WRITE);file.store_string(JSON.stringify(result,"  ")+"\n");file.close()
	GLTFDocument.unregister_gltf_document_extension(mesh_converter)
	print("GOLF_IMPORT success=",not failed," wearables=",passed.size());quit(1 if failed else 0)
