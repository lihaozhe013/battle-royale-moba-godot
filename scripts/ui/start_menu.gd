class_name StartMenu
extends Control

const HERO_SELECT_SCENE := "res://scenes/hero_select.tscn"
const ARTWORK_BREAKPOINT := 960.0
const LEFT_COLUMN_WIDTH := 360.0
const MENU_BUTTON_WIDTH := 280.0
const MENU_BUTTON_HEIGHT := 52.0

var _style: UIStyle
var _content_margins: MarginContainer
var _content_row: HBoxContainer
var _navigation_column: VBoxContainer
var _artwork_reserve: Control
var _background_art: TextureRect
var _settings_panel: SettingsPanelUI
var _start_button: Button
var _settings_button: Button
var _quit_button: Button
var _transitioning := false
var _built := false


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	MatchSetup.reset()
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_build()
	get_viewport().size_changed.connect(_on_viewport_size_changed)
	_update_layout()
	_start_button.call_deferred("grab_focus")
	DebugLogger.log(
		"[start_menu] ready viewport=%s" % get_viewport().get_visible_rect().size
	)


func _build() -> void:
	if _built:
		return
	_built = true
	_style = UIStyle.new()
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	var background := ColorRect.new()
	background.name = "Background"
	_style.set_full_rect(background)
	background.color = _style.MENU_BACKGROUND
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)

	_background_art = TextureRect.new()
	_background_art.name = "BackgroundArt"
	_style.set_full_rect(_background_art)
	_background_art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_background_art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
	_background_art.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_background_art.visible = false
	add_child(_background_art)

	var background_tint := ColorRect.new()
	background_tint.name = "BackgroundTint"
	_style.set_full_rect(background_tint)
	background_tint.color = Color(0.0, 0.0, 0.0, 0.45)
	background_tint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background_tint)

	_content_margins = MarginContainer.new()
	_content_margins.name = "ContentMargins"
	_style.set_full_rect(_content_margins)
	add_child(_content_margins)

	_content_row = HBoxContainer.new()
	_content_row.name = "ContentRow"
	_content_row.add_theme_constant_override("separation", 48)
	_content_margins.add_child(_content_row)

	_navigation_column = VBoxContainer.new()
	_navigation_column.name = "NavigationColumn"
	_navigation_column.custom_minimum_size = Vector2(LEFT_COLUMN_WIDTH, 0)
	_navigation_column.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_navigation_column.alignment = BoxContainer.ALIGNMENT_CENTER
	_content_row.add_child(_navigation_column)

	var eyebrow := _style.make_label(
		"BATTLE ROYALE", _style.FONT_SEMIBOLD, 16
	)
	eyebrow.custom_minimum_size = Vector2(LEFT_COLUMN_WIDTH, 24)
	eyebrow.add_theme_color_override("font_color", _style.MENU_ACCENT)
	_navigation_column.add_child(eyebrow)

	var title := _style.make_label("MOBA", _style.FONT_SEMIBOLD, 58)
	title.custom_minimum_size = Vector2(LEFT_COLUMN_WIDTH, 76)
	title.add_theme_color_override("font_color", _style.MENU_TEXT)
	_navigation_column.add_child(title)

	var subtitle := _style.make_label(
		"TACTICAL ARENA", _style.FONT_REGULAR, 14
	)
	subtitle.custom_minimum_size = Vector2(LEFT_COLUMN_WIDTH, 24)
	subtitle.add_theme_color_override("font_color", _style.MENU_MUTED_TEXT)
	_navigation_column.add_child(subtitle)

	var accent_line := ColorRect.new()
	accent_line.name = "AccentLine"
	accent_line.custom_minimum_size = Vector2(96, 2)
	accent_line.color = _style.MENU_ACCENT
	accent_line.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_navigation_column.add_child(accent_line)

	_add_gap(_navigation_column, 34)

	_start_button = _make_menu_button("START MATCH", true)
	_start_button.name = "StartButton"
	_start_button.pressed.connect(_on_start_pressed)
	_navigation_column.add_child(_start_button)

	_add_gap(_navigation_column, 10)

	_settings_button = _make_menu_button("SETTINGS", false)
	_settings_button.name = "SettingsButton"
	_settings_button.pressed.connect(_on_settings_pressed)
	_navigation_column.add_child(_settings_button)

	_add_gap(_navigation_column, 10)

	_quit_button = _make_menu_button("QUIT GAME", false)
	_quit_button.name = "QuitButton"
	_quit_button.pressed.connect(_on_quit_pressed)
	_navigation_column.add_child(_quit_button)

	_add_gap(_navigation_column, 24)

	var controls_hint := _style.make_label(
		"Right-click to move  •  Q / W / E / R to cast  •  A to attack",
		_style.FONT_REGULAR,
		12
	)
	controls_hint.custom_minimum_size = Vector2(LEFT_COLUMN_WIDTH, 38)
	controls_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	controls_hint.add_theme_color_override(
		"font_color", _style.MENU_MUTED_TEXT
	)
	_navigation_column.add_child(controls_hint)

	_artwork_reserve = Control.new()
	_artwork_reserve.name = "ArtworkReserve"
	_artwork_reserve.custom_minimum_size = Vector2(360, 0)
	_artwork_reserve.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_artwork_reserve.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_artwork_reserve.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_content_row.add_child(_artwork_reserve)

	_settings_panel = SettingsPanelUI.new()
	_settings_panel.name = "SettingsPanel"
	_settings_panel.build(_style, SettingsPanelUI.Context.MAIN_MENU)
	_settings_panel.closed.connect(_on_settings_closed)
	add_child(_settings_panel)


