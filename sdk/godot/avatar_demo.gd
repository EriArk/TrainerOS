# SPDX-License-Identifier: GPL-3.0-or-later
extends Node3D
## Runnable read-only integration: no art files and no wardrobe writes.
const Client = preload("avatar_client.gd")
const Presenter = preload("avatar_presenter.gd")
var client = Client.new()
var visual: Node3D
var fallback := MeshInstance3D.new()
var status := Label.new()
var actions := HBoxContainer.new()
var emotions := OptionButton.new()
var mouths := OptionButton.new()
var strength := HSlider.new()
var reactions := HBoxContainer.new()
var camera := Camera3D.new()

func _ready() -> void:
	add_child(camera)
	camera.position=Vector3(0,0.55,2.5);camera.fov=30;camera.near=0.01
	var environment := WorldEnvironment.new()
	environment.environment=Environment.new()
	environment.environment.background_mode=Environment.BG_COLOR
	environment.environment.background_color=Color("dfe6ee")
	environment.environment.ambient_light_source=Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color=Color.WHITE
	environment.environment.ambient_light_energy=0.65
	add_child(environment)
	var light := DirectionalLight3D.new()
	light.rotation_degrees=Vector3(-25,-25,0);light.light_energy=1.4
	add_child(light)
	var capsule := CapsuleMesh.new();capsule.radius=0.12;capsule.height=0.8
	fallback.mesh=capsule;fallback.position.y=0.4
	var material := StandardMaterial3D.new();material.albedo_color=Color("8ca49b")
	fallback.material_override=material;add_child(fallback)
	var layer := CanvasLayer.new();add_child(layer)
	var panel := VBoxContainer.new();layer.add_child(panel)
	var place_controls := func(): panel.position=Vector2(20,maxf(20,get_viewport().get_visible_rect().size.y-panel.size.y-20))
	panel.resized.connect(place_controls);get_viewport().size_changed.connect(place_controls)
	status.text="Connecting to TrainerOS…";panel.add_child(status)
	var reconnect := Button.new();reconnect.text="Connect to current Trainer"
	reconnect.pressed.connect(func(): _clear("Connecting…");client.connect_host())
	panel.add_child(reconnect);panel.add_child(actions)
	var face_row := HBoxContainer.new(); panel.add_child(face_row)
	face_row.add_child(emotions); face_row.add_child(mouths)
	var intensity := Label.new();intensity.text="Intensity";face_row.add_child(intensity)
	strength.min_value=0; strength.max_value=1; strength.step=0.05; strength.value=1
	strength.custom_minimum_size.x=120; face_row.add_child(strength)
	emotions.item_selected.connect(func(_index): _emotion())
	strength.value_changed.connect(func(_value): _emotion())
	mouths.item_selected.connect(func(index):
		if is_instance_valid(visual):
			if index==0: visual.clear_mouth_override()
			else: visual.set_mouth(mouths.get_item_text(index),strength.value))
	panel.add_child(reactions)
	for reaction in ["celebrate","disappointed","startled","hurt"]:
		var button := Button.new(); button.text=reaction; reactions.add_child(button)
		button.pressed.connect(func():
			if is_instance_valid(visual): visual.react(reaction,strength.value))
	var closeup := CheckButton.new(); closeup.text="Face close-up"; panel.add_child(closeup)
	closeup.toggled.connect(func(enabled):
		camera.position=Vector3(0,0.80,0.90) if enabled else Vector3(0,0.55,2.5))
	for action in ["wave","raise_arms","squat","step"]:
		var button := Button.new();button.text=action
		button.pressed.connect(func():
			if is_instance_valid(visual) and not visual.play_action(action): status.text="Action unavailable: "+action)
		actions.add_child(button)
	client.app_id="org.example.trainer-avatar"
	client.appearance_changed.connect(_appearance)
	client.unavailable.connect(_clear)
	add_child(client)
	_clear("Connecting to TrainerOS…")
	# Explicit local review mode uses the same hash-checked resolved descriptor.
	# It neither connects to a host nor writes a profile.
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--descriptor="):
			var description = JSON.parse_string(FileAccess.get_file_as_string(arg.trim_prefix("--descriptor=")))
			if description is Dictionary:
				_appearance(description)
				if is_instance_valid(visual): status.text="Local face review · "+str(description.appearance.eyes)
			else: _clear("Invalid local review descriptor")
			return
	client.connect_host()

func _emotion() -> void:
	if is_instance_valid(visual) and emotions.selected>=0:
		visual.set_expression(emotions.get_item_text(emotions.selected),strength.value)
		mouths.select(0)

func _appearance(descriptor: Dictionary) -> void:
	if is_instance_valid(visual) and visual.appearance_revision==descriptor.get("revision",""): return
	# In an actual game, queue the latest descriptor until an action/shot ends.
	var replacement = Presenter.from_descriptor(descriptor)
	if replacement==null:
		_clear("Avatar unavailable; using an anonymous placeholder.");return
	_clear("")
	visual=replacement;add_child(visual)
	fallback.hide();actions.show()
	emotions.clear();mouths.clear();mouths.add_item("Expression mouth")
	for name in visual.facial_rig.get("expressions",{}):
		emotions.add_item(name)
		if name=="neutral": emotions.select(emotions.item_count-1)
	for name in visual.facial_rig.get("mouths",[]): mouths.add_item(name)
	strength.set_value_no_signal(1)
	emotions.disabled=emotions.item_count==0;mouths.disabled=emotions.disabled
	strength.editable=not emotions.disabled;reactions.visible=not emotions.disabled
	status.text="Saved Trainer appearance · "+str(descriptor.appearance.eyes)

func _clear(reason: String) -> void:
	if is_instance_valid(visual):
		visual.clear_identity();visual.queue_free()
	visual=null;fallback.show();actions.hide()
	emotions.disabled=true;mouths.disabled=true;strength.editable=false;reactions.hide()
	status.text=reason
