class_name MatchResults
extends Control

const START_MENU_SCENE := "res://scenes/start_menu.tscn"
const RESULT_IN_PROGRESS := 0
const RESULT_DEFEAT := 1
const RESULT_VICTORY := 2
const NARROW_BREAKPOINT := 1040.0
const SUMMARY_WIDTH := 320.0
const CONTENT_GAP := 16.0
const TABLE_WIDTH := 930.0

const DEFEAT_COLOR := Color(0.96, 0.38, 0.42, 1.0)
const VICTORY_COLOR := Color(0.42, 0.92, 0.62, 1.0)
const PANEL_SURFACE := Color(0.055, 0.065, 0.09, 0.98)
const PANEL_SURFACE_ALT := Color(0.075, 0.085, 0.12, 0.98)
const ROW_SURFACE := Color(0.08, 0.09, 0.125, 0.98)
const LOCAL_ROW_SURFACE := Color(0.20, 0.16, 0.09, 0.98)
const LOCAL_ROW_BORDER := Color(1.0, 0.78, 0.4, 0.95)
const TABLE_MUTED := Color(0.55, 0.60, 0.70, 1.0)

var _style: UIStyle
var _page_margins: MarginContainer
var _main_scroll: ScrollContainer
var _content_area: Control
var _summary_panel: PanelContainer
var _leaderboard_panel: PanelContainer
var _return_button: Button
var _transitioning := false
var _result_data: Dictionary = {}
var _participants: Array[Dictionary] = []


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_style = UIStyle.new()
	_result_data = MatchResultStore.take_result()
	_participants = _read_participants(_result_data)
	_participants.sort_custom(Callable(self, "_sort_participants"))
	_build()
	_update_layout()
	get_viewport().size_changed.connect(_on_viewport_size_changed)
	call_deferred("_update_layout")
	_return_button.call_deferred("grab_focus")
	DebugLogger.log(
		"[match_results] ready participants=%d has_result=%s"
		% [_participants.size(), not _result_data.is_empty()]
	)


func _build() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP

	var background := ColorRect.new()
	background.name = "Background"
	_style.set_full_rect(background)
	background.color = _style.MENU_BACKGROUND
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)

	var background_tint := ColorRect.new()
	background_tint.name = "BackgroundTint"
	_style.set_full_rect(background_tint)
	background_tint.color = Color(0.025, 0.035, 0.06, 0.5)
	background_tint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background_tint)

	_page_margins = MarginContainer.new()
	_page_margins.name = "PageMargins"
	_style.set_full_rect(_page_margins)
	add_child(_page_margins)

	var page_content := VBoxContainer.new()
	page_content.name = "PageContent"
	page_content.add_theme_constant_override("separation", 12)
	_page_margins.add_child(page_content)

	page_content.add_child(_build_header())

	var accent_line := ColorRect.new()
	accent_line.name = "AccentLine"
	accent_line.custom_minimum_size = Vector2(0, 2)
	accent_line.color = _style.MENU_ACCENT
	accent_line.mouse_filter = Control.MOUSE_FILTER_IGNORE
	page_content.add_child(accent_line)

	_main_scroll = ScrollContainer.new()
	_main_scroll.name = "ResultsScroll"
	_main_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_main_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	_main_scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	page_content.add_child(_main_scroll)

	_content_area = Control.new()
	_content_area.name = "ResultsContent"
	_content_area.custom_minimum_size = Vector2(0, 520)
	_content_area.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_main_scroll.add_child(_content_area)

	_summary_panel = _build_summary_panel()
	_content_area.add_child(_summary_panel)
	_leaderboard_panel = _build_leaderboard_panel()
	_content_area.add_child(_leaderboard_panel)

	var footer := HBoxContainer.new()
	footer.name = "Footer"
	footer.alignment = BoxContainer.ALIGNMENT_END
	footer.custom_minimum_size = Vector2(0, 52)
	page_content.add_child(footer)

	_return_button = _make_button("RETURN TO MAIN MENU", true)
	_return_button.name = "ReturnButton"
	_return_button.custom_minimum_size = Vector2(280, 48)
	_return_button.pressed.connect(_on_return_pressed)
	footer.add_child(_return_button)


