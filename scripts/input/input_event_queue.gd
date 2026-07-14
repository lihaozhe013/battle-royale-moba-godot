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
