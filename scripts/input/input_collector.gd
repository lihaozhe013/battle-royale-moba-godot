extends Node

var move_input := Vector2.ZERO
var aim_world := Vector2.ZERO
var fire := false
var input_seq := 0

# 施法信号（帧脉冲，Sim 每 tick 消费）
var cast_slot := -1        # -1=无, 0-3=技能槽号（仅首次按下帧有效）
var cast_confirm := false  # 左键确认（仅按下帧有效）
var cast_cancel := false   # 右键取消（Aiming + Casting 都响应）
var cast_interrupt := false  # ESC/S/H 打断（仅 Casting 响应）
var cast_aim := Vector2.ZERO

var _prev_skill := [false, false, false, false]
const SKILL_KEYS := [KEY_C, KEY_E, KEY_R, KEY_F]


func _process(_delta: float) -> void:
	input_seq += 1

	_read_move()
	_read_aim()
	_read_skill_input()


func _read_move() -> void:
	var h := 0.0
	var v := 0.0
	if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP):   v += 1
	if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN): v -= 1
	if Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT): h += 1
	if Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT):h -= 1

	var raw := Vector2(h, v)
	move_input = raw.normalized() if raw.length_squared() > 1.0 else raw


func _read_aim() -> void:
	var cam := get_viewport().get_camera_3d()
	if cam:
		var mouse_pos := get_viewport().get_mouse_position()
		var from := cam.project_ray_origin(mouse_pos)
		var dir := cam.project_ray_normal(mouse_pos)
		if abs(dir.y) > 0.001:
			var t := -from.y / dir.y
			var hit := from + dir * t
			aim_world = Vector2(hit.x, hit.z)
	cast_aim = aim_world


func _read_skill_input() -> void:
	# 每帧先清脉冲信号（cast_slot 保留为按住持续）
	cast_confirm = false
	cast_cancel = false
	cast_interrupt = false

	# 1. 技能键按住持续设 cast_slot（松开后归 -1）
	var any_held := -1
	for i in 4:
		var pressed = Input.is_key_pressed(SKILL_KEYS[i])
		if pressed:
			any_held = i
		_prev_skill[i] = pressed
	cast_slot = any_held
	if cast_slot >= 0:
		cast_aim = aim_world

	# 2. 右键按住 = cast_cancel（Aiming+Casting 响应，持续有效防 Sim 丢帧）
	cast_cancel = Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)

	# 3. ESC/S/H 按住 = cast_interrupt（仅 Casting 响应，持续有效防 Sim 丢帧）
	cast_interrupt = Input.is_key_pressed(KEY_ESCAPE) || Input.is_key_pressed(KEY_H) || Input.is_key_pressed(KEY_S)

	# 4. 左键按住 = cast_confirm（Aiming 中=确认施法，非 Aiming=普攻）
	cast_confirm = Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	fire = cast_confirm