func _build_header() -> Control:
	var header := HBoxContainer.new()
	header.name = "Header"
	header.custom_minimum_size = Vector2(0, 72)

	var title_column := VBoxContainer.new()
	title_column.name = "TitleColumn"
	title_column.add_theme_constant_override("separation", 2)
	header.add_child(title_column)

	var eyebrow := _make_label(
		"BATTLE ROYALE", _style.FONT_SEMIBOLD, 14, _style.MENU_ACCENT
	)
	title_column.add_child(eyebrow)

	var title := _make_label("MATCH COMPLETE", _style.FONT_SEMIBOLD, 32)
	title_column.add_child(title)

	var subtitle := _make_label(
		"FINAL PERFORMANCE REPORT", _style.FONT_REGULAR, 12, TABLE_MUTED
	)
	title_column.add_child(subtitle)

	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(spacer)

	var outcome_column := VBoxContainer.new()
	outcome_column.name = "OutcomeColumn"
	outcome_column.alignment = BoxContainer.ALIGNMENT_CENTER
	outcome_column.add_theme_constant_override("separation", 2)
	header.add_child(outcome_column)

	var result := int(_result_data.get("result", RESULT_IN_PROGRESS))
	var outcome_text := "IN PROGRESS"
	var outcome_color := TABLE_MUTED
	if result == RESULT_VICTORY:
		outcome_text = "VICTORY"
		outcome_color = VICTORY_COLOR
	elif result == RESULT_DEFEAT:
		outcome_text = "DEFEAT"
		outcome_color = DEFEAT_COLOR
	var outcome := _make_label(
		outcome_text, _style.FONT_SEMIBOLD, 25, outcome_color
	)
	outcome.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	outcome_column.add_child(outcome)

	var time_label := _make_label(
		"TIME  %s" % _format_duration(_result_data.get("match_time", 0.0)),
		_style.FONT_REGULAR,
		13,
		TABLE_MUTED
	)
	time_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	outcome_column.add_child(time_label)
	return header


func _build_summary_panel() -> PanelContainer:
	var panel := _make_panel(PANEL_SURFACE)
	panel.name = "PlayerSummary"
	panel.custom_minimum_size = Vector2(SUMMARY_WIDTH, 0)

	var margin := _make_margin(panel, 24, 22, 24, 22)
	var content := VBoxContainer.new()
	content.name = "SummaryContent"
	content.add_theme_constant_override("separation", 8)
	margin.add_child(content)

	var heading := _make_label(
		"YOUR PERFORMANCE", _style.FONT_SEMIBOLD, 15, _style.MENU_ACCENT
	)
	content.add_child(heading)

	var local := _find_local_participant()
	var has_local := not local.is_empty()
	var rank_text := "NO MATCH DATA"
	if has_local:
		rank_text = "RANK  #%d / %d" % [
			_find_rank(local),
			_participants.size(),
		]
	var rank_label := _make_label(rank_text, _style.FONT_REGULAR, 13, TABLE_MUTED)
	content.add_child(rank_label)

	var score_label := _make_label(
		str(int(local.get("score", 0))) if has_local else "-",
		_style.FONT_SEMIBOLD,
		46,
		_style.MENU_TEXT,
	)
	score_label.name = "Score"
	content.add_child(score_label)

	var score_caption := _make_label("TOTAL SCORE", _style.FONT_REGULAR, 11, TABLE_MUTED)
	content.add_child(score_caption)

	var separator := HSeparator.new()
	content.add_child(separator)

	var grid := GridContainer.new()
	grid.name = "StatsGrid"
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 12)
	grid.add_theme_constant_override("v_separation", 6)
	content.add_child(grid)

	_add_metric(grid, "K / D", "%d / %d" % [
		int(local.get("kills", 0)),
		int(local.get("deaths", 0)),
	])
	_add_metric(grid, "LEVEL", str(int(local.get("level", 0))))
	_add_metric(grid, "DAMAGE", _format_number(local.get("damage_dealt", 0)))
	_add_metric(grid, "TAKEN", _format_number(local.get("damage_taken", 0)))
	_add_metric(grid, "HEALING", _format_number(local.get("healing_done", 0)))
	_add_metric(grid, "XP EARNED", _format_number(local.get("xp_earned", 0)))
	_add_metric(grid, "SKILLS", str(int(local.get("skill_casts", 0))))
	_add_metric(grid, "HERO", str(local.get("hero_name", "Unknown")))
	_add_metric(grid, "FINAL HP", "%d / %d" % [
		int(local.get("hp", 0)),
		int(local.get("max_hp", 0)),
	])

	var fill := Control.new()
	fill.size_flags_vertical = Control.SIZE_EXPAND_FILL
	content.add_child(fill)

	var hint := _make_label(
		"Score rewards combat pressure, survival and progression.",
		_style.FONT_REGULAR,
		11,
		TABLE_MUTED,
	)
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	content.add_child(hint)
	return panel


