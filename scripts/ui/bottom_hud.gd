class_name BottomHUD
extends CanvasLayer

const KEY_HINTS := ["Q", "W", "E", "R"]

var _skill_slots: Array[SkillSlotUI] = []
var _item_slots: Array[ItemSlotUI] = []

@onready var _avatar: TextureRect = $HUDPanel/HUDContainer/AvatarSection/Avatar
@onready var _hp_fill: ColorRect = $HUDPanel/HUDContainer/ResourceSection/HPContainer/HPFill
@onready var _hp_label: Label = $HUDPanel/HUDContainer/ResourceSection/HPContainer/HPLabel
@onready var _mana_fill: ColorRect = $HUDPanel/HUDContainer/ResourceSection/ManaContainer/ManaFill
@onready var _mana_label: Label = $HUDPanel/HUDContainer/ResourceSection/ManaContainer/ManaLabel
@onready var _stats_label: Label = $HUDPanel/HUDContainer/ResourceSection/StatsLabel
@onready var _skill_section: HBoxContainer = $HUDPanel/HUDContainer/SkillSection
@onready var _item_section: HBoxContainer = $HUDPanel/HUDContainer/ItemSection
@onready var _backpack_section: HBoxContainer = $HUDPanel/HUDContainer/BackpackSection


func _ready() -> void:
	for child in _skill_section.get_children():
		if child is SkillSlotUI:
			child.set_key_hint(KEY_HINTS[_skill_slots.size()])
			_skill_slots.append(child)

	for child in _item_section.get_children():
		if child is ItemSlotUI:
			_item_slots.append(child)

	for child in _backpack_section.get_children():
		if child is ItemSlotUI:
			_item_slots.append(child)


func sync_player(p) -> void:
	var hp_ratio := float(p.hp) / float(p.max_hp) if p.max_hp > 0 else 0.0
	_hp_fill.size.x = _hp_fill.size.x  # placeholder, user will set bar width
	_hp_label.text = "%d/%d" % [p.hp, p.max_hp]

	var mana_val = p.get("mana") if p.get("mana") != null else 0
	var max_mana_val = p.get("max_mana") if p.get("max_mana") != null else 0
	var mana_ratio := float(mana_val) / float(max_mana_val) if max_mana_val > 0 else 0.0
	_mana_label.text = "%d/%d" % [mana_val, max_mana_val]

	_stats_label.text = "Lv%d | ATK:%.0f | ASP:%.2f | Kills:%d | XP:%d/%d" % [
		p.level, p.atk, p.asp, p.kills, p.xp, p.xp_needed
	]


func sync_skills(skills_data: Array) -> void:
	for i in _skill_slots.size():
		if i < skills_data.size():
			var s = skills_data[i]
			_skill_slots[i].set_skill(s.skill_id, s.level)
			var cd_ratio = s.cooldown / s.max_cooldown if s.max_cooldown > 0 else 0.0
			_skill_slots[i].set_cooldown(cd_ratio)
		else:
			_skill_slots[i].reset()


func sync_items(items_data: Array) -> void:
	for i in _item_slots.size():
		if i < items_data.size():
			_item_slots[i].set_item(items_data[i])
		else:
			_item_slots[i].reset()
