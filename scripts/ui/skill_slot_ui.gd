class_name SkillSlotUI
extends Control

const SLOT_SIZE := 48.0
const GRAY := Color(0.12, 0.12, 0.12, 1)

var slot_index: int = 0
var _skill_id: int = 0
var _cooldown_ratio: float = 0.0
var _mana_enough: bool = true
var _built := false

var _icon: TextureRect
var _cooldown_mask: ColorRect
var _cd_label: Label
var _key_hint: Label
var _mana_label: Label


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	custom_minimum_size = Vector2(64, 64)
	custom_maximum_size = Vector2(64, 64)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	_icon = TextureRect.new()
	_icon.name = "Icon"
	_icon.position = Vector2(8, 8)
	_icon.size = Vector2(SLOT_SIZE, SLOT_SIZE)
	_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_icon.modulate = GRAY
	add_child(_icon)

	_cooldown_mask = ColorRect.new()
	_cooldown_mask.name = "CooldownMask"
	_cooldown_mask.position = Vector2(8, 8)
	_cooldown_mask.size = Vector2(SLOT_SIZE, 0)
	_cooldown_mask.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_cooldown_mask.color = Color(0, 0, 0, 0.55)
	add_child(_cooldown_mask)

	_cd_label = style.make_label("", style.FONT_REGULAR, 18)
	_cd_label.name = "CooldownLabel"
	_cd_label.position = Vector2(12, 20)
	_cd_label.size = Vector2(40, 24)
	_cd_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_cd_label.add_theme_color_override("font_color", Color(1, 1, 1, 0.9))
	_cd_label.add_theme_color_override(
		"font_outline_color", Color(0, 0, 0, 0.8)
	)
	_cd_label.add_theme_constant_override("outline_size", 2)
	_cd_label.visible = false
	_cd_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_cd_label)

	_mana_label = style.make_label("", style.FONT_REGULAR, 9)
	_mana_label.name = "ManaCostLabel"
	_mana_label.position = Vector2(37, 15)
	_mana_label.size = Vector2(19, 22)
	_mana_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_mana_label.add_theme_color_override("font_color", Color(0.268, 0.55, 1, 1))
	_mana_label.visible = false
	_mana_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_mana_label)

	_key_hint = style.make_label("", style.FONT_REGULAR, 14)
	_key_hint.name = "KeyHint"
	_key_hint.position = Vector2(0, 20)
	_key_hint.size = Vector2(64, 24)
	_key_hint.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_key_hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_key_hint)


func set_skill(
	skill_id: int, mana_cost: float = 0.0, icon: Texture2D = null
) -> void:
	if not _built:
		return
	_skill_id = skill_id
	_icon.texture = icon
	if skill_id == 0:
		_icon.modulate = GRAY
		_cd_label.visible = false
		_mana_label.visible = false
	else:
		_icon.modulate = Color.WHITE
		_mana_label.visible = true
		_mana_label.text = str(int(mana_cost))


func set_cooldown(ratio: float) -> void:
	if not _built:
		return
	_cooldown_ratio = clampf(ratio, 0.0, 1.0)
	_cooldown_mask.size = Vector2(SLOT_SIZE, SLOT_SIZE * _cooldown_ratio)
	_cd_label.visible = _cooldown_ratio > 0.0


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
	_cooldown_mask.size = Vector2.ZERO
	_mana_label.visible = false
	_cd_label.visible = false