func _build_leaderboard_panel() -> PanelContainer:
	var panel := _make_panel(PANEL_SURFACE_ALT)
	panel.name = "Leaderboard"

	var margin := _make_margin(panel, 20, 18, 20, 18)
	var content := VBoxContainer.new()
	content.name = "LeaderboardContent"
	content.add_theme_constant_override("separation", 8)
	margin.add_child(content)

	var heading := _make_label("SCOREBOARD", _style.FONT_SEMIBOLD, 15, _style.MENU_ACCENT)
	content.add_child(heading)

	var table_scroll := ScrollContainer.new()
	table_scroll.name = "TableScroll"
	table_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	table_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	table_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	table_scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	content.add_child(table_scroll)

	var table := VBoxContainer.new()
	table.name = "Table"
	table.custom_minimum_size = Vector2(TABLE_WIDTH, 0)
	table.add_theme_constant_override("separation", 4)
	table_scroll.add_child(table)

	table.add_child(_build_table_header())
	if _participants.is_empty():
		var empty := _make_label(
			"No match data available.", _style.FONT_REGULAR, 14, TABLE_MUTED
		)
		empty.custom_minimum_size = Vector2(TABLE_WIDTH, 42)
		table.add_child(empty)
	else:
		for rank in _participants.size():
			table.add_child(_build_participant_row(_participants[rank], rank + 1))
	return panel


func _build_table_header() -> Control:
	var header := HBoxContainer.new()
	header.name = "TableHeader"
	header.custom_minimum_size = Vector2(TABLE_WIDTH, 26)
	header.add_theme_constant_override("separation", 4)
	_add_table_cell(header, "#", 42, HORIZONTAL_ALIGNMENT_CENTER, TABLE_MUTED, 11)
	_add_table_cell(header, "CONTENDER", 190, HORIZONTAL_ALIGNMENT_LEFT, TABLE_MUTED, 11)
	_add_table_cell(header, "LV", 44, HORIZONTAL_ALIGNMENT_CENTER, TABLE_MUTED, 11)
	_add_table_cell(header, "K / D", 68, HORIZONTAL_ALIGNMENT_CENTER, TABLE_MUTED, 11)
	_add_table_cell(header, "DAMAGE", 98, HORIZONTAL_ALIGNMENT_RIGHT, TABLE_MUTED, 11)
	_add_table_cell(header, "TAKEN", 98, HORIZONTAL_ALIGNMENT_RIGHT, TABLE_MUTED, 11)
	_add_table_cell(header, "HEAL", 92, HORIZONTAL_ALIGNMENT_RIGHT, TABLE_MUTED, 11)
	_add_table_cell(header, "XP", 82, HORIZONTAL_ALIGNMENT_RIGHT, TABLE_MUTED, 11)
	_add_table_cell(header, "CASTS", 70, HORIZONTAL_ALIGNMENT_RIGHT, TABLE_MUTED, 11)
	_add_table_cell(header, "PTS", 100, HORIZONTAL_ALIGNMENT_RIGHT, TABLE_MUTED, 11)
	return header


