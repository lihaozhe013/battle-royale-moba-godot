class_name CastBarUI
extends Control

const BAR_WIDTH := 300.0

var _bar_fill: ColorRect
var _channeling_label: Label
var _built := false


func build(style: UIStyle) -> void:
	if _built:
		return
	_built = true
	style.set_full_rect(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	var panel := Control.new()
	panel.name = "CastBarPanel"
	panel.anchor_left = 0.5
	panel.anchor_top = 0.5
	panel.anchor_right = 0.5
	panel.anchor_bottom = 0.5
	panel.offset_left = -150
	panel.offset_top = 50
	panel.offset_right = 150
	panel.offset_bottom = 110
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(panel)

	var background := ColorRect.new()
	background.name = "CastBarBg"
	background.size = Vector2(BAR_WIDTH, 40)
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	background.color = Color(0.098, 0.098, 0.098, 0.8)
	panel.add_child(background)

	_bar_fill = ColorRect.new()
	_bar_fill.name = "CastBarFill"
	_bar_fill.size = Vector2(BAR_WIDTH, 40)
	_bar_fill.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_bar_fill.color = Color(0.051, 0.538, 0.987, 1)
	panel.add_child(_bar_fill)

	_channeling_label = style.make_label(
		"Channeling...", style.FONT_SEMIBOLD, 16
	)
	_channeling_label.name = "CastBarChannelingLabel"
	_channeling_label.size = Vector2(BAR_WIDTH, 40)
	_channeling_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_channeling_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_channeling_label.add_theme_color_override("font_color", Color.WHITE)
	_channeling_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	panel.add_child(_channeling_label)

	visible = false


func sync_cast(progress: float) -> void:
	if not _built:
		return
	visible = true
	_channeling_label.visible = true
	_bar_fill.size.x = clampf(progress, 0.0, 1.0) * BAR_WIDTH


func hide_cast() -> void:
	if not _built:
		return
	_bar_fill.size.x = 0
	visible = false
	_channeling_label.visible = false
