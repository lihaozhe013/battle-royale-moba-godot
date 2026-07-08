extends CanvasLayer

@onready var _mode_option: OptionButton = $PanelBg/ConfigName/Mode/ModeOption
@onready var _camera_mode_option: OptionButton = $PanelBg/ConfigName/HBoxContainer2/CameraModeOption
@onready var _edge_pan_option: OptionButton = $PanelBg/ConfigName/HBoxContainer3/EdgePanOption
@onready var _edge_speed_spinbox: SpinBox = $PanelBg/ConfigName/HBoxContainer4/EdgeSpeedSpinBox
@onready var _smooth_pan_option: OptionButton = $PanelBg/ConfigName/HBoxContainer5/SmoothPanOption
@onready var _fullscreen_option: OptionButton = $PanelBg/ConfigName/HBoxContainer6/FullscreenOption


func _ready() -> void:
	visible = false

	_mode_option.add_item("WASD", 0)
	_mode_option.add_item("MOBA", 1)
	_mode_option.select(GameSettings.move_mode)

	_camera_mode_option.add_item("Locked", 0)
	_camera_mode_option.add_item("Free", 1)
	_camera_mode_option.select(GameSettings.camera_mode)

	_edge_pan_option.add_item("Off", 0)
	_edge_pan_option.add_item("On", 1)
	_edge_pan_option.select(int(GameSettings.edge_pan))

	_edge_speed_spinbox.value = GameSettings.edge_pan_speed

	_smooth_pan_option.add_item("Off", 0)
	_smooth_pan_option.add_item("On", 1)
	_smooth_pan_option.select(int(GameSettings.smooth_pan))

	_fullscreen_option.add_item("Windowed", 0)
	_fullscreen_option.add_item("Borderless", 1)
	_fullscreen_option.add_item("Exclusive", 2)
	_fullscreen_option.select(GameSettings.fullscreen)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		visible = not visible
		get_viewport().set_input_as_handled()


func _on_mode_selected(index: int) -> void:
	GameSettings.move_mode = index as GameSettings.MoveMode


func _on_camera_mode_selected(index: int) -> void:
	GameSettings.camera_mode = index as GameSettings.CamMode


func _on_edge_pan_selected(index: int) -> void:
	GameSettings.edge_pan = (index == 1)


func _on_edge_speed_changed(value: float) -> void:
	GameSettings.edge_pan_speed = value


func _on_smooth_pan_selected(index: int) -> void:
	GameSettings.smooth_pan = (index == 1)


func _on_fullscreen_selected(index: int) -> void:
	GameSettings.fullscreen = index as GameSettings.FullscreenMode


func _on_quit_pressed() -> void:
	get_tree().quit()


func _on_close_pressed() -> void:
	visible = false
