class_name Command
extends RefCounted

enum CmdType { MOVE, SKILL, SKILL_UPGRADE, ATTACK, CANCEL, STOP }

var type: int
# MOVE
var move_target: Vector2
# SKILL / SKILL_UPGRADE
var skill_slot: int = -1
# 0-3 = Q/W/E/R 技能槽
# 预留扩展：10-15 = 装备主动技能槽（P1-8）
var skill_confirm: bool
var skill_aim: Vector2
var skill_target_id: int = -1
# ATTACK
var attack_target_id: int = -1
var attack_ground: Vector2
# CANCEL
var cancel_scope: int

func get_type_name() -> String:
	match type:
		CmdType.MOVE: return "MOVE"
		CmdType.SKILL: return "SKILL"
		CmdType.SKILL_UPGRADE: return "SKILL_UPGRADE"
		CmdType.ATTACK: return "ATTACK"
		CmdType.CANCEL: return "CANCEL"
		CmdType.STOP: return "STOP"
	return "UNKNOWN"
