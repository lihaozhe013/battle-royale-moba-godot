class_name UIStyle
extends RefCounted

const FONT_REGULAR: FontFile = preload("res://resources/ui/Cinzel-Regular.ttf")
const FONT_SEMIBOLD: FontFile = preload(
	"res://resources/ui/Cinzel-SemiBold.ttf"
)
const AVATAR_TEXTURE: Texture2D = preload(
	"res://resources/characters/protagonist/skaterMaleA.png"
)

const HUD_BACKGROUND := Color(0.114, 0.114, 0.123, 1.0)
const PANEL_BACKGROUND := Color(0.12, 0.12, 0.14, 0.92)
const TRANSPARENT_BACKGROUND := Color(0.6, 0.6, 0.6, 0.0)
const BAR_BACKGROUND := Color(0.1, 0.1, 0.1, 0.8)
const HEALTH_FILL := Color(0.2, 1.0, 0.2, 1.0)
const MANA_FILL := Color(0.2, 0.4, 1.0, 1.0)
const DARK_SLOT := Color(0.12, 0.12, 0.12, 1.0)


func panel_style(color: Color, radius: int = 0) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.corner_radius_top_left = radius
	style.corner_radius_top_right = radius
	style.corner_radius_bottom_left = radius
	style.corner_radius_bottom_right = radius
	return style


func configure_label(
	label: Label, font: FontFile = FONT_REGULAR, font_size: int = 14
) -> Label:
	label.add_theme_font_override("font", font)
	label.add_theme_font_size_override("font_size", font_size)
	return label


func make_label(
	text: String = "", font: FontFile = FONT_REGULAR, font_size: int = 14
) -> Label:
	var label := Label.new()
	label.text = text
	configure_label(label, font, font_size)
	return label


func make_spacer(size: Vector2) -> Control:
	var spacer := Control.new()
	spacer.custom_minimum_size = size
	spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return spacer


func set_full_rect(control: Control) -> void:
	control.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
