extends SceneTree
var received := 0
var revision := ""
var client: Node
var visual: Node3D
func _initialize() -> void:
	call_deferred("run")
func run() -> void:
	client=load("res://avatar_client.gd").new()
	root.add_child(client)
	client.app_id="org.test.godot.avatar"
	client.appearance_changed.connect(func(value: Dictionary):
		var next=load("res://avatar_presenter.gd").from_descriptor(value)
		if next==null or value.profile!="alice": quit(1);return
		if visual!=null: visual.clear_identity();visual.queue_free()
		visual=next;root.add_child(visual)
		if received>0 and revision==value.revision: quit(2);return
		revision=value.revision;received+=1
		print("AVATAR_RECEIVED ",received))
	client.unavailable.connect(func(reason: String):
		if visual!=null: visual.clear_identity()
		if received==2 and reason=="Trainer identity ended" and visual.get_child_count()==0:
			print("AVATAR_REVOKED");quit(0)
		else: printerr(reason);quit(3))
	client.connect_host()
	await create_timer(30).timeout
	quit(4)
