class_name CommandBuilder
extends Node

signal move_issued(target: Vector2)

const SKILL_KEYS := [KEY_Q, KEY_W, KEY_E, KEY_R]
const ATTACK_KEY := KEY_A
const SKILL_UPGRADE_KEYS := [KEY_Q, KEY_W, KEY_E, KEY_R]
const MOVE_REPEAT_INTERVAL := 0.167
const MIN_MOVE_INTERVAL := 0.08

var _last_move_time := 0.0
var _prev_skill_held := [false, false, false, false]
var _prev_attack_held := false
var _prev_right := false
var _prev_s := false
var _prev_ctrl := false

var queue: InputEventQueue
var fsm: InputStateMachine
var buffer: CommandBuffer
var cast_settings: CastSettings

func setup(q: InputEventQueue, f: InputStateMachine, b: CommandBuffer, cs: CastSettings) -> void:
	queue = q
	fsm = f
	buffer = b
	cast_settings = cs

func process_frame() -> void:
	var events = queue.pop_all()
	if events.is_empty():
		_send_skill_aim_update()
		_process_held_move()
		return

	for ev in events:
		_process_event(ev)

	_process_held_move()

func _process_event(ev) -> void:
	var k = ev.key

	match ev.type:
		InputEventQueue.EType.KEY_PRESS:
			_on_key_press(k, ev)
		InputEventQueue.EType.KEY_RELEASE:
			_on_key_release(k, ev)
		InputEventQueue.EType.MB_PRESS:
			_on_mb_press(k, ev)
		InputEventQueue.EType.MB_RELEASE:
			_on_mb_release(k, ev)

func _on_key_press(k: int, ev) -> void:
	# Ctrl modifier check
	var ctrl: bool = ev.seq >= 0 and queue.is_held(KEY_CTRL)

	# SKILL_UPGRADE: Ctrl+Q/W/E/R
	if ctrl:
		for i in 4:
			if k == SKILL_UPGRADE_KEYS[i]:
				if not fsm.is_in_cast_lock():
					_make_upgrade(i)
				return

	# Skill keys Q/W/E/R
	for i in 4:
		if k == SKILL_KEYS[i]:
			_prev_skill_held[i] = true
			if fsm.is_in_cast_lock():
				return
			if cast_settings.is_quick(i):
				_make_skill(i, true, queue.mouse_world)
			else:
				fsm.command_axis = InputStateMachine.CommandAxis.SKILL_AIMING
				fsm.active_skill_slot = i
				_make_skill(i, false, queue.mouse_world)
			return

	# A key
	if k == ATTACK_KEY:
		_prev_attack_held = true
		if fsm.is_in_cast_lock():
			return
		if fsm.command_axis == InputStateMachine.CommandAxis.SKILL_AIMING:
			_make_cancel(0)
		fsm.command_axis = InputStateMachine.CommandAxis.ATTACK_AIMING
		return

	# S key
	if k == KEY_S:
		_prev_s = true
		match fsm.command_axis:
			InputStateMachine.CommandAxis.IDLE:
				_make_stop()
			InputStateMachine.CommandAxis.SKILL_AIMING:
				_make_cancel(0)
				fsm.command_axis = InputStateMachine.CommandAxis.IDLE
			InputStateMachine.CommandAxis.ATTACK_AIMING:
				_make_cancel(1)
				fsm.command_axis = InputStateMachine.CommandAxis.IDLE
			InputStateMachine.CommandAxis.CAST_LOCKED:
				_make_cancel(0)
		return

	# ESC
	if k == KEY_ESCAPE:
		if fsm.is_aiming():
			_make_cancel(0 if fsm.command_axis == InputStateMachine.CommandAxis.SKILL_AIMING else 1)
			fsm.command_axis = InputStateMachine.CommandAxis.IDLE
		return

	# H
	if k == KEY_H:
		if fsm.is_aiming():
			_make_cancel(0 if fsm.command_axis == InputStateMachine.CommandAxis.SKILL_AIMING else 1)
			fsm.command_axis = InputStateMachine.CommandAxis.IDLE
		elif fsm.is_in_cast_lock():
			_make_cancel(0)
		return

