class_name SettingsPanelUI
extends Control

enum Context { GAMEPLAY, MAIN_MENU }

signal main_menu_requested
signal opened
signal closed

const PANEL_WIDTH := 560.0
const PANEL_HEIGHT := 390.0
const LABEL_WIDTH := 230.0
const OPTION_WIDTH := 132.0
const ACTION_BUTTON_WIDTH := 122.0
const ACTION_BUTTON_HEIGHT := 38.0

var _camera_mode_option: OptionButton
var _edge_pan_option: OptionButton
var _edge_speed_spinbox: SpinBox
var _smooth_pan_option: OptionButton
var _fullscreen_option: OptionButton
var _cast_mode_option: OptionButton
var _cast_settings: CastSettings
var _style: UIStyle
var _context: Context = Context.GAMEPLAY
var _panel: PanelContainer
var _scrim: ColorRect
var _main_menu_button: Button
var _leave_match_dialog: ConfirmationDialog
var _built := false


func build(style: UIStyle, context: Context = Context.GAMEPLAY) -> void:
	if _built:
		return
	_built = true
	_style = style
	_context = context
	process_mode = Node.PROCESS_MODE_ALWAYS
	style.set_full_rect(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	_scrim = ColorRect.new()
	_scrim.name = "Scrim"
	style.set_full_rect(_scrim)
	_scrim.color = style.MENU_SCRIM
	_scrim.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(_scrim)

	_panel = PanelContainer.new()
	_panel.name = "Panel"
	_panel.anchor_left = 0.5
	_panel.anchor_top = 0.5
	_panel.anchor_right = 0.5
	_panel.anchor_bottom = 0.5
	_panel.offset_left = -PANEL_WIDTH * 0.5
	_panel.offset_top = -PANEL_HEIGHT * 0.5
	_panel.offset_right = PANEL_WIDTH * 0.5
	_panel.offset_bottom = PANEL_HEIGHT * 0.5
	_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_panel.grow_vertical = Control.GROW_DIRECTION_BOTH
	_panel.add_theme_stylebox_override("panel", style.menu_panel_style())
	add_child(_panel)

	var margin := MarginContainer.new()
	margin.name = "Margin"
	margin.add_theme_constant_override("margin_left", 32)
	margin.add_theme_constant_override("margin_top", 24)
	margin.add_theme_constant_override("margin_right", 32)
	margin.add_theme_constant_override("margin_bottom", 24)
	_panel.add_child(margin)

	var content := VBoxContainer.new()
	content.name = "Content"
	content.add_theme_constant_override("separation", 8)
	margin.add_child(content)

	var header := HBoxContainer.new()
	header.name = "Header"
	header.custom_minimum_size = Vector2(0, 38)
	content.add_child(header)

	var title := style.make_label("Settings", style.FONT_SEMIBOLD, 26)
	title.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	header.add_child(title)

	var header_spacer := Control.new()
	header_spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(header_spacer)

	var close_button := _make_action_button("×", style, false)
	close_button.name = "CloseButton"
	close_button.custom_minimum_size = Vector2(40, 36)
	close_button.add_theme_font_size_override("font_size", 22)
	close_button.pressed.connect(close)
	header.add_child(close_button)

	var separator := HSeparator.new()
	separator.name = "HeaderSeparator"
	content.add_child(separator)

	_camera_mode_option = _make_option_row(
		content, "Camera", ["Locked", "Free"], style
	)
	_camera_mode_option.item_selected.connect(_on_camera_mode_selected)

	_edge_pan_option = _make_option_row(
		content, "Edge Pan", ["Off", "On"], style
	)
	_edge_pan_option.item_selected.connect(_on_edge_pan_selected)

	_edge_speed_spinbox = _make_spin_row(content, "Edge Speed", style)
	_edge_speed_spinbox.value_changed.connect(_on_edge_speed_changed)

	_smooth_pan_option = _make_option_row(
		content, "Smooth Pan", ["Off", "On"], style
	)
	_smooth_pan_option.item_selected.connect(_on_smooth_pan_selected)

	_fullscreen_option = _make_option_row(
		content, "Fullscreen", ["Windowed", "Borderless", "Exclusive"], style
	)
	_fullscreen_option.item_selected.connect(_on_fullscreen_selected)

	_cast_mode_option = _make_option_row(
		content, "Cast Mode", ["Normal", "Quick"], style
	)
	_cast_mode_option.item_selected.connect(_on_cast_mode_selected)

	var action_separator := HSeparator.new()
	action_separator.name = "ActionSeparator"
	content.add_child(action_separator)

	var actions := HBoxContainer.new()
	actions.name = "Actions"
	actions.alignment = BoxContainer.ALIGNMENT_END
	actions.add_theme_constant_override("separation", 10)
	content.add_child(actions)

	var back_button := _make_action_button("Back", style, false)
	back_button.name = "BackButton"
	back_button.pressed.connect(close)
	actions.add_child(back_button)

	if _context == Context.GAMEPLAY:
		_main_menu_button = _make_action_button("Main Menu", style, false)
		_main_menu_button.name = "MainMenuButton"
		_main_menu_button.pressed.connect(_on_main_menu_pressed)
		actions.add_child(_main_menu_button)

		var quit_button := _make_action_button("Quit Game", style, false)
		quit_button.name = "QuitButton"
		quit_button.pressed.connect(_on_quit_pressed)
		actions.add_child(quit_button)

		_leave_match_dialog = ConfirmationDialog.new()
		_leave_match_dialog.name = "LeaveMatchDialog"
		_leave_match_dialog.title = "Leave Match?"
		_leave_match_dialog.dialog_text = (
			"Leave the current match and return to the start menu?"
		)
		_leave_match_dialog.ok_button_text = "Leave Match"
		_leave_match_dialog.cancel_button_text = "Cancel"
		_leave_match_dialog.process_mode = Node.PROCESS_MODE_ALWAYS
		_leave_match_dialog.add_theme_font_override("font", style.FONT_REGULAR)
		_leave_match_dialog.add_theme_font_size_override("font_size", 16)
		_leave_match_dialog.confirmed.connect(_on_leave_match_confirmed)
		_leave_match_dialog.close_requested.connect(_on_leave_match_dialog_closed)
		add_child(_leave_match_dialog)

	_sync_from_settings()
	visible = false


func _make_option_row(
	parent: VBoxContainer, text: String, items: Array[String], style: UIStyle
) -> OptionButton:
	var row := HBoxContainer.new()
	row.custom_minimum_size = Vector2(0, 32)
	parent.add_child(row)

	var label := style.make_label(text, style.FONT_SEMIBOLD, 15)
	label.custom_minimum_size = Vector2(LABEL_WIDTH, 0)
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	row.add_child(label)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)

	var option := OptionButton.new()
	option.custom_minimum_size = Vector2(OPTION_WIDTH, 0)
	option.add_theme_font_override("font", style.FONT_SEMIBOLD)
	option.add_theme_font_size_override("font_size", 15)
	option.focus_mode = Control.FOCUS_ALL
	for i in items.size():
		option.add_item(items[i], i)
	row.add_child(option)
	return option


