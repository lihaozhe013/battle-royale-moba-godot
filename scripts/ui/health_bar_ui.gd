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

@onready var _background: ColorRect = $Background
@onready var _damage_bar: ColorRect = $DamageBar
@onready var _fill: ColorRect = $Fill
@onready var _level_badge: ColorRect = $LevelBadge
@onready var _level_label: Label = $LevelBadge/LevelLabel
@onready var _mana_bar: ColorRect = $ManaBar
@onready var _status_label: Label = $StatusLabel


func update_hp(hp: int, max_hp: int) -> void:
	var ratio := float(hp) / float(max_hp) if max_hp > 0 else 0.0
	ratio = clampf(ratio, 0.0, 1.0)
	_hp_ratio = ratio

	_fill.size = Vector2(BAR_WIDTH * _hp_ratio, BAR_HEIGHT)
	_update_color()


func update_level(lv: int, tier: int) -> void:
	_level_label.text = str(lv)
	_level_badge.color = TIER_COLORS.get(tier, TIER_COLORS[0])


func update_mana(mana: float, max_mana: float) -> void:
	var ratio := mana / max_mana if max_mana > 0 else 0.0
	ratio = clampf(ratio, 0.0, 1.0)
	_mana_bar.size = Vector2(BAR_WIDTH * ratio, _mana_bar.size.y)


func update_status(status: int) -> void:
	_status_label.text = STATUS_NAMES.get(status, "")
	_status_label.visible = status > 0


func _update_color() -> void:
	if _team == 2:
		_fill.color = Color(1.0, 0.3, 0.3)
		return
	if _hp_ratio > 0.6:
		_fill.color = Color(0.2, 1.0, 0.2) if _team == 0 else Color(0.2, 0.6, 1.0)
	elif _hp_ratio > 0.25:
		_fill.color = Color(1.0, 0.8, 0.2)
	else:
		_fill.color = Color(1.0, 0.3, 0.3)


func set_team(team: int) -> void:
	_team = team
	_update_color()


func set_screen_position(screen_pos: Vector2) -> void:
	position = screen_pos - Vector2(BAR_OFFSET_X + BAR_WIDTH * 0.5, BAR_OFFSET_Y + BAR_HEIGHT * 0.5)


func reset() -> void:
	_hp_ratio = 1.0
	_damage_ratio = 1.0
	_team = 0
	_fill.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_fill.color = Color(0.2, 1.0, 0.2)
	_damage_bar.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_level_label.text = "1"
	_level_badge.color = TIER_COLORS[0]
	_mana_bar.size = Vector2(0, _mana_bar.size.y)
	_status_label.text = ""
	_status_label.visible = false
	visible = false


func _process(delta: float) -> void:
	_damage_ratio = move_toward(_damage_ratio, _hp_ratio, DAMAGE_LERP_SPEED * delta)
	_damage_bar.size = Vector2(BAR_WIDTH * _damage_ratio, BAR_HEIGHT)
