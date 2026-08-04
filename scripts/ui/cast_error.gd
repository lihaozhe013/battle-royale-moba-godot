class_name CastErrorUI
extends Control

const ERROR_MESSAGES := {
	1: "On Cooldown",
	2: "Not enough Mana",
	3: "Stunned",
	4: "No target selected",
	5: "Target unavailable",
}

const ERROR_DURATION := 2.0

var _label: Label
var _tween: Tween
var _built := false


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	style.set_full_rect(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	var container := VBoxContainer.new()
	container.name = "VBoxContainer"
	container.anchor_left = 0.5
	container.anchor_top = 1.0
	container.anchor_right = 0.5
	container.anchor_bottom = 1.0
	container.offset_left = -200
	container.offset_top = -117
	container.offset_right = 200
	container.grow_horizontal = Control.GROW_DIRECTION_BOTH
	container.grow_vertical = Control.GROW_DIRECTION_BEGIN
	container.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(container)

	_label = style.make_label("", style.FONT_SEMIBOLD, 40)
	_label.name = "CastErrorLabel"
	_label.custom_minimum_size = Vector2(400, 60)
	_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_label.add_theme_color_override("font_color", Color(1, 0.18, 0.047, 1))
	_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	container.add_child(_label)

	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, 240)
	spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	container.add_child(spacer)

	visible = false


func show_error(code: int) -> void:
	if not _built:
		return
	var msg: String = ERROR_MESSAGES.get(code, "")
	if msg.is_empty():
		return
	visible = true
	_label.modulate = Color(1, 0.3, 0.2, 1)
	_label.scale = Vector2.ONE
	_label.pivot_offset = _label.size * 0.5
	_label.text = msg
	if _tween and _tween.is_valid():
		_tween.kill()
	_tween = create_tween()
	_tween.tween_property(_label, "scale", Vector2(1.3, 1.3), 0.08)
	(
		_tween
		. tween_property(_label, "scale", Vector2.ONE, 0.35)
		. set_ease(Tween.EASE_OUT)
		. set_trans(Tween.TRANS_BACK)
	)
	(
		_tween
		. tween_property(
			_label, "modulate", Color(1, 0.3, 0.2, 0), ERROR_DURATION
		)
		. set_delay(0.3)
	)
	_tween.tween_callback(_hide_after_error)


func _hide_after_error() -> void:
	visible = false
