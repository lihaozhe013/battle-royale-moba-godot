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

var attack_command_mode := false
var attack_target_id := -1
var attack_ground := false
var attack_ground_pos := Vector2.ZERO
var attack_clear := false

var _prev_skill := [false, false, false, false]
var _prev_right := false
var _prev_s := false
var _prev_attack := false

const MIN_MOVE_INTERVAL := 0.08
const MOVE_REPEAT_INTERVAL := 0.167
var _last_move_time := 0.0

var _skill_keys := [KEY_C, KEY_E, KEY_R, KEY_F]
var _attack_key := ATTACK_KEY_WASD
const SKILL_KEYS_WASD := [KEY_C, KEY_E, KEY_R, KEY_F]
const SKILL_KEYS_MOBA := [KEY_Q, KEY_W, KEY_E, KEY_R]
const ATTACK_KEY_WASD := KEY_Q
const ATTACK_KEY_MOBA := KEY_A


func _ready() -> void:
	GameSettings.mode_changed.connect(_on_mode_changed)
	_on_mode_changed(GameSettings.move_mode)


func _on_mode_changed(m: int) -> void:
	if m == GameSettings.MoveMode.MOBA:
		_skill_keys = SKILL_KEYS_MOBA
	else:
		_skill_keys = SKILL_KEYS_WASD


func _process(_delta: float) -> void:
	input_seq += 1
	_read_move()
	_read_aim()
	_read_skill_input()


func _read_move() -> void:
	if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
		move_input = Vector2.ZERO
		return

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
	cast_confirm = false
	cast_interrupt = false

	# 1. 技能键 held → cast_slot
	var any_held := -1
	for i in 4:
		var pressed = Input.is_key_pressed(_skill_keys[i])
		if pressed:
			any_held = i
		_prev_skill[i] = pressed
	cast_slot = any_held
	cast_target_id = hovered_entity_id if any_held >= 0 else -1
	if cast_slot >= 0:
		cast_aim = aim_world
		attack_command_mode = false

	# 2. 攻击键边沿 → attack_command_mode
	var attack_now := Input.is_key_pressed(_attack_key)
	if attack_now and not _prev_attack:
		print("[ATK] attack_command_mode=true key=", _attack_key)
		attack_command_mode = true
	_prev_attack = attack_now

	# 3. 取消 attack_command_mode
	if attack_command_mode:
		if Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) \
		   or Input.is_key_pressed(KEY_ESCAPE) \
		   or Input.is_key_pressed(KEY_S) \
		   or Input.is_key_pressed(KEY_H):
			attack_command_mode = false

	# 4. 左键 = 确认
	var left_now := Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	if attack_command_mode and left_now:
		if hovered_entity_id >= 0:
			attack_target_id = hovered_entity_id
			print("[ATK] A+左键点 target=", hovered_entity_id)
		else:
			attack_ground = true
			attack_ground_pos = aim_world
			print("[ATK] A+左键点地板 pos=", aim_world)
		attack_command_mode = false
		cast_confirm = false
		fire = false
	elif cast_slot >= 0 and left_now:
		cast_confirm = true
		fire = false
	else:
		cast_confirm = false
		fire = false

	# 5. 右键处理
	var right_now := Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)
	var right_edge := right_now and not _prev_right

	if right_edge:
		cast_cancel = true

	if cast_slot < 0:
		if hovered_entity_id >= 0 and right_edge:
			attack_target_id = hovered_entity_id
			print("[ATK] 右键点 target=", hovered_entity_id)
		elif hovered_entity_id < 0 and GameSettings.move_mode == GameSettings.MoveMode.MOBA:
			var now := Time.get_ticks_msec() / 1000.0
			var should_issue := false
			if right_edge:
				should_issue = now - _last_move_time >= MIN_MOVE_INTERVAL
			elif right_now:
				should_issue = now - _last_move_time >= MOVE_REPEAT_INTERVAL
			if should_issue:
				_last_move_time = now
				move_cmd_target = aim_world
				move_cmd_issue = true
				move_issued.emit(move_cmd_target)
				attack_clear = true
	_prev_right = right_now

	# 6. 打断键
	if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
		cast_interrupt = Input.is_key_pressed(KEY_H) or Input.is_key_pressed(KEY_S)
	else:
		cast_interrupt = Input.is_key_pressed(KEY_H)

	# 7. S 键 stop + 清锁
	if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
		var s_now := Input.is_key_pressed(KEY_S)
		if s_now and not _prev_s:
			stop = true
			attack_clear = true
		_prev_s = s_now
