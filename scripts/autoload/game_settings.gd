extends Node

enum CamMode { LOCKED, FREE }
enum FullscreenMode { WINDOWED, BORDERLESS, EXCLUSIVE }

signal camera_mode_changed(m: CamMode)
signal edge_pan_changed(on: bool)
signal edge_pan_speed_changed(v: float)
signal smooth_pan_changed(on: bool)
signal fullscreen_changed(m: FullscreenMode)

var camera_mode: CamMode = CamMode.FREE:
	set(value):
		if camera_mode == value:
			return
		camera_mode = value
		camera_mode_changed.emit(camera_mode)
		_save()

var edge_pan: bool = true:
	set(value):
		if edge_pan == value:
			return
		edge_pan = value
		edge_pan_changed.emit(edge_pan)
		_save()

var edge_pan_speed: float = 25.0:
	set(value):
		if edge_pan_speed == value:
			return
		edge_pan_speed = value
		edge_pan_speed_changed.emit(edge_pan_speed)
		_save()

var smooth_pan: bool = false:
	set(value):
		if smooth_pan == value:
			return
		smooth_pan = value
		smooth_pan_changed.emit(smooth_pan)
		_save()

var fullscreen: FullscreenMode = FullscreenMode.EXCLUSIVE:
	set(value):
		if fullscreen == value:
			return
		fullscreen = value
		fullscreen_changed.emit(fullscreen)
		_save()
		_apply_fullscreen()

const CFG_PATH := "user://settings.cfg"
const CFG_SECTION_CTRL := "controls"
const CFG_SECTION_DISP := "display"


func _ready() -> void:
	_load()
	_apply_fullscreen()


func _load() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(CFG_PATH) == OK:
		camera_mode   = cfg.get_value(CFG_SECTION_CTRL, "camera_mode", int(CamMode.LOCKED)) as CamMode
		edge_pan      = bool(cfg.get_value(CFG_SECTION_CTRL, "edge_pan", false))
		edge_pan_speed = float(cfg.get_value(CFG_SECTION_CTRL, "edge_pan_speed", 14.0))
		smooth_pan    = bool(cfg.get_value(CFG_SECTION_CTRL, "smooth_pan", true))
		fullscreen    = cfg.get_value(CFG_SECTION_DISP, "fullscreen", int(FullscreenMode.WINDOWED)) as FullscreenMode


func _save() -> void:
	var cfg := ConfigFile.new()
	cfg.set_value(CFG_SECTION_CTRL, "camera_mode", int(camera_mode))
	cfg.set_value(CFG_SECTION_CTRL, "edge_pan", edge_pan)
	cfg.set_value(CFG_SECTION_CTRL, "edge_pan_speed", edge_pan_speed)
	cfg.set_value(CFG_SECTION_CTRL, "smooth_pan", smooth_pan)
	cfg.set_value(CFG_SECTION_DISP, "fullscreen", int(fullscreen))
	cfg.save(CFG_PATH)


func _apply_fullscreen() -> void:
	var ds := DisplayServer
	match fullscreen:
		FullscreenMode.WINDOWED:
			ds.window_set_mode(ds.WINDOW_MODE_WINDOWED)
			ds.window_set_flag(ds.WINDOW_FLAG_BORDERLESS, false)
		FullscreenMode.BORDERLESS:
			ds.window_set_mode(ds.WINDOW_MODE_FULLSCREEN)
			ds.window_set_flag(ds.WINDOW_FLAG_BORDERLESS, true)
		FullscreenMode.EXCLUSIVE:
			ds.window_set_mode(ds.WINDOW_MODE_EXCLUSIVE_FULLSCREEN)
			ds.window_set_flag(ds.WINDOW_FLAG_BORDERLESS, false)
