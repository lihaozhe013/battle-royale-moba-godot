class_name HealthBarUI
extends Control

const BAR_WIDTH := 100.0
const BAR_HEIGHT := 10.0
const BAR_OFFSET_X := 24.0
const BAR_OFFSET_Y := 3.0
const DAMAGE_LERP_SPEED := 3.0

const TIER_COLORS := {
	0: Color(0.122, 0.122, 0.122),
	1: Color(0.6, 0.2, 0.8),
	2: Color(1.0, 0.8, 0.0),
}

const STATUS_NAMES := {
	0: "",
	1: "ROOT",
	2: "STUN",
}

var _hp_ratio: float = 1.0
var _damage_ratio: float = 1.0
var _team: int = 0
var _built := false

var _background: ColorRect
var _damage_bar: ColorRect
var _fill: ColorRect
var _level_badge: ColorRect
var _level_label: Label
var _mana_bar: ColorRect
var _status_label: Label


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	custom_minimum_size = Vector2(124, 16)
	custom_maximum_size = Vector2(124, 16)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	_level_badge = ColorRect.new()
	_level_badge.name = "LevelBadge"
	_level_badge.position = Vector2(0, 1)
	_level_badge.size = Vector2(22, 14)
	_level_badge.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_level_badge.color = TIER_COLORS[0]
	add_child(_level_badge)

	_level_label = style.make_label("1", style.FONT_SEMIBOLD, 11)
	_level_label.name = "LevelLabel"
	_level_badge.add_child(_level_label)
	_style_level_label()

	_background = ColorRect.new()
	_background.name = "Background"
	_background.position = Vector2(BAR_OFFSET_X, BAR_OFFSET_Y)
	_background.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_background.color = style.BAR_BACKGROUND
	add_child(_background)

	_damage_bar = ColorRect.new()
	_damage_bar.name = "DamageBar"
	_damage_bar.position = Vector2(BAR_OFFSET_X, BAR_OFFSET_Y)
	_damage_bar.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_damage_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_damage_bar.color = Color(1.0, 0.8, 0.0)
	add_child(_damage_bar)

	_fill = ColorRect.new()
	_fill.name = "Fill"
	_fill.position = Vector2(BAR_OFFSET_X, BAR_OFFSET_Y)
	_fill.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_fill.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_fill.color = style.HEALTH_FILL
	add_child(_fill)

	_mana_bar = ColorRect.new()
	_mana_bar.name = "ManaBar"
	_mana_bar.position = Vector2(BAR_OFFSET_X, 10)
	_mana_bar.size = Vector2(0, 4)
	_mana_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_mana_bar.color = Color(0.263, 0.667, 1.0, 1.0)
	add_child(_mana_bar)

	_status_label = style.make_label("", style.FONT_SEMIBOLD, 18)
	_status_label.name = "StatusLabel"
	_status_label.position = Vector2(BAR_OFFSET_X, -19)
	_status_label.size = Vector2(BAR_WIDTH, 25)
	_status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_status_label.add_theme_color_override(
		"font_color", Color(0.071, 0.071, 0.071, 1)
	)
	_status_label.visible = false
	_status_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_status_label)


func _style_level_label() -> void:
	_level_label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_level_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_level_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_level_label.add_theme_color_override("font_color", Color.WHITE)
	_level_label.mouse_filter = Control.MOUSE_FILTER_IGNORE


func update_hp(hp: int, max_hp: int) -> void:
	if not _built:
		return
	var ratio := float(hp) / float(max_hp) if max_hp > 0 else 0.0
	_hp_ratio = clampf(ratio, 0.0, 1.0)
	_fill.size.x = BAR_WIDTH * _hp_ratio
	_update_color()


func update_level(lv: int, tier: int) -> void:
	if not _built:
		return
	_level_label.text = str(lv)
	_level_badge.color = TIER_COLORS.get(tier, TIER_COLORS[0])


func update_mana(mana: float, max_mana: float) -> void:
	if not _built:
		return
	var ratio := mana / max_mana if max_mana > 0 else 0.0
	ratio = clampf(ratio, 0.0, 1.0)
	_mana_bar.size.x = BAR_WIDTH * ratio


func update_status(status: int) -> void:
	if not _built:
		return
	_status_label.text = STATUS_NAMES.get(status, "")
	_status_label.visible = status > 0


func _update_color() -> void:
	if not _built:
		return
	if _team == 2:
		_fill.color = Color(1.0, 0.3, 0.3)
		return
	if _hp_ratio > 0.6:
		_fill.color = (
			Color(0.2, 1.0, 0.2) if _team == 0 else Color(0.2, 0.6, 1.0)
		)
	elif _hp_ratio > 0.25:
		_fill.color = Color(1.0, 0.8, 0.2)
	else:
		_fill.color = Color(1.0, 0.3, 0.3)


func set_team(team: int) -> void:
	_team = team
	_update_color()


func set_screen_position(screen_pos: Vector2) -> void:
	position = (
		screen_pos
		- Vector2(
			BAR_OFFSET_X + BAR_WIDTH * 0.5, BAR_OFFSET_Y + BAR_HEIGHT * 0.5
		)
	)


func reset() -> void:
	if not _built:
		return
	_hp_ratio = 1.0
	_damage_ratio = 1.0
	_team = 0
	_fill.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_fill.color = Color(0.2, 1.0, 0.2)
	_damage_bar.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_level_label.text = "1"
	_level_badge.color = TIER_COLORS[0]
	_mana_bar.size.x = 0
	_status_label.text = ""
	_status_label.visible = false
	visible = false


func _process(delta: float) -> void:
	if not _built:
		return
	_damage_ratio = move_toward(
		_damage_ratio, _hp_ratio, DAMAGE_LERP_SPEED * delta
	)
	_damage_bar.size.x = BAR_WIDTH * _damage_ratio
