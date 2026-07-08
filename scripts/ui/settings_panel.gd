extends CanvasLayer

@onready var _mode_option: OptionButton = $PanelBg/ModeOption


func _ready() -> void:
	visible = false
	_mode_option.add_item("WASD", 0)
	_mode_option.add_item("MOBA", 1)
	_mode_option.select(GameSettings.move_mode)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		visible = not visible
		get_viewport().set_input_as_handled()


func _on_mode_selected(index: int) -> void:
	GameSettings.move_mode = index


func _on_quit_pressed() -> void:
	get_tree().quit()


func _on_close_pressed() -> void:
	visible = false
