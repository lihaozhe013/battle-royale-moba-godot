class_name CastSettings
extends Node

enum CastMode { NORMAL, QUICK }

# per-slot cast mode, default all NORMAL
var skill_cast_mode: Array[int] = [CastMode.NORMAL, CastMode.NORMAL, CastMode.NORMAL, CastMode.NORMAL]

func is_quick(slot: int) -> bool:
	if slot < 0 or slot >= 4:
		return false
	return skill_cast_mode[slot] == CastMode.QUICK