func _make_spin_row(
	parent: VBoxContainer, text: String, style: UIStyle
) -> SpinBox:
	var row := HBoxContainer.new()
	row.custom_minimum_size = Vector2(0, 32)
	parent.add_child(row)

	var label := style.make_label(text, style.FONT_SEMIBOLD, 15)
	label.custom_minimum_size = Vector2(LABEL_WIDTH, 0)
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	row.add_child(label)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)

	var spin_box := SpinBox.new()
	spin_box.custom_minimum_size = Vector2(OPTION_WIDTH, 0)
	spin_box.add_theme_font_override("font", style.FONT_SEMIBOLD)
	spin_box.add_theme_font_size_override("font_size", 15)
	spin_box.min_value = 1.0
	spin_box.max_value = 50.0
	spin_box.step = 0.5
	spin_box.focus_mode = Control.FOCUS_ALL
	row.add_child(spin_box)
	return spin_box


func _make_action_button(text: String, style: UIStyle, primary: bool) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(
		ACTION_BUTTON_WIDTH, ACTION_BUTTON_HEIGHT
	)
	button.add_theme_font_override("font", style.FONT_SEMIBOLD)
	button.add_theme_font_size_override("font_size", 14)
	button.focus_mode = Control.FOCUS_ALL
	style.apply_menu_button(button, primary)
	return button


func bind_cast_settings(settings: CastSettings) -> void:
	_cast_settings = settings
	if _cast_settings:
		_cast_settings.sync_from_game_settings()
	_sync_from_settings()


func open() -> void:
	if not _built or visible:
		return
	_sync_from_settings()
	visible = true
	_camera_mode_option.call_deferred("grab_focus")
	opened.emit()


func close() -> void:
	if not _built or not visible:
		return
	if _leave_match_dialog and _leave_match_dialog.visible:
		_leave_match_dialog.hide()
	visible = false
	closed.emit()


func toggle() -> void:
	if visible:
		close()
	else:
		open()


func _sync_from_settings() -> void:
	if not _built:
		return
	_camera_mode_option.select(int(GameSettings.camera_mode))
	_edge_pan_option.select(int(GameSettings.edge_pan))
	_edge_speed_spinbox.value = GameSettings.edge_pan_speed
	_smooth_pan_option.select(int(GameSettings.smooth_pan))
	_fullscreen_option.select(int(GameSettings.fullscreen))
	_cast_mode_option.select(int(GameSettings.cast_mode))


func _unhandled_input(event: InputEvent) -> void:
	if not _built:
		return
	if not event is InputEventKey:
		return
	if not event.pressed or event.echo or event.keycode != KEY_ESCAPE:
		return
	if _leave_match_dialog and _leave_match_dialog.visible:
		return
	if visible:
		close()
	elif _context == Context.GAMEPLAY:
		open()
	else:
		return
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
	GameSettings.cast_mode = index as GameSettings.CastMode


func _on_main_menu_pressed() -> void:
	if _leave_match_dialog:
		_leave_match_dialog.popup_centered()


func _on_leave_match_confirmed() -> void:
	close()
	main_menu_requested.emit()


func _on_leave_match_dialog_closed() -> void:
	if visible and _main_menu_button:
		_main_menu_button.grab_focus()


func _on_quit_pressed() -> void:
	get_tree().quit()
