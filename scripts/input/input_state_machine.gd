class_name InputStateMachine
extends Node

enum MoveAxis { NOT_MOVING, MOVING }
enum CommandAxis { IDLE, SKILL_AIMING, ATTACK_AIMING, CAST_LOCKED }

var move_axis: int = MoveAxis.NOT_MOVING
var command_axis: int = CommandAxis.IDLE

var active_skill_slot: int = -1

func sync_from_snapshot(p) -> void:
	move_axis = MoveAxis.MOVING if p.is_moving else MoveAxis.NOT_MOVING
	if p.cast_state != 0:
		if command_axis != CommandAxis.CAST_LOCKED:
			command_axis = CommandAxis.CAST_LOCKED
	else:
		if command_axis == CommandAxis.CAST_LOCKED:
			command_axis = CommandAxis.IDLE

func is_in_cast_lock() -> bool:
	return command_axis == CommandAxis.CAST_LOCKED

func is_idle() -> bool:
	return command_axis == CommandAxis.IDLE

func is_moving() -> bool:
	return move_axis == MoveAxis.MOVING

func is_aiming() -> bool:
	return command_axis in [CommandAxis.SKILL_AIMING, CommandAxis.ATTACK_AIMING]
