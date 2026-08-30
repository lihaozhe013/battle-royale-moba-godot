class_name BottomHUD
extends Control

const KEY_HINTS := ["Q", "W", "E", "R"]
const BASE_HUD_WIDTH := 750.0
const BASE_HUD_HEIGHT := 90.0
const HUD_WIDTH_RATIO := 0.75
const HUD_SCALE := 0.70
const HP_BAR_WIDTH := 280.0
const MANA_BAR_WIDTH := 280.0
const SKILL_TOP_SPACE := 1.0
const SKILL_BOTTOM_SPACE := 4.0
const RESOURCE_BOTTOM_SPACE := 2.0
const AVATAR_SECTION_SIZE := 88.0
const STATS_PANEL_WIDTH := 96.0
const STATS_PANEL_HEIGHT := 84.0
const ITEM_SLOT_GAP := 4.0
const ITEM_SECTION_WIDTH := ItemSlotUI.SLOT_SIZE * 3.0 + ITEM_SLOT_GAP * 2.0

var _style: UIStyle
var _built := false
var _hud_panel: Control
var _skill_slots: Array[SkillSlotUI] = []
var _item_slots: Array[ItemSlotUI] = []

var _avatar: TextureRect
var _hero_name_label: Label
var _hp_fill: ColorRect
var _hp_label: Label
var _mana_fill: ColorRect
var _mana_label: Label
var _stat_value_labels: Array[Label] = []


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	_style = style
	DebugLogger.log("[ui_bootstrap] BottomHUD.build started")
	style.set_full_rect(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	resized.connect(_on_resized)

	_hud_panel = Control.new()
	_hud_panel.name = "HUDPanel"
	_hud_panel.anchor_left = 0.5
	_hud_panel.anchor_top = 1.0
	_hud_panel.anchor_right = 0.5
	_hud_panel.anchor_bottom = 1.0
	_hud_panel.offset_left = -BASE_HUD_WIDTH / 2.0
	_hud_panel.offset_top = -BASE_HUD_HEIGHT
	_hud_panel.offset_right = BASE_HUD_WIDTH / 2.0
	_hud_panel.offset_bottom = 0.0
	_hud_panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_hud_panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	_hud_panel.pivot_offset = Vector2(BASE_HUD_WIDTH / 2.0, BASE_HUD_HEIGHT)
	_hud_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_hud_panel)

	var panel := PanelContainer.new()
	panel.name = "PanelContainer"
	panel.position = Vector2(25, 0)
	panel.size = Vector2(700, BASE_HUD_HEIGHT)
	panel.add_theme_stylebox_override(
		"panel", style.panel_style(style.HUD_BACKGROUND, 5)
	)
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hud_panel.add_child(panel)

	var container := HBoxContainer.new()
	container.name = "HUDContainer"
	container.position = Vector2(36, 0)
	container.size = Vector2(700, BASE_HUD_HEIGHT)
	container.add_theme_constant_override("separation", 0)
	container.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hud_panel.add_child(container)

	container.add_child(style.make_spacer(Vector2(11, 0)))
	var stats_panel := PanelContainer.new()
	stats_panel.name = "StatsPanel"
	stats_panel.custom_minimum_size = Vector2(
		STATS_PANEL_WIDTH, STATS_PANEL_HEIGHT
	)
	var stats_panel_style := style.panel_style(style.PANEL_BACKGROUND, 6)
	stats_panel_style.content_margin_left = 6.0
	stats_panel_style.content_margin_top = 4.0
	stats_panel_style.content_margin_right = 6.0
	stats_panel_style.content_margin_bottom = 4.0
	stats_panel.add_theme_stylebox_override("panel", stats_panel_style)
	var stats_box := VBoxContainer.new()
	stats_box.name = "StatsBox"
	stats_box.add_theme_constant_override("separation", 1)
	var stats_title := style.make_label("STATS", style.FONT_SEMIBOLD, 10)
	stats_title.custom_minimum_size = Vector2(0, 14)
	stats_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	stats_title.mouse_filter = Control.MOUSE_FILTER_IGNORE
	stats_box.add_child(stats_title)

	var stats_grid := GridContainer.new()
	stats_grid.name = "StatsGrid"
	stats_grid.columns = 2
	stats_grid.add_theme_constant_override("h_separation", 5)
	stats_grid.add_theme_constant_override("v_separation", 0)
	for stat_name in ["LV", "ATK", "ASP", "KILLS", "XP"]:
		var name_label := style.make_label(stat_name, style.FONT_REGULAR, 9)
		name_label.add_theme_color_override(
			"font_color", Color(0.62, 0.66, 0.74, 1)
		)
		name_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		stats_grid.add_child(name_label)

		var value_label := style.make_label("-", style.FONT_SEMIBOLD, 10)
		value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		value_label.add_theme_color_override(
			"font_color", Color(0.95, 0.96, 1.0, 1)
		)
		value_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_stat_value_labels.append(value_label)
		stats_grid.add_child(value_label)
	stats_box.add_child(stats_grid)
	stats_panel.add_child(stats_box)
	container.add_child(stats_panel)

	container.add_child(style.make_spacer(Vector2(11, 0)))
	container.add_child(_build_avatar_section())
	container.add_child(style.make_spacer(Vector2(7, 0)))
	container.add_child(_build_resource_section())
	container.add_child(style.make_spacer(Vector2(8, 0)))
	container.add_child(_build_item_section())
	DebugLogger.log(
		(
			"[ui_bootstrap] BottomHUD.build complete child_count=%d root_size=%s"
			% [get_child_count(), size]
		)
	)
	call_deferred("_layout_hud")


