class_name HealthBarUI
extends Control

const BAR_WIDTH := 100.0
const BAR_HEIGHT := 10.0
const DAMAGE_LERP_SPEED := 3.0

var _hp_ratio: float = 1.0
var _damage_ratio: float = 1.0
var _team: int = 0

@onready var _background: ColorRect = $Background
@onready var _damage_bar: ColorRect = $DamageBar
@onready var _fill: ColorRect = $Fill


func update_hp(hp: int, max_hp: int) -> void:
	var ratio := float(hp) / float(max_hp) if max_hp > 0 else 0.0
	ratio = clampf(ratio, 0.0, 1.0)
	_hp_ratio = ratio

	_fill.size = Vector2(BAR_WIDTH * _hp_ratio, BAR_HEIGHT)
	_update_color()


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
	position = screen_pos - Vector2(BAR_WIDTH * 0.5, 0.0)


func reset() -> void:
	_hp_ratio = 1.0
	_damage_ratio = 1.0
	_team = 0
	_fill.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	_fill.color = Color(0.2, 1.0, 0.2)
	_damage_bar.size = Vector2(BAR_WIDTH, BAR_HEIGHT)
	visible = false


func _process(delta: float) -> void:
	_damage_ratio = move_toward(_damage_ratio, _hp_ratio, DAMAGE_LERP_SPEED * delta)
	_damage_bar.size = Vector2(BAR_WIDTH * _damage_ratio, BAR_HEIGHT)
