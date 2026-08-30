class_name InputStateMachine
extends Node

enum MoveAxis { NOT_MOVING, MOVING }
enum CommandAxis { IDLE, SKILL_AIMING, ATTACK_AIMING, CAST_LOCKED }

var move_axis: int = MoveAxis.NOT_MOVING
var command_axis: int = CommandAxis.IDLE

var active_skill_slot: int = -1
var skill_target_modes: Array[int] = [0, 0, 0, 0]
var skill_passive: Array[bool] = [false, false, false, false]


func sync_from_snapshot(p) -> void:
	move_axis = MoveAxis.MOVING if p.is_moving else MoveAxis.NOT_MOVING
	for i in skill_target_modes.size():
		if i >= p.skills.size():
			break
		var skill = p.skills[i]
		skill_target_modes[i] = int(skill.get("target_mode")) if skill.get("target_mode") != null else 0
		skill_passive[i] = bool(skill.get("is_passive")) if skill.get("is_passive") != null else false
	if p.cast_state != 0:
		if command_axis != CommandAxis.CAST_LOCKED:
			command_axis = CommandAxis.CAST_LOCKED
	else:
		if command_axis == CommandAxis.CAST_LOCKED:
			command_axis = CommandAxis.IDLE


func is_in_cast_lock() -> bool:
	return command_axis == CommandAxis.CAST_LOCKED


func is_cast_mode() -> bool:
	return command_axis in [CommandAxis.SKILL_AIMING, CommandAxis.CAST_LOCKED]


func is_targeting_mode() -> bool:
	return (
		command_axis
		in [
			CommandAxis.SKILL_AIMING,
			CommandAxis.ATTACK_AIMING,
			CommandAxis.CAST_LOCKED,
		]
	)


func is_idle() -> bool:
	return command_axis == CommandAxis.IDLE


func is_moving() -> bool:
	return move_axis == MoveAxis.MOVING


func is_aiming() -> bool:
	return command_axis in [CommandAxis.SKILL_AIMING, CommandAxis.ATTACK_AIMING]


func get_skill_target_mode(slot: int) -> int:
	return skill_target_modes[slot] if slot >= 0 and slot < skill_target_modes.size() else 0


func is_skill_passive(slot: int) -> bool:
	return skill_passive[slot] if slot >= 0 and slot < skill_passive.size() else false