func _on_resized() -> void:
	_layout_hud()


func _layout_hud() -> void:
	if not _hud_panel:
		return
	var viewport_width := get_viewport_rect().size.x
	if viewport_width <= 0.0:
		return
	var target_width := viewport_width * HUD_WIDTH_RATIO
	var hud_scale := target_width / BASE_HUD_WIDTH
	_hud_panel.scale = Vector2.ONE * hud_scale * HUD_SCALE


func log_layout() -> void:
	var hud_panel := get_node_or_null("HUDPanel") as Control
	if not hud_panel:
		DebugLogger.log("[ui_bootstrap] BottomHUD.layout missing HUDPanel")
		return
	DebugLogger.log(
		(
			"[ui_bootstrap] BottomHUD.layout visible=%s root_size=%s root_global=%s panel_position=%s panel_size=%s panel_scale=%s visual_width=%.1f panel_global=%s viewport=%s"
			% [
				visible,
				size,
				global_position,
				hud_panel.position,
				hud_panel.size,
				hud_panel.scale,
				hud_panel.size.x * hud_panel.scale.x,
				hud_panel.global_position,
				get_viewport().get_visible_rect().size,
			]
		)
	)


func _build_avatar_section() -> Control:
	var section := Control.new()
	section.name = "AvatarSection"
	section.custom_minimum_size = Vector2(
		AVATAR_SECTION_SIZE, AVATAR_SECTION_SIZE
	)
	section.custom_maximum_size = Vector2(
		AVATAR_SECTION_SIZE, AVATAR_SECTION_SIZE
	)

	_avatar = TextureRect.new()
	_avatar.name = "Avatar"
	_avatar.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_avatar.texture = UIStyle.AVATAR_TEXTURE
	_avatar.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_avatar.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_avatar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	section.add_child(_avatar)

	_hero_name_label = _style.make_label("Player", _style.FONT_REGULAR, 12)
	_hero_name_label.position = Vector2(0, AVATAR_SECTION_SIZE - 20)
	_hero_name_label.size = Vector2(AVATAR_SECTION_SIZE, 20)
	_hero_name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hero_name_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	section.add_child(_hero_name_label)
	return section


func _build_resource_section() -> Control:
	var section := VBoxContainer.new()
	section.name = "ResourceSection"
	section.custom_minimum_size = Vector2(MANA_BAR_WIDTH, BASE_HUD_HEIGHT)
	section.alignment = BoxContainer.ALIGNMENT_END
	section.add_theme_constant_override("separation", 0)

	section.add_child(_style.make_spacer(Vector2(0, SKILL_TOP_SPACE)))
	var skill_section := HBoxContainer.new()
	skill_section.name = "SkillSection"
	skill_section.add_theme_constant_override("separation", 4)
	skill_section.alignment = BoxContainer.ALIGNMENT_CENTER
	for i in 4:
		var slot := SkillSlotUI.new()
		slot.slot_index = i
		slot.build(_style)
		slot.set_key_hint(KEY_HINTS[i])
		_skill_slots.append(slot)
		skill_section.add_child(slot)
	section.add_child(skill_section)

	section.add_child(_style.make_spacer(Vector2(0, SKILL_BOTTOM_SPACE)))
	var hp_container := _build_resource_bar("HP")
	_hp_fill = hp_container.get_node("Fill") as ColorRect
	_hp_label = hp_container.get_node("Label") as Label
	section.add_child(hp_container)

	var mana_container := _build_resource_bar("Mana")
	_mana_fill = mana_container.get_node("Fill") as ColorRect
	_mana_label = mana_container.get_node("Label") as Label
	_mana_fill.color = _style.MANA_FILL
	_mana_label.add_theme_font_size_override("font_size", 11)
	section.add_child(mana_container)
	section.add_child(_style.make_spacer(Vector2(0, RESOURCE_BOTTOM_SPACE)))
	return section


