class_name CommandBuffer
extends Node

var _q: Array[Command] = []

func push(cmd: Command) -> void:
	_q.append(cmd)

func pop_all() -> Array[Command]:
	var result = _q.duplicate()
	_q.clear()
	return result

func empty() -> bool:
	return _q.is_empty()

func clear() -> void:
	_q.clear()

func merge_commands(cmds: Array[Command]) -> Array[Command]:
	var merged: Array[Command] = []
	var last_move: Command = null
	var last_skill: Command = null
	var last_attack: Command = null
	var has_stop := false
	var has_cancel_skill := false
	var has_cancel_attack := false
	var last_upgrade: Command = null

	for c in cmds:
		match c.type:
			Command.CmdType.MOVE:
				last_move = c
			Command.CmdType.SKILL:
				if last_skill != null and last_skill.skill_slot == c.skill_slot:
					if c.skill_confirm:
						last_skill = c
				else:
					last_skill = c
			Command.CmdType.SKILL_UPGRADE:
				last_upgrade = c
			Command.CmdType.ATTACK:
				last_attack = c
			Command.CmdType.CANCEL:
				if c.cancel_scope == 0:
					has_cancel_skill = true
				elif c.cancel_scope == 1:
					has_cancel_attack = true
				else:
					has_cancel_skill = true
					has_cancel_attack = true
			Command.CmdType.STOP:
				has_stop = true

	if last_move != null:
		merged.append(last_move)
	if last_skill != null:
		merged.append(last_skill)
	if last_upgrade != null:
		merged.append(last_upgrade)
	if last_attack != null:
		merged.append(last_attack)
	if has_stop:
		var s := Command.new()
		s.type = Command.CmdType.STOP
		merged.append(s)
	if has_cancel_skill or has_cancel_attack:
		var c := Command.new()
		c.type = Command.CmdType.CANCEL
		c.cancel_scope = (0 if has_cancel_skill else 1) if not (has_cancel_skill and has_cancel_attack) else 2
		merged.append(c)

	return merged
