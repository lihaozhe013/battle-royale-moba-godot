class_name ItemSlotUI
extends Control

const SLOT_SIZE := 40.0

var slot_index: int = 0
var _item_id: int = 0
var _count: int = 0

@onready var _icon: TextureRect = $Icon
@onready var _cooldown_mask: ColorRect = $CooldownMask
@onready var _count_label: Label = $CountLabel
@onready var _key_hint: Label = $KeyHint


func set_item(item_id: int, count: int = 1) -> void:
	_item_id = item_id
	_count = count
	if item_id == 0:
		_icon.color = Color(0.12, 0.12, 0.12, 1)
		_count_label.visible = false
	else:
		_count_label.visible = count > 1
		_count_label.text = str(count)


func set_cooldown(ratio: float) -> void:
	ratio = clampf(ratio, 0.0, 1.0)
	_cooldown_mask.size.x = SLOT_SIZE * ratio


func set_key_hint(text: String) -> void:
	_key_hint.text = text


func reset() -> void:
	_item_id = 0
	_count = 0
	_icon.color = Color(0.12, 0.12, 0.12, 1)
	_cooldown_mask.size.x = 0.0
	_count_label.text = ""
	_count_label.visible = false
