# SPDX-License-Identifier: GPL-3.0-or-later
extends SceneTree
var folder := ""
var failed := false
var converter := GLTFDocumentExtensionConvertImporterMesh.new()
func _initialize() -> void:
	GLTFDocument.register_gltf_document_extension(converter)
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--descriptors="): folder=argument.trim_prefix("--descriptors=")
	call_deferred("run")
func check(ok: bool, message: String) -> void:
	if not ok: failed=true;printerr(message)
func run() -> void:
	var count := 0
	for file in DirAccess.get_files_at(folder):
		if not file.begins_with("wear-") or not file.ends_with(".json"): continue
		var descriptor=JSON.parse_string(FileAccess.get_file_as_string(folder.path_join(file)))
		var avatar=load("res://avatar_presenter.gd").from_descriptor(descriptor)
		check(avatar!=null,"Resolved wardrobe import: "+file)
		if avatar==null: continue
		root.add_child(avatar)
		var skeletons=avatar.find_children("*","Skeleton3D",true,false)
		check(skeletons.size()==1 and skeletons[0].get_bone_count()==54,"One shared skeleton: "+file)
		var garments := 0
		for node in avatar.find_children("*","MeshInstance3D",true,false):
			for slot in ["top","bottom","socks","shoes","headwear"]:
				if str(node.name)==descriptor.appearance[slot]:
					garments+=1
					check(node.skin!=null and node.mesh.get_blend_shape_count()==46,"Skinned garment correctives: "+str(node.name))
		check(garments==5,"All five selected garments: "+file)
		for expression in ["happy","love","shocked","cat_smug"]:
			check(avatar.set_expression(expression,1,0),"Face with wardrobe: "+expression)
		for action in ["wave","raise_arms","squat","step","sit"]:
			check(avatar.play_action(action),"Wardrobe action: "+action)
			avatar.animation_player.advance(0.5)
			await process_frame
		avatar.clear_identity();check(avatar.get_child_count()==0,"Identity cleanup")
		avatar.queue_free();await process_frame;count+=1
	check(count==8,"Expected eight hair/headwear combinations")
	GLTFDocument.unregister_gltf_document_extension(converter)
	print("WARDROBE success=",not failed," combinations=",count)
	quit(1 if failed else 0)
