extends SceneTree
var descriptor_path := ""
var output := ""
var failed := false
func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--descriptor="): descriptor_path=arg.trim_prefix("--descriptor=")
		if arg.begins_with("--output="): output=arg.trim_prefix("--output=")
	call_deferred("run")
func run() -> void:
	if descriptor_path.is_empty() or not FileAccess.file_exists(descriptor_path):
		printerr("Supply --descriptor=PATH after --");quit(2);return
	var descriptor=JSON.parse_string(FileAccess.get_file_as_string(descriptor_path))
	if not descriptor is Dictionary:
		printerr("Invalid avatar descriptor");quit(2);return
	var script=load("res://avatar_presenter.gd")
	var avatar=script.from_descriptor(descriptor)
	if avatar==null: printerr("Avatar load failed");quit(1);return
	root.add_child(avatar)
	var camera:=Camera3D.new();root.add_child(camera);camera.position=Vector3(1.4,0.8,2.2);camera.look_at(Vector3(0,0.52,0));camera.fov=30
	var light:=DirectionalLight3D.new();root.add_child(light);light.rotation_degrees=Vector3(-30,-30,0);light.light_energy=1.4
	var world:=WorldEnvironment.new();world.environment=Environment.new();world.environment.background_mode=Environment.BG_COLOR;world.environment.background_color=Color("dfe6ee");world.environment.ambient_light_source=Environment.AMBIENT_SOURCE_COLOR;world.environment.ambient_light_color=Color.WHITE;world.environment.ambient_light_energy=.65;root.add_child(world)
	var count:=0
	for mesh in avatar.find_children("*","MeshInstance3D",true,false):
		count+=1
		if mesh.mesh==null: failed=true
	if count<3 or avatar.animation_player==null: failed=true
	for action in ["neutral","raise_arms","squat","step"]:
		if action!="neutral":
			if not avatar.play_action(action): failed=true
			# Disable crossfade only for deterministic inspection of each pose.
			avatar.animation_player.play(avatar.actions[action],0.0)
			avatar.animation_player.advance(0)
			var animation=avatar.animation_player.get_animation(avatar.actions[action])
			avatar.animation_player.seek(animation.length*0.5,true)
			avatar.animation_player.pause()
		await process_frame
		await process_frame
		if not output.is_empty():
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png(output.path_join(action+".png"))
	print("SHARED_AVATAR meshes=",count," profile=",avatar.profile_id," revision=",avatar.appearance_revision," success=",not failed)
	avatar.clear_identity()
	if avatar.get_child_count()!=0: failed=true
	quit(1 if failed else 0)