func _build_participant_row(participant: Dictionary, rank: int) -> Control:
	var is_local := bool(participant.get("is_local", false))
	var panel := _make_panel(LOCAL_ROW_SURFACE if is_local else ROW_SURFACE, 5)
	panel.custom_minimum_size = Vector2(TABLE_WIDTH, 38)
	if is_local:
		var local_style := _style.panel_style(LOCAL_ROW_SURFACE, 5)
		local_style.border_width_left = 1
		local_style.border_width_top = 1
		local_style.border_width_right = 1
		local_style.border_width_bottom = 1
		local_style.border_color = LOCAL_ROW_BORDER
		panel.add_theme_stylebox_override("panel", local_style)

	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 4)
	panel.add_child(row)

	var rank_color := _style.MENU_ACCENT if rank == 1 else _style.MENU_TEXT
	_add_table_cell(row, str(rank), 42, HORIZONTAL_ALIGNMENT_CENTER, rank_color, 14)
	_add_table_cell(
		row,
		_participant_name(participant),
		190,
		HORIZONTAL_ALIGNMENT_LEFT,
		_style.MENU_TEXT if not is_local else _style.MENU_ACCENT,
		13,
	)
	_add_table_cell(
		row, str(int(participant.get("level", 0))), 44, HORIZONTAL_ALIGNMENT_CENTER
	)
	_add_table_cell(
		row,
		"%d / %d" % [
			int(participant.get("kills", 0)),
			int(participant.get("deaths", 0)),
		],
		68,
		HORIZONTAL_ALIGNMENT_CENTER,
	)
	_add_table_cell(
		row,
		_format_number(participant.get("damage_dealt", 0)),
		98,
		HORIZONTAL_ALIGNMENT_RIGHT,
	)
	_add_table_cell(
		row,
		_format_number(participant.get("damage_taken", 0)),
		98,
		HORIZONTAL_ALIGNMENT_RIGHT,
	)
	_add_table_cell(
		row,
		_format_number(participant.get("healing_done", 0)),
		92,
		HORIZONTAL_ALIGNMENT_RIGHT,
	)
	_add_table_cell(
		row,
		_format_number(participant.get("xp_earned", 0)),
		82,
		HORIZONTAL_ALIGNMENT_RIGHT,
	)
	_add_table_cell(
		row,
		str(int(participant.get("skill_casts", 0))),
		70,
		HORIZONTAL_ALIGNMENT_RIGHT,
	)
	_add_table_cell(
		row,
		_format_number(participant.get("score", 0)),
		100,
		HORIZONTAL_ALIGNMENT_RIGHT,
		_style.MENU_ACCENT if is_local else _style.MENU_TEXT,
		14,
	)
	return panel


func _add_metric(grid: GridContainer, caption: String, value: String) -> void:
	var name_label := _make_label(caption, _style.FONT_REGULAR, 11, TABLE_MUTED)
	name_label.custom_minimum_size = Vector2(100, 18)
	grid.add_child(name_label)

	var value_label := _make_label(value, _style.FONT_SEMIBOLD, 13, _style.MENU_TEXT)
	value_label.custom_minimum_size = Vector2(130, 18)
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	grid.add_child(value_label)


func _add_table_cell(
	row: HBoxContainer,
	text: String,
	width: float,
	alignment: HorizontalAlignment,
	color: Color = Color(0.95, 0.96, 1.0, 1.0),
	font_size: int = 12
) -> Label:
	var label := _make_label(text, _style.FONT_REGULAR, font_size, color)
	label.custom_minimum_size = Vector2(width, 28)
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.horizontal_alignment = alignment
	label.clip_text = true
	row.add_child(label)
	return label


func _make_panel(color: Color, radius: int = 8) -> PanelContainer:
	var panel := PanelContainer.new()
	var panel_style := _style.panel_style(color, radius)
	panel_style.border_width_left = 1
	panel_style.border_width_top = 1
	panel_style.border_width_right = 1
	panel_style.border_width_bottom = 1
	panel_style.border_color = _style.MENU_BORDER
	panel.add_theme_stylebox_override("panel", panel_style)
	return panel


func _make_margin(
	parent: Control, left: int, top: int, right: int, bottom: int
) -> MarginContainer:
	var margin := MarginContainer.new()
	margin.name = "Margin"
	margin.add_theme_constant_override("margin_left", left)
	margin.add_theme_constant_override("margin_top", top)
	margin.add_theme_constant_override("margin_right", right)
	margin.add_theme_constant_override("margin_bottom", bottom)
	parent.add_child(margin)
	return margin


func _make_label(
	text: String,
	font: FontFile,
	font_size: int,
	color: Color = Color(0.95, 0.96, 1.0, 1.0)
) -> Label:
	var label := _style.make_label(text, font, font_size)
	label.add_theme_color_override("font_color", color)
	return label


func _make_button(text: String, primary: bool) -> Button:
	var button := Button.new()
	button.text = text
	button.add_theme_font_override("font", _style.FONT_SEMIBOLD)
	button.add_theme_font_size_override("font_size", 14)
	button.focus_mode = Control.FOCUS_ALL
	_style.apply_menu_button(button, primary)
	return button


func _read_participants(result: Dictionary) -> Array[Dictionary]:
	var output: Array[Dictionary] = []
	for participant in result.get("participants", []):
		if participant is Dictionary:
			output.append(participant.duplicate(true))
	return output


func _sort_participants(a: Dictionary, b: Dictionary) -> bool:
	var a_score := int(a.get("score", 0))
	var b_score := int(b.get("score", 0))
	if a_score != b_score:
		return a_score > b_score
	var a_kills := int(a.get("kills", 0))
	var b_kills := int(b.get("kills", 0))
	if a_kills != b_kills:
		return a_kills > b_kills
	var a_damage := int(a.get("damage_dealt", 0))
	var b_damage := int(b.get("damage_dealt", 0))
	if a_damage != b_damage:
		return a_damage > b_damage
	return int(a.get("id", 0)) < int(b.get("id", 0))


