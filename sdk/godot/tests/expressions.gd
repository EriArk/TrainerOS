# SPDX-License-Identifier: GPL-3.0-or-later
extends SceneTree
var descriptor_path := ""
var output := ""
var failed := false
var legacy := false
func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--descriptor="): descriptor_path = arg.trim_prefix("--descriptor=")
		if arg.begins_with("--output="): output = arg.trim_prefix("--output=")
		if arg=="--legacy": legacy=true
	call_deferred("run")
func check(value: bool, label: String) -> void:
	if not value: failed = true; printerr("FAIL: "+label)
func run() -> void:
	var description = JSON.parse_string(FileAccess.get_file_as_string(descriptor_path))
	var avatar = load("res://avatar_presenter.gd").from_descriptor(description)
	if avatar == null: printerr("Cannot load avatar"); quit(1); return
	root.add_child(avatar)
	if not legacy:
		check(avatar.supports_expression("happy"), "facial metadata and named targets loaded")
		if not avatar.supports_expression("happy"): quit(1); return
	var camera := Camera3D.new(); root.add_child(camera)
	camera.position = Vector3(0,0.80,0.80); camera.look_at(Vector3(0,0.80,0)); camera.fov = 27
	var light := DirectionalLight3D.new(); root.add_child(light)
	light.rotation_degrees = Vector3(-25,-20,0); light.light_energy = 0.75
	var world := WorldEnvironment.new(); world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color("dfe6ee")
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_color = Color.WHITE; world.environment.ambient_light_energy = 0.75
	root.add_child(world)
	if not output.is_empty(): DirAccess.make_dir_recursive_absolute(output)
	if legacy:
		check(not avatar.supports_expression("happy") and not avatar.set_expression("happy") and not avatar.set_mouth("open") and not avatar.react("startled"), "older packs retain a normal neutral avatar")
		await capture("neutral")
		avatar.clear_identity();print("LEGACY_FACE success=",not failed);quit(1 if failed else 0);return
	for name in avatar.facial_rig.expressions:
		check(avatar.set_expression(name,1,0), "expression "+name)
		var brows := 0
		for node in avatar.find_children("*", "MeshInstance3D", true, false):
			if "-brow" in str(node.name):
				brows+=1
				check(node.visible and node.mesh.get_aabb().size.x>0.08, "brows retained during "+name)
				check(node.mesh.surface_get_material(0).vertex_color_use_as_albedo, "brow contrast tint applied during "+name)
		check(brows==1, "selected eyebrow pair present during "+name)
		await capture(name)
	for name in ["cat_smug","content","love","shocked","squeezed"]:
		check(avatar.supports_expression(name), "cartoon pose available "+name)
		avatar.rotation_degrees.y=35
		avatar.set_expression(name,1,0)
		await capture(name+"-three-quarter")
	avatar.rotation_degrees.y=0
	avatar.set_expression("cat_smug",0.5,0)
	await capture("cat_smug-half")
	avatar.set_expression("love",1,0)
	avatar.set_expression("neutral",1,0.18)
	avatar._process(0.2)
	for control in ["eyes_closed","eyes_hearts","eyes_shocked","eyes_squeezed"]:
		check(avatar._face_values.get(control,0)==0, "cartoon geometry cleared by neutral "+control)
	check(not avatar.set_expression("unknown"), "unknown expression rejected")
	check(not avatar.set_expression("happy",NAN), "non-finite strength rejected")
	check(not avatar.react("unknown"), "unknown reaction rejected")
	var other = load("res://avatar_presenter.gd").from_descriptor(description)
	root.add_child(other); other.hide()
	avatar.set_expression("angry",1,0)
	for binding in other._face_bindings.mouth_flat:
		if not other._cpu_faces.has(binding[0].get_instance_id()):
			check(binding[0].get_blend_shape_value(binding[1])==0, "two instances do not share expression weights")
		else:
			var face = other._cpu_faces[binding[0].get_instance_id()]
			check(face.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]==face.surfaces[0].arrays[Mesh.ARRAY_VERTEX], "second CPU face remains neutral")
	other.clear_identity();other.queue_free()
	check(avatar.set_expression("sad",0.5,0), "base expression")
	check(avatar.react("startled",1,0.05,0), "transient reaction")
	avatar._process(0.1); avatar._process(0.3)
	check(absf(avatar._face_values.mouth_frown-0.5)<0.001, "reaction restores base strength")
	check(avatar.react("celebrate",1,1,0), "first reaction")
	check(avatar.react("hurt",0.8,0.03,0), "newest reaction replaces previous")
	avatar._process(0.1); avatar._process(0.3)
	check(absf(avatar._face_values.mouth_frown-0.5)<0.001, "no stale reaction timer")
	check(avatar.set_mouth("open",0.7,0), "mouth independent of brow emotion")
	check(absf(avatar._face_values.brow_worry-0.425)<0.001, "mouth retains brows")
	check(avatar._face_values.mouth_frown==0 and absf(avatar._face_values.mouth_open-0.7)<0.001, "one convex mouth pose")
	avatar.clear_mouth_override(0)
	check(absf(avatar._face_values.mouth_frown-0.5)<0.001, "mouth override released")
	avatar.reset_face(0)
	for value in avatar._face_values.values(): check(value==0, "neutral has no residual weights")
	check(avatar.set_expression("happy",1,0), "expression before body action")
	check(avatar.play_action("wave"), "body animation supported")
	avatar.animation_player.advance(0.25)
	check(avatar._face_values.mouth_smile==1, "body action does not overwrite face")
	avatar.rotation_degrees.y=35
	await capture("happy-three-quarter-wave")
	avatar.clear_identity()
	check(avatar._face_bindings.is_empty() and avatar._cpu_faces.is_empty() and avatar.facial_rig.is_empty() and not avatar.is_processing(), "revocation clears facial state")
	check(not avatar.react("celebrate"), "no reaction after revocation")
	print("FACIAL_RIG success=",not failed)
	quit(1 if failed else 0)
func capture(name: String) -> void:
	await process_frame
	await process_frame
	if not output.is_empty() and DisplayServer.get_name()!="headless":
		await RenderingServer.frame_post_draw
		check(root.get_texture().get_image().save_png(output.path_join(name+".png"))==OK, "capture "+name)
