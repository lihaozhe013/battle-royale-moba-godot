class_name HeroSelect
extends Control

const GAME_SCENE := "res://scenes/main.tscn"
const START_MENU_SCENE := "res://scenes/start_menu.tscn"
const SKILL_KEYS := ["Q", "W", "E", "R"]

var _style: UIStyle
var _catalog: Array = []
var _selected_index := 0
var _start_button: Button
var _back_button: Button
var _status_label: Label
var _selection_label: Label
var _cards: Array[PanelContainer] = []
var _transitioning := false


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	MatchSetup.reset()
	_style = UIStyle.new()
	_build()
	_load_catalog()
	_update_selection()
	get_viewport().size_changed.connect(_on_viewport_size_changed)
	_on_viewport_size_changed()
	_start_button.call_deferred("grab_focus")
	DebugLogger.log("[hero_selection] ready")


func _build() -> void:
	var background := ColorRect.new()
	_style.set_full_rect(background)
	background.color = _style.MENU_BACKGROUND
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)

	var tint := ColorRect.new()
	_style.set_full_rect(tint)
	tint.color = Color(0.025, 0.035, 0.06, 0.35)
	tint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(tint)

	var margins := MarginContainer.new()
	_style.set_full_rect(margins)
	add_child(margins)

	var content := VBoxContainer.new()
	content.name = "Content"
	content.add_theme_constant_override("separation", 12)
	margins.add_child(content)

	var header := VBoxContainer.new()
	header.add_theme_constant_override("separation", 2)
	var eyebrow := _style.make_label("BATTLE ROYALE", _style.FONT_SEMIBOLD, 14)
	eyebrow.add_theme_color_override("font_color", _style.MENU_ACCENT)
	header.add_child(eyebrow)
	var title := _style.make_label("CHOOSE YOUR HERO", _style.FONT_SEMIBOLD, 34)
	header.add_child(title)
	var subtitle := _style.make_label(
		"Select a prototype for this match. Heroes are loaded from the simulation catalog.",
		_style.FONT_REGULAR,
		13
	)
	subtitle.add_theme_color_override("font_color", _style.MENU_MUTED_TEXT)
	subtitle.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	header.add_child(subtitle)
	content.add_child(header)

	var line := ColorRect.new()
	line.custom_minimum_size = Vector2(0, 2)
	line.color = _style.MENU_ACCENT
	line.mouse_filter = Control.MOUSE_FILTER_IGNORE
	content.add_child(line)

	var scroll := ScrollContainer.new()
	scroll.name = "HeroScroll"
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	content.add_child(scroll)

	var cards := HBoxContainer.new()
	cards.name = "HeroCards"
	cards.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	cards.size_flags_vertical = Control.SIZE_EXPAND_FILL
	cards.add_theme_constant_override("separation", 16)
	scroll.add_child(cards)

	_status_label = _style.make_label("Loading hero catalog…", _style.FONT_REGULAR, 13)
	_status_label.add_theme_color_override("font_color", _style.MENU_MUTED_TEXT)
	_status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	content.add_child(_status_label)

	_selection_label = _style.make_label("", _style.FONT_SEMIBOLD, 14)
	_selection_label.add_theme_color_override("font_color", _style.MENU_ACCENT)
	content.add_child(_selection_label)

	var footer := HBoxContainer.new()
	footer.alignment = BoxContainer.ALIGNMENT_END
	footer.add_theme_constant_override("separation", 10)
	content.add_child(footer)
	_back_button = _make_button("BACK", false)
	_back_button.pressed.connect(_on_back_pressed)
	footer.add_child(_back_button)
	_start_button = _make_button("START MATCH", true)
	_start_button.pressed.connect(_on_start_pressed)
	footer.add_child(_start_button)


func _load_catalog() -> void:
	var file := FileAccess.open("res://data/stats.yaml", FileAccess.READ)
	if file == null:
		_set_catalog_error("Unable to open data/stats.yaml.")
		return
	var stats_yaml := file.get_as_text()
	file.close()
	var server := SimServer.new()
	_catalog = server.get_hero_catalog(stats_yaml)
	if _catalog.is_empty():
		_set_catalog_error("Hero catalog failed to load. Start Match is disabled.")
		return

	var cards := _find_cards_container()
	if cards == null:
		_set_catalog_error("Hero card container is unavailable.")
		return
	for hero in _catalog:
		var card := _build_hero_card(hero)
		cards.add_child(card)
		_cards.append(card)
	_status_label.text = "%d heroes available" % _catalog.size()
	_start_button.disabled = false


