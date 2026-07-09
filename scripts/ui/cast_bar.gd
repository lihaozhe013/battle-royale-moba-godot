extends CanvasLayer

const ERROR_MESSAGES := {
	1: "On Cooldown",
	2: "Not enough Mana",
	3: "Stunned",
	4: "No target selected",
	5: "Target unavailable",
}

const ERROR_DURATION := 2.0
const BAR_WIDTH := 300.0

@onready var _bar_fill: ColorRect = $CastBarPanel/CastBarFill
@onready var _channeling_label: Label = $CastBarPanel/CastBarChannelingLabel
@onready var _error_label: Label = $CastBarPanel/CastBarErrorLabel

var _error_tween: Tween


func sync_cast(progress: float) -> void:
	visible = true
	_channeling_label.visible = true
	_bar_fill.offset_right = progress * BAR_WIDTH


func hide_cast() -> void:
	visible = false
	_channeling_label.visible = false


func show_error(code: int) -> void:
	var msg: String = ERROR_MESSAGES.get(code, "")
	if msg.is_empty():
		return
	_error_label.modulate = Color(1, 0.3, 0.2, 1)
	_error_label.text = msg
	if _error_tween and _error_tween.is_valid():
		_error_tween.kill()
	_error_tween = create_tween()
	_error_tween.tween_property(_error_label, "modulate", Color(1, 0.3, 0.2, 0), ERROR_DURATION).set_delay(0.3)
