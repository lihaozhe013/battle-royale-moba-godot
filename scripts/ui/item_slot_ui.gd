class_name ItemSlotUI
extends Control

const SLOT_SIZE := 40.0
const ICON_MARGIN := 3.0
const ICON_SIZE := SLOT_SIZE - ICON_MARGIN * 2.0
const EMPTY_COLOR := Color(0.12, 0.12, 0.12, 1)

var slot_index: int = 0
var _item_id: int = 0
var _count: int = 0
var _built := false

var _slot_frame: Panel
var _icon_surface: Panel
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

	_slot_frame = Panel.new()
	_slot_frame.name = "SlotFrame"
	_slot_frame.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_slot_frame.add_theme_stylebox_override("panel", style.skill_slot_style())
	_slot_frame.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_slot_frame)

	_icon_surface = Panel.new()
	_icon_surface.name = "IconSurface"
	_icon_surface.position = Vector2(ICON_MARGIN, ICON_MARGIN)
	_icon_surface.size = Vector2(ICON_SIZE, ICON_SIZE)
	_icon_surface.add_theme_stylebox_override(
		"panel", style.item_slot_surface_style()
	)
	_icon_surface.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_icon_surface)

	_icon = TextureRect.new()
	_icon.name = "Icon"
	_icon.position = Vector2(ICON_MARGIN, ICON_MARGIN)
	_icon.size = Vector2(ICON_SIZE, ICON_SIZE)
	_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_icon.modulate = EMPTY_COLOR
	add_child(_icon)

	_cooldown_mask = ColorRect.new()
	_cooldown_mask.name = "CooldownMask"
	_cooldown_mask.position = Vector2(ICON_MARGIN, ICON_MARGIN + ICON_SIZE)
	_cooldown_mask.size = Vector2(ICON_SIZE, 0)
	_cooldown_mask.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_cooldown_mask.color = Color(0, 0, 0, 0.55)
	add_child(_cooldown_mask)

	_count_label = style.make_label("", style.FONT_REGULAR, 14)
	_count_label.name = "CountLabel"
	_count_label.position = Vector2(21, 21)
	_count_label.size = Vector2(15, 16)
	_count_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_count_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_count_label.visible = false
	_count_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_count_label)

	_key_hint = style.make_label("", style.FONT_REGULAR, 10)
	_key_hint.name = "KeyHint"
	_key_hint.position = Vector2(4, 3)
	_key_hint.size = Vector2(15, 15)
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
	var mask_height := ICON_SIZE * ratio
	_cooldown_mask.position = Vector2(
		ICON_MARGIN, ICON_MARGIN + ICON_SIZE - mask_height
	)
	_cooldown_mask.size = Vector2(ICON_SIZE, mask_height)


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
	_cooldown_mask.position = Vector2(ICON_MARGIN, ICON_MARGIN + ICON_SIZE)
	_cooldown_mask.size = Vector2(ICON_SIZE, 0)
	_count_label.text = ""
	_count_label.visible = false