func _find_cards_container() -> HBoxContainer:
	for node in find_children("HeroCards", "HBoxContainer", true, false):
		return node as HBoxContainer
	return null


func _set_catalog_error(message: String) -> void:
	_catalog.clear()
	_status_label.text = message
	_status_label.add_theme_color_override("font_color", Color(1.0, 0.42, 0.45, 1.0))
	_start_button.disabled = true
	DebugLogger.log("[ERROR] [hero_selection] %s" % message)


func _build_hero_card(hero: Dictionary) -> PanelContainer:
	var card := PanelContainer.new()
	card.custom_minimum_size = Vector2(360, 430)
	card.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	card.mouse_filter = Control.MOUSE_FILTER_STOP
	card.set_meta("hero_id", int(hero.get("id", 1)))
	card.gui_input.connect(_on_card_input.bind(card))
	card.add_theme_stylebox_override("panel", _card_style(false))

	var margin := MarginContainer.new()
	margin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	margin.add_theme_constant_override("margin_left", 18)
	margin.add_theme_constant_override("margin_top", 16)
	margin.add_theme_constant_override("margin_right", 18)
	margin.add_theme_constant_override("margin_bottom", 14)
	card.add_child(margin)
	var body := VBoxContainer.new()
	body.mouse_filter = Control.MOUSE_FILTER_IGNORE
	body.add_theme_constant_override("separation", 7)
	margin.add_child(body)

	var portrait := TextureRect.new()
	portrait.custom_minimum_size = Vector2(0, 112)
	portrait.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	portrait.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	portrait.texture = HeroVisualCatalog.portrait(int(hero.get("prefab_id", 0)))
	body.add_child(portrait)

	var name_label := _style.make_label(str(hero.get("name", "Unknown")), _style.FONT_SEMIBOLD, 22)
	name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	body.add_child(name_label)
	var role_label := _style.make_label(str(hero.get("role", "")), _style.FONT_REGULAR, 12)
	role_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	role_label.add_theme_color_override("font_color", _style.MENU_ACCENT)
	body.add_child(role_label)
	var description := _style.make_label(str(hero.get("description", "")), _style.FONT_REGULAR, 12)
	description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	description.custom_minimum_size = Vector2(0, 32)
	description.add_theme_color_override("font_color", _style.MENU_MUTED_TEXT)
	body.add_child(description)

	var stats := _style.make_label(
		"HP %d   MANA %.0f   ATK %.1f   ASP %.2f   MOVE %.1f   RANGE %.1f" % [
			int(hero.get("base_hp", 0)),
			float(hero.get("base_mana", 0.0)),
			float(hero.get("base_attack", 0.0)),
			float(hero.get("base_attack_speed", 0.0)),
			float(hero.get("base_move_speed", 0.0)),
			float(hero.get("attack_range", 0.0)),
		],
		_style.FONT_SEMIBOLD,
		11
	)
	stats.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	stats.add_theme_color_override("font_color", _style.MENU_TEXT)
	body.add_child(stats)

	var skills := hero.get("skills", []) as Array
	for i in skills.size():
		var skill: Dictionary = skills[i]
		var skill_key: String = str(SKILL_KEYS[i]) if i < SKILL_KEYS.size() else str(i + 1)
		var skill_label := _style.make_label(
			"%s  %s: %s" % [skill_key, str(skill.get("name", "Skill")), str(skill.get("description", ""))],
			_style.FONT_REGULAR,
			10
		)
		skill_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		skill_label.add_theme_color_override("font_color", _style.MENU_MUTED_TEXT)
		body.add_child(skill_label)

	var filler := Control.new()
	filler.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_child(filler)
	var select_hint := _style.make_label("CLICK CARD TO SELECT", _style.FONT_SEMIBOLD, 10)
	select_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	select_hint.add_theme_color_override("font_color", _style.MENU_ACCENT)
	body.add_child(select_hint)
	return card