func _on_key_release(k: int, ev) -> void:
	for i in 4:
		if k == SKILL_KEYS[i]:
			_prev_skill_held[i] = false
			return
	if k == ATTACK_KEY:
		_prev_attack_held = false
	if k == KEY_S:
		_prev_s = false

func _on_mb_press(b: int, ev) -> void:
	var now := Time.get_ticks_msec() / 1000.0

	if b == MOUSE_BUTTON_LEFT:
		match fsm.command_axis:
			InputStateMachine.CommandAxis.SKILL_AIMING:
				_make_skill(fsm.active_skill_slot, true, queue.mouse_world)
				fsm.command_axis = InputStateMachine.CommandAxis.IDLE
			InputStateMachine.CommandAxis.ATTACK_AIMING:
				var hover_id = _get_hovered_enemy_id()
				if hover_id >= 0:
					_make_attack(hover_id)
				else:
					_make_attack_ground(queue.mouse_world)
				fsm.command_axis = InputStateMachine.CommandAxis.IDLE
		return

	if b == MOUSE_BUTTON_RIGHT:
		var right_edge := not _prev_right
		_prev_right = true

		if fsm.command_axis == InputStateMachine.CommandAxis.SKILL_AIMING:
			_make_cancel(0)
			fsm.command_axis = InputStateMachine.CommandAxis.IDLE
			return

		if fsm.command_axis == InputStateMachine.CommandAxis.ATTACK_AIMING:
			_make_cancel(1)
			fsm.command_axis = InputStateMachine.CommandAxis.IDLE
			return

		if fsm.is_in_cast_lock():
			_make_cancel(0)
			return

		var hover_id = _get_hovered_enemy_id()
		if hover_id >= 0:
			_make_attack(hover_id)
		elif right_edge:
			if now - _last_move_time >= MIN_MOVE_INTERVAL:
				_last_move_time = now
				_make_move(queue.mouse_world)

func _on_mb_release(b: int, ev) -> void:
	if b == MOUSE_BUTTON_RIGHT:
		_prev_right = false

func _send_skill_aim_update() -> void:
	if fsm.command_axis == InputStateMachine.CommandAxis.SKILL_AIMING:
		_make_skill(fsm.active_skill_slot, false, queue.mouse_world)

func _process_held_move() -> void:
	if not _prev_right:
		return
	if fsm.command_axis != InputStateMachine.CommandAxis.IDLE:
		return
	var hover_id = _get_hovered_enemy_id()
	if hover_id >= 0:
		return
	var now := Time.get_ticks_msec() / 1000.0
	if now - _last_move_time >= MOVE_REPEAT_INTERVAL:
		_last_move_time = now
		_make_move(queue.mouse_world)

func _get_hovered_enemy_id() -> int:
	var em = get_node_or_null("/root/Main/EntityManager")
	if em and em.has_method("get_hovered_id"):
		return em.get_hovered_id()
	return -1

# ── Command factory methods ──

func _make_move(target: Vector2) -> void:
	var c := Command.new()
	c.type = Command.CmdType.MOVE
	c.move_target = target
	buffer.push(c)
	move_issued.emit(target)

func _make_skill(slot: int, confirm: bool, aim: Vector2) -> void:
	var c := Command.new()
	c.type = Command.CmdType.SKILL
	c.skill_slot = slot
	c.skill_confirm = confirm
	c.skill_aim = aim
	c.skill_target_id = _get_hovered_enemy_id()
	buffer.push(c)

func _make_upgrade(slot: int) -> void:
	var c := Command.new()
	c.type = Command.CmdType.SKILL_UPGRADE
	c.skill_slot = slot
	buffer.push(c)

func _make_attack(target_id: int) -> void:
	var c := Command.new()
	c.type = Command.CmdType.ATTACK
	c.attack_target_id = target_id
	buffer.push(c)

func _make_attack_ground(pos: Vector2) -> void:
	var c := Command.new()
	c.type = Command.CmdType.ATTACK
	c.attack_ground = pos
	buffer.push(c)

func _make_cancel(scope: int) -> void:
	var c := Command.new()
	c.type = Command.CmdType.CANCEL
	c.cancel_scope = scope
	buffer.push(c)

func _make_stop() -> void:
	var c := Command.new()
	c.type = Command.CmdType.STOP
	buffer.push(c)
