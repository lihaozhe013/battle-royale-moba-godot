extends Node

signal move_issued(target: Vector2)

var move_input := Vector2.ZERO
var aim_world := Vector2.ZERO
var fire := false
var input_seq := 0

# 施法信号（帧脉冲，Sim 每 tick 消费）
var cast_slot := -1        # -1=无, 0-3=技能槽号
var cast_confirm := false  # 左键确认/普攻（每帧持续）
var cast_cancel := false   # 右键取消（每帧持续）
var cast_interrupt := false  # H/S 打断（每帧持续）
var cast_aim := Vector2.ZERO

# 移动指令（MOBA 模式：右键点地板）
var move_cmd_target := Vector2.ZERO
var move_cmd_issue := false  # 边沿脉冲（仅按下帧为 true）
var stop := false            # S 键停止（边沿脉冲）

var _prev_skill := [false, false, false, false]
var _prev_right := false
var _prev_s := false

# 右键节流
const MIN_MOVE_INTERVAL := 0.08
const MOVE_REPEAT_INTERVAL := 0.167  # 长按重复 ~6 次/秒
var _last_move_time := 0.0

# 模式相关键位表
var _skill_keys := [KEY_C, KEY_E, KEY_R, KEY_F]
const SKILL_KEYS_WASD := [KEY_C, KEY_E, KEY_R, KEY_F]
const SKILL_KEYS_MOBA := [KEY_Q, KEY_W, KEY_E, KEY_R]


func _ready() -> void:
	GameSettings.mode_changed.connect(_on_mode_changed)
	# 同步初始状态（GameSettings 的 _ready 在我们之前触发过 signal）
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
	# 每帧先清持续态脉冲（held 重读），边沿脉冲由 sim_bridge 消费后清
	cast_confirm = false
	cast_interrupt = false

	# 1. 技能键按住持续设 cast_slot（松开后归 -1）
	var any_held := -1
	for i in 4:
		var pressed = Input.is_key_pressed(_skill_keys[i])
		if pressed:
			any_held = i
		_prev_skill[i] = pressed
	cast_slot = any_held
	if cast_slot >= 0:
		cast_aim = aim_world

	# 2. 右键（边沿触发：按下帧才 cancel，持续压制会打断移动时的施法）
	var right_now := Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)
	cast_cancel = right_now and not _prev_right

	if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
		# MOBA 模式：右键边沿 + 长按连点
		var now := Time.get_ticks_msec() / 1000.0
		var should_issue := false
		if right_now and not _prev_right:
			should_issue = now - _last_move_time >= MIN_MOVE_INTERVAL
		elif right_now and _prev_right:
			should_issue = now - _last_move_time >= MOVE_REPEAT_INTERVAL
		if should_issue:
			_last_move_time = now
			move_cmd_target = aim_world
			move_cmd_issue = true
			move_issued.emit(move_cmd_target)
	_prev_right = right_now

	# 3. 打断键（H 通用，MOBA 模式增加 S）
	if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
		cast_interrupt = Input.is_key_pressed(KEY_H) or Input.is_key_pressed(KEY_S)
	else:
		cast_interrupt = Input.is_key_pressed(KEY_H)

	# 4. MOBA 模式：S 键边沿 = stop 脉冲（同时已作为打断键持续压制）
	if GameSettings.move_mode == GameSettings.MoveMode.MOBA:
		var s_now := Input.is_key_pressed(KEY_S)
		if s_now and not _prev_s:
			stop = true
		_prev_s = s_now

	# 5. 左键 = 确认施法 / 普攻
	cast_confirm = Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	fire = cast_confirm
