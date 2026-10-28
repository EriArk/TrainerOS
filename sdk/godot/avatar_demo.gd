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

func _ready() -> void:
	var camera := Camera3D.new()
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
	var panel := VBoxContainer.new();panel.position=Vector2(20,20);layer.add_child(panel)
	status.text="Connecting to TrainerOS…";panel.add_child(status)
	var reconnect := Button.new();reconnect.text="Connect to current Trainer"
	reconnect.pressed.connect(func(): _clear("Connecting…");client.connect_host())
	panel.add_child(reconnect);panel.add_child(actions)
	for action in ["wave","raise_arms","squat","step"]:
		var button := Button.new();button.text=action
		button.pressed.connect(func():
			if is_instance_valid(visual) and not visual.play_action(action): status.text="Action unavailable: "+action)
		actions.add_child(button)
	client.app_id="org.example.trainer-avatar"
	client.appearance_changed.connect(_appearance)
	client.unavailable.connect(_clear)
	add_child(client)
	actions.hide();client.connect_host()

func _appearance(descriptor: Dictionary) -> void:
	if is_instance_valid(visual) and visual.appearance_revision==descriptor.get("revision",""): return
	# In an actual game, queue the latest descriptor until an action/shot ends.
	var replacement = Presenter.from_descriptor(descriptor)
	if replacement==null:
		_clear("Avatar unavailable; using an anonymous placeholder.");return
	_clear("")
	visual=replacement;add_child(visual)
	fallback.hide();actions.show()
	status.text="Saved Trainer appearance · "+str(descriptor.appearance.eyes)

func _clear(reason: String) -> void:
	if is_instance_valid(visual):
		visual.clear_identity();visual.queue_free()
	visual=null;fallback.show();actions.hide()
	status.text=reason
