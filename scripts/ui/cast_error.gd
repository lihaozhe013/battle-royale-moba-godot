extends CanvasLayer

const ERROR_MESSAGES := {
	1: "On Cooldown",
	2: "Not enough Mana",
	3: "Stunned",
	4: "No target selected",
	5: "Target unavailable",
}

const ERROR_DURATION := 2.0

@onready var _label: Label = $VBoxContainer/CastErrorLabel

var _tween: Tween


func show_error(code: int) -> void:
	var msg: String = ERROR_MESSAGES.get(code, "")
	if msg.is_empty():
		return
	visible = true
	_label.modulate = Color(1, 0.3, 0.2, 1)
	_label.scale = Vector2(1, 1)
	_label.pivot_offset = _label.size * 0.5
	_label.text = msg
	if _tween and _tween.is_valid():
		_tween.kill()
	_tween = create_tween()
	_tween.tween_property(_label, "scale", Vector2(1.3, 1.3), 0.08)
	_tween.tween_property(_label, "scale", Vector2(1, 1), 0.35).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BACK)
	_tween.tween_property(_label, "modulate", Color(1, 0.3, 0.2, 0), ERROR_DURATION).set_delay(0.3)