func _build_resource_bar(kind: String) -> Control:
	var container := Control.new()
	container.name = "%sContainer" % kind
	var base_width := HP_BAR_WIDTH if kind == "HP" else MANA_BAR_WIDTH
	var base_height := 14.0 if kind == "HP" else 10.0
	container.custom_minimum_size = Vector2(base_width, base_height)

	var background := ColorRect.new()
	background.name = "Background"
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	background.color = _style.BAR_BACKGROUND
	container.add_child(background)

	var fill := ColorRect.new()
	fill.name = "Fill"
	fill.position = Vector2.ZERO
	fill.size = Vector2(base_width, base_height)
	fill.mouse_filter = Control.MOUSE_FILTER_IGNORE
	fill.color = _style.HEALTH_FILL
	container.add_child(fill)

	var label := _style.make_label(
		"", _style.FONT_REGULAR, 14 if kind == "HP" else 11
	)
	label.name = "Label"
	label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	container.add_child(label)
	return container


func _build_item_section() -> Control:
	var section := VBoxContainer.new()
	section.name = "ItemSection"
	section.custom_minimum_size = Vector2(ITEM_SECTION_WIDTH, BASE_HUD_HEIGHT)
	section.alignment = BoxContainer.ALIGNMENT_CENTER
	section.add_theme_constant_override("separation", ITEM_SLOT_GAP)
	for row_index in 2:
		var row := HBoxContainer.new()
		row.name = "Row%d" % (row_index + 1)
		row.custom_minimum_size = Vector2(
			ITEM_SECTION_WIDTH, ItemSlotUI.SLOT_SIZE
		)
		row.add_theme_constant_override("separation", ITEM_SLOT_GAP)
		for column_index in 3:
			var slot := ItemSlotUI.new()
			slot.slot_index = row_index * 3 + column_index
			slot.build(_style)
			_item_slots.append(slot)
			row.add_child(slot)
		section.add_child(row)
	return section


func sync_player(p) -> void:
	if not _built:
		return
	var prefab_id := int(p.get("prefab_id")) if p.get("prefab_id") != null else 0
	var hero_name := str(p.get("hero_name")) if p.get("hero_name") != null else "Player"
	_hero_name_label.text = hero_name if not hero_name.is_empty() else "Player"
	_avatar.texture = _style.get_hero_portrait(prefab_id)
	var hp_ratio := float(p.hp) / float(p.max_hp) if p.max_hp > 0 else 0.0
	hp_ratio = clampf(hp_ratio, 0.0, 1.0)
	_hp_fill.size.x = HP_BAR_WIDTH * hp_ratio
	_hp_label.text = "%d/%d" % [p.hp, p.max_hp]

	var mana_val = p.get("mana") if p.get("mana") != null else 0
	var max_mana_val = p.get("max_mana") if p.get("max_mana") != null else 0
	var mana_ratio := (
		float(mana_val) / float(max_mana_val) if max_mana_val > 0 else 0.0
	)
	mana_ratio = clampf(mana_ratio, 0.0, 1.0)
	_mana_fill.size.x = MANA_BAR_WIDTH * mana_ratio
	_mana_label.text = "%d/%d" % [mana_val, max_mana_val]

	var stat_values := [
		str(p.level),
		str(int(p.atk)),
		"%.2f" % p.asp,
		str(p.kills),
		"%d/%d" % [p.xp, p.xp_needed],
	]
	for i in stat_values.size():
		_stat_value_labels[i].text = stat_values[i]


func sync_skills(skills_data: Array) -> void:
	if not _built:
		return
	for i in _skill_slots.size():
		if i < skills_data.size():
			var s = skills_data[i]
			_skill_slots[i].set_skill(
				s.skill_id,
				s.mana_cost,
				_style.get_skill_icon(s.skill_id),
				bool(s.get("is_passive")) if s.get("is_passive") != null else false
			)
			var cd_ratio = (
				s.cooldown / s.max_cooldown if s.max_cooldown > 0 else 0.0
			)
			_skill_slots[i].set_cooldown(cd_ratio)
			_skill_slots[i].set_cooldown_text(s.cooldown)
		else:
			_skill_slots[i].reset()


func sync_items(items_data: Array) -> void:
	if not _built:
		return
	for i in _item_slots.size():
		if i >= items_data.size():
			_item_slots[i].reset()
			continue
		var item = items_data[i]
		if item is Dictionary:
			_item_slots[i].set_item(
				int(item.get("item_id", item.get("id", 0))),
				int(item.get("count", 1))
			)
		elif item is Array and item.size() > 0:
			_item_slots[i].set_item(
				int(item[0]), int(item[1]) if item.size() > 1 else 1
			)
		else:
			_item_slots[i].set_item(int(item))