func _find_local_participant() -> Dictionary:
	for participant in _participants:
		if bool(participant.get("is_local", false)):
			return participant
	return {}


func _find_rank(participant: Dictionary) -> int:
	for index in _participants.size():
		if int(_participants[index].get("id", -1)) == int(participant.get("id", -2)):
			return index + 1
	return 0


func _participant_name(participant: Dictionary) -> String:
	var hero_name := str(participant.get("hero_name", ""))
	if bool(participant.get("is_local", false)):
		return "YOU  ·  %s" % hero_name if not hero_name.is_empty() else "YOU"
	var tier_names: Array[String] = ["NORMAL", "ELITE", "BOSS"]
	var tier := int(participant.get("tier", 0))
	var tier_name: String = "NORMAL"
	if tier >= 0 and tier < tier_names.size():
		tier_name = tier_names[tier]
	var label := "BOT %d  ·  %s" % [int(participant.get("id", 0)), tier_name]
	return "%s  ·  %s" % [hero_name, label] if not hero_name.is_empty() else label


func _format_number(value) -> String:
	return str(int(value))


func _format_duration(value) -> String:
	var total_seconds := maxi(0, int(float(value)))
	var minutes := int(total_seconds / 60.0)
	var seconds := total_seconds % 60
	return "%02d:%02d" % [minutes, seconds]


func _on_viewport_size_changed() -> void:
	_update_layout()


func _update_layout() -> void:
	if not _page_margins or not _content_area:
		return
	var viewport_size := get_viewport_rect().size
	var horizontal_margin := clampi(int(viewport_size.x * 0.06), 28, 84)
	var vertical_margin := clampi(int(viewport_size.y * 0.055), 20, 52)
	_page_margins.add_theme_constant_override("margin_left", horizontal_margin)
	_page_margins.add_theme_constant_override("margin_right", horizontal_margin)
	_page_margins.add_theme_constant_override("margin_top", vertical_margin)
	_page_margins.add_theme_constant_override("margin_bottom", vertical_margin)

	var available_width := maxf(1.0, viewport_size.x - horizontal_margin * 2.0)
	var is_narrow := available_width < NARROW_BREAKPOINT
	if is_narrow:
		var leaderboard_height := maxf(
			420.0, 82.0 + _participants.size() * 42.0
		)
		var content_height := 270.0 + CONTENT_GAP + leaderboard_height
		_content_area.custom_minimum_size = Vector2(available_width, content_height)
		_content_area.size = Vector2(available_width, content_height)
		_summary_panel.position = Vector2.ZERO
		_summary_panel.size = Vector2(available_width, 270.0)
		_leaderboard_panel.position = Vector2(0, 270.0 + CONTENT_GAP)
		_leaderboard_panel.size = Vector2(available_width, leaderboard_height)
	else:
		var content_height := maxf(460.0, _main_scroll.size.y)
		_content_area.custom_minimum_size = Vector2(available_width, content_height)
		_content_area.size = Vector2(available_width, content_height)
		var summary_width := minf(SUMMARY_WIDTH, available_width * 0.32)
		_summary_panel.position = Vector2.ZERO
		_summary_panel.size = Vector2(summary_width, content_height)
		_leaderboard_panel.position = Vector2(summary_width + CONTENT_GAP, 0)
		_leaderboard_panel.size = Vector2(
			maxf(1.0, available_width - summary_width - CONTENT_GAP),
			content_height
		)


func _on_return_pressed() -> void:
	if _transitioning:
		return
	_transitioning = true
	_return_button.disabled = true
	MatchResultStore.clear()
	MatchSetup.reset()
	get_tree().paused = false
	DebugLogger.log("[match_results] returning_to_menu")
	var error := get_tree().change_scene_to_file(START_MENU_SCENE)
	if error != OK:
		_transitioning = false
		_return_button.disabled = false
		DebugLogger.log(
			"[ERROR] [match_results] menu_scene_failed code=%d" % error
		)


func _unhandled_input(event: InputEvent) -> void:
	if _transitioning or not event is InputEventKey:
		return
	if not event.pressed or event.echo or event.keycode != KEY_ESCAPE:
		return
	_on_return_pressed()
	get_viewport().set_input_as_handled()
