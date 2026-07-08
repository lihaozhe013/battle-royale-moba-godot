extends Node

enum MoveMode { WASD, MOBA }
var move_mode: MoveMode = MoveMode.WASD:
	set(value):
		if move_mode == value:
			return
		move_mode = value
		mode_changed.emit(move_mode)
		_save()

signal mode_changed(m: MoveMode)

const CFG_PATH := "user://settings.cfg"
const CFG_SECTION := "controls"
const CFG_KEY := "move_mode"


func _ready() -> void:
	_load()


func _load() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(CFG_PATH) == OK:
		var val = cfg.get_value(CFG_SECTION, CFG_KEY, int(MoveMode.WASD))
		move_mode = val as MoveMode


func _save() -> void:
	var cfg := ConfigFile.new()
	cfg.set_value(CFG_SECTION, CFG_KEY, int(move_mode))
	cfg.save(CFG_PATH)
