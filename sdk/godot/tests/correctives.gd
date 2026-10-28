# SPDX-License-Identifier: GPL-3.0-or-later
extends SceneTree
## Runtime morph regression: real clip weights, fixed head vertices, skin and isolation.
var failed := false
func check(ok: bool, message: String) -> void:
	if not ok:failed=true;push_error(message)
func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty():quit(2);return
	var descriptor: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	var avatar := TrainerAvatarPresenter.from_descriptor(descriptor)
	var second := TrainerAvatarPresenter.from_descriptor(descriptor)
	check(avatar!=null and second!=null,"Both authenticated avatars load")
	if avatar==null or second==null:quit(1);return
	root.add_child(avatar);root.add_child(second)
	if avatar._cpu_correctives.is_empty():avatar._prepare_cpu_correctives(avatar.facial_rig.get("controls",[]),true)
	if second._cpu_correctives.is_empty():second._prepare_cpu_correctives(second.facial_rig.get("controls",[]),true)
	check(not avatar._cpu_correctives.is_empty(),"Body corrective evaluator exists")
	var observed := false
	var samples := 0
	for clip_name in avatar.animation_player.get_animation_list():
		if clip_name=="RESET":continue
		var clip := avatar.animation_player.get_animation(clip_name)
		for fraction in [0.0,0.25,0.5,0.9]:
			avatar.animation_player.play(clip_name,0)
			avatar.animation_player.seek(clip.length*fraction,true)
			avatar._update_cpu_correctives()
			for body in avatar._cpu_correctives.values():
				for weight in body.proxy.values:observed=observed or absf(weight)>0.001
				for surface in body.surfaces.size():
					var base: Array = body.surfaces[surface].arrays
					var current: Array = body.mesh.surface_get_arrays(surface)
					check(current[Mesh.ARRAY_BONES]==base[Mesh.ARRAY_BONES] and current[Mesh.ARRAY_WEIGHTS]==base[Mesh.ARRAY_WEIGHTS],"Correctives preserve skin binding")
					for i in base[Mesh.ARRAY_VERTEX].size():
						var position: Vector3 = current[Mesh.ARRAY_VERTEX][i]
						check(position.is_finite(),"Finite corrective vertex")
						if body.instance.name=="Trainer_Body" and base[Mesh.ARRAY_VERTEX][i].y>0.72:
							check(position.distance_to(base[Mesh.ARRAY_VERTEX][i])<0.00001,"Body action leaves head surface intact")
			for other in second._cpu_correctives.values():
				for weight in other.proxy.values:check(weight==0,"Independent instances do not share clip weights")
			samples+=1
	check(observed,"Animation tracks reach corrective weights")
	avatar.animation_player.stop()
	for body in avatar._cpu_correctives.values():
		body.proxy.values.fill(0);body.proxy.dirty=true
	avatar._update_cpu_correctives()
	for body in avatar._cpu_correctives.values():
		for i in body.surfaces.size():check(body.mesh.surface_get_arrays(i)[Mesh.ARRAY_VERTEX]==body.surfaces[i].arrays[Mesh.ARRAY_VERTEX],"Zero weights restore exact neutral surface")
	avatar.clear_identity();second.clear_identity()
	check(avatar._cpu_correctives.is_empty() and not RenderingServer.frame_pre_draw.is_connected(avatar._update_cpu_correctives),"Revocation disconnects corrective updates")
	print("AVATAR_CORRECTIVES success=",not failed," clip_samples=",samples)
	quit(1 if failed else 0)
