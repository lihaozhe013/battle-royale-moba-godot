extends Node

signal move_issued(target: Vector2)

var move_input := Vector2.ZERO
var aim_world := Vector2.ZERO
var fire := false
var input_seq := 0

var cast_slot := -1
var cast_confirm := false
var cast_cancel := false
var cast_interrupt := false
var cast_aim := Vector2.ZERO
var cast_target_id := -1
var hovered_entity_id := -1

var move_cmd_target := Vector2.ZERO
var move_cmd_issue := false
var stop := false

var attack_target_id := -1

var _prev_skill := [false, false, false, false, false]  # Q,W,E,R,A
var _prev_right := false
var _prev_s := false

const MIN_MOVE_INTERVAL := 0.08
const MOVE_REPEAT_INTERVAL := 0.167
var _last_move_time := 0.0

const SKILL_KEYS := [KEY_Q, KEY_W, KEY_E, KEY_R]
const ATTACK_KEY := KEY_A
const ALL_KEYS := [KEY_Q, KEY_W, KEY_E, KEY_R, KEY_A]


func _process(_delta: float) -> void:
	input_seq += 1
	_read_move()
	_read_aim()
	_read_skill_input()


func _read_move() -> void:
	move_input = Vector2.ZERO


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
	cast_confirm = false
	cast_interrupt = false

	# 1. 检测技能键（Q/W/E/R）和攻击键（A）的 held
	var any_held := -1
	for i in 5:
		var pressed = Input.is_key_pressed(ALL_KEYS[i])
		if pressed:
			any_held = i
		_prev_skill[i] = pressed

	if any_held >= 0:
		cast_slot = any_held
		cast_target_id = hovered_entity_id if any_held < 4 else -1
		cast_aim = aim_world
	else:
		cast_slot = -1

	# 2. 左键 = 技能确认（Aiming 中）或普攻（非 Aiming 中）
	var left_now := Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	if cast_slot >= 0 and left_now:
		cast_confirm = true
		fire = false
	elif left_now:
		fire = true
	else:
		cast_confirm = false
		fire = false

	# 3. 右键处理（先检测边沿，再更新 _prev_right）
	var right_now := Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)
	var right_edge := right_now and not _prev_right

	if right_edge:
		cast_cancel = true  # Sim 仅在 Aiming/Casting 时消费
		if hovered_entity_id >= 0:
			# 右键点敌人 → 直接攻击
			attack_target_id = hovered_entity_id
		else:
			# 右键点空地 → 移动
			var now := Time.get_ticks_msec() / 1000.0
			if now - _last_move_time >= MIN_MOVE_INTERVAL:
				_last_move_time = now
				move_cmd_target = aim_world
				move_cmd_issue = true
				move_issued.emit(move_cmd_target)
	elif right_now and hovered_entity_id < 0:
		# 右键长按连点（空地）
		var now := Time.get_ticks_msec() / 1000.0
		if now - _last_move_time >= MOVE_REPEAT_INTERVAL:
			_last_move_time = now
			move_cmd_target = aim_world
			move_cmd_issue = true
			move_issued.emit(move_cmd_target)

	_prev_right = right_now

	# 4. 打断键
	cast_interrupt = Input.is_key_pressed(KEY_H) or Input.is_key_pressed(KEY_S)

	# 5. S 键 stop 脉冲
	var s_now := Input.is_key_pressed(KEY_S)
	if s_now and not _prev_s:
		stop = true
	_prev_s = s_now
