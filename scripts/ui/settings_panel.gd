class_name SettingsPanelUI
extends Control

var _camera_mode_option: OptionButton
var _edge_pan_option: OptionButton
var _edge_speed_spinbox: SpinBox
var _smooth_pan_option: OptionButton
var _fullscreen_option: OptionButton
var _cast_mode_label: Label
var _cast_mode_option: OptionButton
var _cast_settings: CastSettings
var _built := false
var _cast_option_added := false


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	process_mode = Node.PROCESS_MODE_ALWAYS
	style.set_full_rect(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	var panel_bg := ColorRect.new()
	panel_bg.name = "PanelBg"
	panel_bg.anchor_left = 0.5
	panel_bg.anchor_top = 0.5
	panel_bg.anchor_right = 0.5
	panel_bg.anchor_bottom = 0.5
	panel_bg.offset_left = -200
	panel_bg.offset_top = -150
	panel_bg.offset_right = 200
	panel_bg.offset_bottom = 150
	panel_bg.scale = Vector2(1.8, 1.8)
	panel_bg.pivot_offset = Vector2(200, 150)
	panel_bg.color = style.PANEL_BACKGROUND
	panel_bg.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(panel_bg)

	var title := style.make_label("Settings", style.FONT_SEMIBOLD, 24)
	title.position = Vector2(153.5, 0)
	title.size = Vector2(93, 34)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	panel_bg.add_child(title)

	var config := VBoxContainer.new()
	config.name = "ConfigName"
	config.position = Vector2(89, 67)
	config.size = Vector2(222, 166)
	config.add_theme_constant_override("separation", 0)
	panel_bg.add_child(config)

	var camera_row := _make_option_row(
		config, "Camera", ["Locked", "Free"], style
	)
	_camera_mode_option = camera_row
	_camera_mode_option.select(GameSettings.camera_mode)
	_camera_mode_option.item_selected.connect(_on_camera_mode_selected)

	var edge_row := _make_option_row(config, "Edge Pan", ["Off", "On"], style)
	_edge_pan_option = edge_row
	_edge_pan_option.select(int(GameSettings.edge_pan))
	_edge_pan_option.item_selected.connect(_on_edge_pan_selected)

	var speed_row := _make_spin_row(config, "Edge Speed", style)
	_edge_speed_spinbox = speed_row
	_edge_speed_spinbox.value = GameSettings.edge_pan_speed
	_edge_speed_spinbox.value_changed.connect(_on_edge_speed_changed)

	var smooth_row := _make_option_row(
		config, "Smooth Pan", ["Off", "On"], style
	)
	_smooth_pan_option = smooth_row
	_smooth_pan_option.select(int(GameSettings.smooth_pan))
	_smooth_pan_option.item_selected.connect(_on_smooth_pan_selected)

	var fullscreen_row := _make_option_row(
		config, "Fullscreen", ["Windowed", "Borderless", "Exclusive"], style
	)
	_fullscreen_option = fullscreen_row
	_fullscreen_option.select(GameSettings.fullscreen)
	_fullscreen_option.item_selected.connect(_on_fullscreen_selected)

	_add_cast_mode_ui(config, style)

	var quit_button := Button.new()
	quit_button.name = "QuitButton"
	quit_button.position = Vector2(308, 269)
	quit_button.size = Vector2(92, 31)
	quit_button.text = "Quit Game"
	quit_button.add_theme_font_override("font", style.FONT_SEMIBOLD)
	quit_button.add_theme_font_size_override("font_size", 15)
	quit_button.pressed.connect(_on_quit_pressed)
	panel_bg.add_child(quit_button)

	var close_button := Button.new()
	close_button.name = "CloseButton"
	close_button.position = Vector2(360, 0)
	close_button.size = Vector2(40, 31)
	close_button.text = "×"
	close_button.add_theme_font_override("font", style.FONT_SEMIBOLD)
	close_button.add_theme_font_size_override("font_size", 20)
	close_button.pressed.connect(_on_close_pressed)
	panel_bg.add_child(close_button)

	visible = false


func _make_option_row(
	parent: VBoxContainer, text: String, items: Array[String], style: UIStyle
) -> OptionButton:
	var row := HBoxContainer.new()
	row.custom_minimum_size = Vector2(0, 30)
	parent.add_child(row)

	var label := style.make_label(text, style.FONT_SEMIBOLD, 15)
	label.custom_minimum_size = Vector2(160, 0)
	row.add_child(label)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)

	var option := OptionButton.new()
	option.custom_minimum_size = Vector2(80, 0)
	option.add_theme_font_override("font", style.FONT_SEMIBOLD)
	option.add_theme_font_size_override("font_size", 15)
	for i in items.size():
		option.add_item(items[i], i)
	row.add_child(option)
	return option


func _make_spin_row(
	parent: VBoxContainer, text: String, style: UIStyle
) -> SpinBox:
	var row := HBoxContainer.new()
	row.custom_minimum_size = Vector2(0, 30)
	parent.add_child(row)

	var label := style.make_label(text, style.FONT_SEMIBOLD, 15)
	label.custom_minimum_size = Vector2(160, 0)
	row.add_child(label)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)

	var spin_box := SpinBox.new()
	spin_box.custom_minimum_size = Vector2(80, 0)
	spin_box.add_theme_font_override("font", style.FONT_SEMIBOLD)
	spin_box.add_theme_font_size_override("font_size", 15)
	spin_box.min_value = 1.0
	spin_box.max_value = 50.0
	spin_box.step = 0.5
	row.add_child(spin_box)
	return spin_box


func _add_cast_mode_ui(parent: VBoxContainer, style: UIStyle) -> void:
	if _cast_option_added:
		return
	_cast_option_added = true
	var option := _make_option_row(
		parent, "Cast Mode", ["Normal", "Quick"], style
	)
	option.select(0)
	option.item_selected.connect(_on_cast_mode_selected)
	_cast_mode_label = option.get_parent().get_child(0) as Label
	_cast_mode_option = option


func bind_cast_settings(settings: CastSettings) -> void:
	_cast_settings = settings


func _unhandled_input(event: InputEvent) -> void:
	if not _built:
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		visible = not visible
		get_viewport().set_input_as_handled()


func _on_camera_mode_selected(index: int) -> void:
	GameSettings.camera_mode = index as GameSettings.CamMode


func _on_edge_pan_selected(index: int) -> void:
	GameSettings.edge_pan = index == 1


func _on_edge_speed_changed(value: float) -> void:
	GameSettings.edge_pan_speed = value


func _on_smooth_pan_selected(index: int) -> void:
	GameSettings.smooth_pan = index == 1


func _on_fullscreen_selected(index: int) -> void:
	GameSettings.fullscreen = index as GameSettings.FullscreenMode


func _on_cast_mode_selected(index: int) -> void:
	if not _cast_settings:
		return
	for i in 4:
		_cast_settings.skill_cast_mode[i] = index


func _on_quit_pressed() -> void:
	get_tree().quit()


func _on_close_pressed() -> void:
	visible = false