func _make_menu_button(text: String, primary: bool) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(
		MENU_BUTTON_WIDTH, MENU_BUTTON_HEIGHT
	)
	button.add_theme_font_override("font", _style.FONT_SEMIBOLD)
	button.add_theme_font_size_override("font_size", 17 if primary else 15)
	button.focus_mode = Control.FOCUS_ALL
	_style.apply_menu_button(button, primary)
	return button


func _add_gap(parent: Container, height: float) -> void:
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0, height)
	gap.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(gap)


func set_background_texture(texture: Texture2D) -> void:
	if not _background_art:
		return
	_background_art.texture = texture
	_background_art.visible = texture != null


func _on_start_pressed() -> void:
	if _transitioning:
		return
	_transitioning = true
	_start_button.disabled = true
	_settings_button.disabled = true
	_quit_button.disabled = true
	get_tree().paused = false
	DebugLogger.log("[start_menu] opening_hero_select scene=%s" % HERO_SELECT_SCENE)
	var error := get_tree().change_scene_to_file(HERO_SELECT_SCENE)
	if error != OK:
		_transitioning = false
		_start_button.disabled = false
		_settings_button.disabled = false
		_quit_button.disabled = false
		DebugLogger.log(
			"[ERROR] [start_menu] hero_select_scene_failed code=%d" % error
		)


func _on_settings_pressed() -> void:
	_settings_panel.open()


func _on_settings_closed() -> void:
	if not _transitioning:
		_settings_button.grab_focus()


func _on_quit_pressed() -> void:
	DebugLogger.log("[start_menu] quit_requested")
	get_tree().quit()


func _unhandled_input(event: InputEvent) -> void:
	if not _built or _transitioning:
		return
	if _settings_panel.visible:
		return
	if not event is InputEventKey:
		return
	if not event.pressed or event.echo:
		return
	if get_viewport().gui_get_focus_owner() != _start_button:
		return
	if (
		event.keycode != KEY_ENTER
		and event.keycode != KEY_KP_ENTER
		and event.keycode != KEY_SPACE
	):
		return
	_on_start_pressed()
	get_viewport().set_input_as_handled()


func _on_viewport_size_changed() -> void:
	_update_layout()


func _update_layout() -> void:
	if not _content_margins or not _artwork_reserve:
		return
	var viewport_size := get_viewport_rect().size
	var horizontal_margin := clampi(int(viewport_size.x * 0.08), 24, 96)
	var vertical_margin := clampi(int(viewport_size.y * 0.08), 24, 72)
	_content_margins.add_theme_constant_override(
		"margin_left", horizontal_margin
	)
	_content_margins.add_theme_constant_override(
		"margin_right", horizontal_margin
	)
	_content_margins.add_theme_constant_override("margin_top", vertical_margin)
	_content_margins.add_theme_constant_override(
		"margin_bottom", vertical_margin
	)

	var show_artwork := viewport_size.x >= ARTWORK_BREAKPOINT
	_artwork_reserve.visible = show_artwork
	_content_row.alignment = (
		BoxContainer.ALIGNMENT_BEGIN
		if show_artwork
		else BoxContainer.ALIGNMENT_CENTER
	)
