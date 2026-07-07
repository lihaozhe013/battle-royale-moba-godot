class_name SkillSlotUI
extends Control

const SLOT_SIZE := 48.0

const GRAY := Color(0.12, 0.12, 0.12, 1)

var slot_index: int = 0
var _skill_id: int = 0
var _level: int = 0
var _cooldown_ratio: float = 0.0
var _mana_enough: bool = true

@onready var _icon: TextureRect = $Icon
@onready var _cooldown_mask: ColorRect = $CooldownMask
@onready var _level_label: Label = $LevelLabel
@onready var _key_hint: Label = $KeyHint
@onready var _mana_label: Label = $ManaCostLabel


func set_skill(skill_id: int, level: int) -> void:
	_skill_id = skill_id
	_level = level
	if skill_id == 0:
		_icon.color = GRAY
		_level_label.visible = false
	else:
		_level_label.visible = true
		_level_label.text = str(level)
		_mana_label.visible = true


func set_cooldown(ratio: float) -> void:
	_cooldown_ratio = clampf(ratio, 0.0, 1.0)
	_cooldown_mask.custom_minimum_size.x = SLOT_SIZE * _cooldown_ratio
	_cooldown_mask.size.x = SLOT_SIZE * _cooldown_ratio


func set_mana_state(enough: bool) -> void:
	_mana_enough = enough
	_mana_label.modulate = Color(1, 1, 1, 1) if enough else Color(1, 0.3, 0.3, 0.6)


func set_key_hint(text: String) -> void:
	_key_hint.text = text


func set_level(lv: int) -> void:
	_level = lv
	_level_label.text = str(lv)


func reset() -> void:
	_skill_id = 0
	_level = 0
	_cooldown_ratio = 0.0
	_icon.color = GRAY
	_cooldown_mask.size.x = 0
	_level_label.text = ""
	_level_label.visible = false
	_mana_label.visible = false
