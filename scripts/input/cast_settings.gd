class_name CastSettings
extends Node

enum CastMode { NORMAL, QUICK }

# per-slot cast mode, default all NORMAL
var skill_cast_mode: Array[int] = [
	CastMode.NORMAL, CastMode.NORMAL, CastMode.NORMAL, CastMode.NORMAL
]


func _ready() -> void:
	GameSettings.cast_mode_changed.connect(_on_game_settings_cast_mode_changed)
	sync_from_game_settings()


func sync_from_game_settings() -> void:
	set_all_skill_cast_mode(int(GameSettings.cast_mode))


func set_all_skill_cast_mode(mode: int) -> void:
	for i in skill_cast_mode.size():
		skill_cast_mode[i] = mode


func _on_game_settings_cast_mode_changed(mode: int) -> void:
	set_all_skill_cast_mode(mode)


func is_quick(slot: int) -> bool:
	if slot < 0 or slot >= 4:
		return false
	return skill_cast_mode[slot] == CastMode.QUICK
