class_name SkillSlotUI
extends Control

const SLOT_SIZE := 56.0
const ICON_MARGIN := 4.0
const ICON_SIZE := SLOT_SIZE - ICON_MARGIN * 2.0
const GRAY := Color(0.12, 0.12, 0.12, 1)

var slot_index: int = 0
var _skill_id: int = 0
var _cooldown_ratio: float = 0.0
var _mana_enough: bool = true
var _is_passive := false
var _built := false

var _slot_frame: Panel
var _icon: TextureRect
var _cooldown_mask: ColorRect
var _cd_label: Label
var _key_hint: Label
var _mana_label: Label
var _key_badge: PanelContainer
var _mana_badge: PanelContainer
var _passive_label: Label


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	custom_minimum_size = Vector2(SLOT_SIZE, SLOT_SIZE)
	custom_maximum_size = Vector2(SLOT_SIZE, SLOT_SIZE)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	_slot_frame = Panel.new()
	_slot_frame.name = "SlotFrame"
	_slot_frame.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_slot_frame.add_theme_stylebox_override("panel", style.skill_slot_style())
	_slot_frame.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_slot_frame)

	_icon = TextureRect.new()
	_icon.name = "Icon"
	_icon.position = Vector2(ICON_MARGIN, ICON_MARGIN)
	_icon.size = Vector2(ICON_SIZE, ICON_SIZE)
	_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_icon.modulate = GRAY
	add_child(_icon)

	_cooldown_mask = ColorRect.new()
	_cooldown_mask.name = "CooldownMask"
	_cooldown_mask.position = Vector2(ICON_MARGIN, ICON_MARGIN + ICON_SIZE)
	_cooldown_mask.size = Vector2(ICON_SIZE, 0)
	_cooldown_mask.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_cooldown_mask.color = Color(0.01, 0.02, 0.04, 0.68)
	add_child(_cooldown_mask)

	_cd_label = style.make_label("", style.FONT_SEMIBOLD, 16)
	_cd_label.name = "CooldownLabel"
	_cd_label.position = Vector2(7, 18)
	_cd_label.size = Vector2(42, 21)
	_cd_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_cd_label.add_theme_color_override("font_color", Color(1, 1, 1, 0.9))
	_cd_label.add_theme_color_override(
		"font_outline_color", Color(0, 0, 0, 0.8)
	)
	_cd_label.add_theme_constant_override("outline_size", 2)
	_cd_label.visible = false
	_cd_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_cd_label)

	_mana_badge = PanelContainer.new()
	_mana_badge.name = "ManaCostBadge"
	_mana_badge.position = Vector2(29, 3)
	_mana_badge.size = Vector2(23, 15)
	_mana_badge.add_theme_stylebox_override(
		"panel", style.skill_badge_style(style.SKILL_MANA_ACCENT)
	)
	_mana_badge.visible = false
	_mana_badge.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_mana_badge)

	_mana_label = style.make_label("", style.FONT_SEMIBOLD, 9)
	_mana_label.name = "ManaCostLabel"
	_mana_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_mana_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_mana_label.add_theme_color_override("font_color", style.SKILL_MANA_ACCENT)
	_mana_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_mana_badge.add_child(_mana_label)

	_key_badge = PanelContainer.new()
	_key_badge.name = "KeyBadge"
	_key_badge.position = Vector2(3, 38)
	_key_badge.size = Vector2(18, 15)
	_key_badge.add_theme_stylebox_override(
		"panel", style.skill_badge_style(style.SKILL_KEY_ACCENT)
	)
	_key_badge.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_key_badge)

	_key_hint = style.make_label("", style.FONT_SEMIBOLD, 11)
	_key_hint.name = "KeyHint"
	_key_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_key_hint.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_key_hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_key_badge.add_child(_key_hint)

	_passive_label = style.make_label("PASSIVE", style.FONT_SEMIBOLD, 8)
	_passive_label.name = "PassiveLabel"
	_passive_label.position = Vector2(2, 39)
	_passive_label.size = Vector2(52, 14)
	_passive_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_passive_label.add_theme_color_override("font_color", style.MENU_ACCENT)
	_passive_label.visible = false
	_passive_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_passive_label)


func set_skill(
	skill_id: int,
	mana_cost: float = 0.0,
	icon: Texture2D = null,
	is_passive: bool = false
) -> void:
	if not _built:
		return
	_skill_id = skill_id
	_is_passive = is_passive
	_icon.texture = icon
	if skill_id == 0:
		_icon.modulate = GRAY
		_cd_label.visible = false
		_cooldown_mask.position = Vector2(ICON_MARGIN, ICON_MARGIN + ICON_SIZE)
		_cooldown_mask.size = Vector2(ICON_SIZE, 0)
		_mana_badge.visible = false
		_key_badge.visible = true
		_passive_label.visible = false
	else:
		_icon.modulate = Color.WHITE
		_mana_badge.visible = not is_passive
		_key_badge.visible = not is_passive
		_passive_label.visible = is_passive
		_mana_label.text = str(int(mana_cost))
		if is_passive:
			_cooldown_mask.position = Vector2(ICON_MARGIN, ICON_MARGIN + ICON_SIZE)
			_cooldown_mask.size = Vector2(ICON_SIZE, 0)
			_cd_label.visible = false


func set_cooldown(ratio: float) -> void:
	if not _built:
		return
	if _is_passive:
		_cooldown_ratio = 0.0
		_cooldown_mask.position = Vector2(ICON_MARGIN, ICON_MARGIN + ICON_SIZE)
		_cooldown_mask.size = Vector2(ICON_SIZE, 0)
		_cd_label.visible = false
		return
	_cooldown_ratio = clampf(ratio, 0.0, 1.0)
	var mask_height := ICON_SIZE * _cooldown_ratio
	_cooldown_mask.position = Vector2(
		ICON_MARGIN, ICON_MARGIN + ICON_SIZE - mask_height
	)
	_cooldown_mask.size = Vector2(ICON_SIZE, mask_height)
	_cd_label.visible = not _is_passive and _cooldown_ratio > 0.0


func set_cooldown_text(seconds: float) -> void:
	if not _built:
		return
	_cd_label.text = str(ceil(seconds)) if ceil(seconds) >= 1 else ""


func set_mana_state(enough: bool) -> void:
	if not _built:
		return
	_mana_enough = enough
	_mana_label.modulate = (
		Color(1, 1, 1, 1) if enough else Color(1, 0.3, 0.3, 0.6)
	)


func set_key_hint(text: String) -> void:
	if not _built:
		return
	_key_hint.text = text


func reset() -> void:
	if not _built:
		return
	_skill_id = 0
	_cooldown_ratio = 0.0
	_icon.texture = null
	_icon.modulate = GRAY
	_cooldown_mask.position = Vector2(ICON_MARGIN, ICON_MARGIN + ICON_SIZE)
	_cooldown_mask.size = Vector2(ICON_SIZE, 0)
	_mana_badge.visible = false
	_key_badge.visible = true
	_passive_label.visible = false
	_cd_label.visible = false
	_is_passive = false
