# SPDX-License-Identifier: GPL-3.0-or-later
extends SceneTree
var demo: Node3D
var received := 0
var revision := ""
func _initialize() -> void: call_deferred("run")
func run() -> void:
	demo=load("res://avatar_demo.tscn").instantiate();root.add_child(demo)
	demo.client.appearance_changed.connect(func(description: Dictionary):
		if demo.visual==null or demo.fallback.visible or description.profile!="alice": quit(1);return
		if not demo.visual.supports_expression("cat_smug") or demo.emotions.item_count!=14 or demo.mouths.item_count!=10: quit(5);return
		if demo.emotions.get_item_text(demo.emotions.selected)!="neutral": quit(7);return
		if not demo.visual.react("celebrate"): quit(6);return
		if received>0 and revision==description.revision: quit(2);return
		revision=description.revision;received+=1
		print("AVATAR_RECEIVED ",received))
	demo.client.unavailable.connect(func(reason: String):
		if received!=2 or reason!="Trainer identity ended" or demo.visual!=null or not demo.fallback.visible or not demo.emotions.disabled or demo.reactions.visible: quit(3);return
		print("AVATAR_REVOKED");quit(0))
	await create_timer(50).timeout
	quit(4)
