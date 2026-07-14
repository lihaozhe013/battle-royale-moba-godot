class_name InputEventQueue
extends Node

enum EType { KEY_PRESS, KEY_RELEASE, MB_PRESS, MB_RELEASE, MOUSE_MOVE }

class Ev:
	var type: int
	var key: int
	var pos: Vector2
	var t: float
	var seq: int

	func _init(p_type: int, p_key: int, p_pos: Vector2, p_t: float, p_seq: int):
		type = p_type
		key = p_key
		pos = p_pos
		t = p_t
		seq = p_seq

var mouse_world := Vector2.ZERO
var held_keys: Dictionary = {}
var held_mouse: Dictionary = {}

var _queue: Array[Ev] = []
var _seq := 0


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var cam := get_viewport().get_camera_3d()
		if cam:
			var from := cam.project_ray_origin(event.position)
			var dir := cam.project_ray_normal(event.position)
			if abs(dir.y) > 0.001:
				var t := -from.y / dir.y
				mouse_world = Vector2(from.x + dir.x * t, from.z + dir.z * t)
		return

	if event is InputEventKey and event.pressed and not event.echo:
		push_key_press(event.keycode)
	elif event is InputEventKey and not event.pressed:
		push_key_release(event.keycode)
	elif event is InputEventMouseButton:
		var cam := get_viewport().get_camera_3d()
		if cam:
			var from := cam.project_ray_origin(event.position)
			var dir := cam.project_ray_normal(event.position)
			if abs(dir.y) > 0.001:
				var t := -from.y / dir.y
				mouse_world = Vector2(from.x + dir.x * t, from.z + dir.z * t)
		if event.pressed:
			push_mb_press(event.button_index)
		else:
			push_mb_release(event.button_index)

func push_key_press(k: int) -> void:
	held_keys[k] = true
	_seq += 1
	_queue.append(Ev.new(EType.KEY_PRESS, k, mouse_world, Time.get_ticks_msec() / 1000.0, _seq))

func push_key_release(k: int) -> void:
	held_keys.erase(k)
	_seq += 1
	_queue.append(Ev.new(EType.KEY_RELEASE, k, mouse_world, Time.get_ticks_msec() / 1000.0, _seq))

func push_mb_press(b: int) -> void:
	held_mouse[b] = true
	_seq += 1
	_queue.append(Ev.new(EType.MB_PRESS, b, mouse_world, Time.get_ticks_msec() / 1000.0, _seq))

func push_mb_release(b: int) -> void:
	held_mouse.erase(b)
	_seq += 1
	_queue.append(Ev.new(EType.MB_RELEASE, b, mouse_world, Time.get_ticks_msec() / 1000.0, _seq))

func push_mouse_move(pos: Vector2) -> void:
	mouse_world = pos
	_seq += 1
	_queue.append(Ev.new(EType.MOUSE_MOVE, 0, pos, Time.get_ticks_msec() / 1000.0, _seq))

func pop_all() -> Array[Ev]:
	var result = _queue.duplicate()
	_queue.clear()
	return result

func peek_all() -> Array[Ev]:
	return _queue.duplicate()

func is_held(k: int) -> bool:
	return held_keys.has(k) or Input.is_key_pressed(k)

func is_mb_held(b: int) -> bool:
	return held_mouse.has(b) or Input.is_mouse_button_pressed(b)

func calibrate() -> void:
	for k in held_keys.keys():
		if not Input.is_key_pressed(k):
			held_keys.erase(k)
	for b in held_mouse.keys():
		if not Input.is_mouse_button_pressed(b):
			held_mouse.erase(b)
