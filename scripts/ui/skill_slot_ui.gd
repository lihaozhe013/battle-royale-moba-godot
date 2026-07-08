class_name SkillSlotUI
extends Control

const SLOT_SIZE := 48.0

const GRAY := Color(0.12, 0.12, 0.12, 1)

var slot_index: int = 0
var _skill_id: int = 0
var _cooldown_ratio: float = 0.0
var _mana_enough: bool = true

@onready var _icon: TextureRect = $Icon
@onready var _cooldown_mask: ColorRect = $CooldownMask
@onready var _cd_label: Label = $CooldownLabel
@onready var _key_hint: Label = $KeyHint
@onready var _mana_label: Label = $ManaCostLabel


func _ready() -> void:
	_cooldown_mask.anchors_preset = Control.PRESET_TOP_LEFT
	_cooldown_mask.offset_left = 8.0
	_cooldown_mask.offset_top = 8.0
	_cooldown_mask.size = Vector2(SLOT_SIZE, 0.0)


func set_skill(skill_id: int, level: int, mana_cost: float = 0.0) -> void:
	_skill_id = skill_id
	if skill_id == 0:
		_icon.color = GRAY
		_cd_label.visible = false
		_mana_label.visible = false
	else:
		_mana_label.visible = true
		_mana_label.text = str(int(mana_cost))


func set_cooldown(ratio: float) -> void:
	_cooldown_ratio = clampf(ratio, 0.0, 1.0)
	_cooldown_mask.size = Vector2(SLOT_SIZE, SLOT_SIZE * _cooldown_ratio)
	_cd_label.visible = _cooldown_ratio > 0.0


func set_cooldown_text(seconds: float) -> void:
	_cd_label.text = str(ceil(seconds)) if ceil(seconds) >= 1 else ""


func set_mana_state(enough: bool) -> void:
	_mana_enough = enough
	_mana_label.modulate = Color(1, 1, 1, 1) if enough else Color(1, 0.3, 0.3, 0.6)


func set_key_hint(text: String) -> void:
	_key_hint.text = text


func reset() -> void:
	_skill_id = 0
	_cooldown_ratio = 0.0
	_icon.color = GRAY
	_cooldown_mask.size = Vector2.ZERO
	_mana_label.visible = false
	_cd_label.visible = false