func _card_style(selected: bool) -> StyleBoxFlat:
	var style := _style.panel_style(
		_style.MENU_SURFACE_HOVER if selected else _style.MENU_SURFACE,
		8
	)
	style.border_width_left = 2 if selected else 1
	style.border_width_top = 2 if selected else 1
	style.border_width_right = 2 if selected else 1
	style.border_width_bottom = 2 if selected else 1
	style.border_color = _style.MENU_ACCENT if selected else _style.MENU_BORDER
	return style


func _make_button(text: String, primary: bool) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(190, 46)
	button.add_theme_font_override("font", _style.FONT_SEMIBOLD)
	button.add_theme_font_size_override("font_size", 15)
	_style.apply_menu_button(button, primary)
	return button


func _on_card_input(event: InputEvent, card: PanelContainer) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		for i in _cards.size():
			if _cards[i] == card:
				_selected_index = i
				_update_selection()
				get_viewport().set_input_as_handled()


func _update_selection() -> void:
	if _catalog.is_empty():
		_selection_label.text = ""
		return
	_selected_index = clampi(_selected_index, 0, _catalog.size() - 1)
	for i in _cards.size():
		_cards[i].add_theme_stylebox_override("panel", _card_style(i == _selected_index))
	var hero: Dictionary = _catalog[_selected_index]
	_selection_label.text = "SELECTED: %s" % str(hero.get("name", "Unknown"))
	MatchSetup.selected_hero_id = int(hero.get("id", 1))


func _on_start_pressed() -> void:
	if _transitioning or _catalog.is_empty():
		return
	_transitioning = true
	_start_button.disabled = true
	_back_button.disabled = true
	get_tree().paused = false
	DebugLogger.log("[hero_selection] starting hero_id=%d" % MatchSetup.selected_hero_id)
	var error := get_tree().change_scene_to_file(GAME_SCENE)
	if error != OK:
		_transitioning = false
		_start_button.disabled = false
		_back_button.disabled = false
		DebugLogger.log("[ERROR] [hero_selection] gameplay_scene_failed code=%d" % error)


func _on_back_pressed() -> void:
	if _transitioning:
		return
	_transitioning = true
	get_tree().paused = false
	MatchSetup.reset()
	DebugLogger.log("[hero_selection] returning_to_menu")
	var error := get_tree().change_scene_to_file(START_MENU_SCENE)
	if error != OK:
		_transitioning = false
		DebugLogger.log("[ERROR] [hero_selection] menu_scene_failed code=%d" % error)


func _unhandled_input(event: InputEvent) -> void:
	if _transitioning or not event.pressed or event.echo:
		return
	if event is InputEventKey:
		if event.keycode == KEY_ESCAPE:
			_on_back_pressed()
			get_viewport().set_input_as_handled()
		elif event.keycode in [KEY_1, KEY_KP_1] and not _catalog.is_empty():
			_selected_index = 0
			_update_selection()
			get_viewport().set_input_as_handled()
		elif event.keycode in [KEY_2, KEY_KP_2] and _catalog.size() > 1:
			_selected_index = 1
			_update_selection()
			get_viewport().set_input_as_handled()
		elif event.keycode in [KEY_ENTER, KEY_KP_ENTER, KEY_SPACE]:
			_on_start_pressed()
			get_viewport().set_input_as_handled()


func _on_viewport_size_changed() -> void:
	var viewport_size := get_viewport_rect().size
	var margin := clampi(int(viewport_size.x * 0.06), 20, 72)
	var content_nodes := find_children("Content", "VBoxContainer", true, false)
	if not content_nodes.is_empty():
		var parent := content_nodes[0].get_parent() as MarginContainer
		parent.add_theme_constant_override("margin_left", margin)
		parent.add_theme_constant_override("margin_right", margin)
		parent.add_theme_constant_override("margin_top", clampi(int(viewport_size.y * 0.05), 18, 48))
		parent.add_theme_constant_override("margin_bottom", clampi(int(viewport_size.y * 0.05), 18, 48))
