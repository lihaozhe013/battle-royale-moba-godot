class_name ItemSlotUI
extends Control

const SLOT_SIZE := 40.0
const EMPTY_COLOR := Color(0.12, 0.12, 0.12, 1)

var slot_index: int = 0
var _item_id: int = 0
var _count: int = 0
var _built := false

var _icon: TextureRect
var _cooldown_mask: ColorRect
var _count_label: Label
var _key_hint: Label


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	custom_minimum_size = Vector2(SLOT_SIZE, SLOT_SIZE)
	custom_maximum_size = Vector2(SLOT_SIZE, SLOT_SIZE)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	_icon = TextureRect.new()
	_icon.name = "Icon"
	_icon.size = Vector2(SLOT_SIZE, SLOT_SIZE)
	_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_icon.modulate = EMPTY_COLOR
	add_child(_icon)

	_cooldown_mask = ColorRect.new()
	_cooldown_mask.name = "CooldownMask"
	_cooldown_mask.size = Vector2(0, SLOT_SIZE)
	_cooldown_mask.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_cooldown_mask.color = Color(0, 0, 0, 0.55)
	add_child(_cooldown_mask)

	_count_label = style.make_label("", style.FONT_REGULAR, 14)
	_count_label.name = "CountLabel"
	_count_label.position = Vector2(0, 9)
	_count_label.size = Vector2(SLOT_SIZE, 22)
	_count_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_count_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_count_label.visible = false
	_count_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_count_label)

	_key_hint = style.make_label("", style.FONT_REGULAR, 12)
	_key_hint.name = "KeyHint"
	_key_hint.position = Vector2(0, 20)
	_key_hint.size = Vector2(SLOT_SIZE, 20)
	_key_hint.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_key_hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_key_hint)


func set_item(item_id: int, count: int = 1) -> void:
	if not _built:
		return
	_item_id = item_id
	_count = count
	if item_id == 0:
		_icon.modulate = EMPTY_COLOR
		_count_label.visible = false
	else:
		_icon.modulate = Color.WHITE
		_count_label.visible = count > 1
		_count_label.text = str(count)


func set_cooldown(ratio: float) -> void:
	if not _built:
		return
	ratio = clampf(ratio, 0.0, 1.0)
	_cooldown_mask.size.x = SLOT_SIZE * ratio


func set_key_hint(text: String) -> void:
	if not _built:
		return
	_key_hint.text = text


func reset() -> void:
	if not _built:
		return
	_item_id = 0
	_count = 0
	_icon.modulate = EMPTY_COLOR
	_cooldown_mask.size = Vector2(0, SLOT_SIZE)
	_count_label.text = ""
	_count_label.visible = false
