class_name UIStyle
extends RefCounted

const FONT_REGULAR: FontFile = preload("res://resources/ui/Cinzel-Regular.ttf")
const FONT_SEMIBOLD: FontFile = preload(
	"res://resources/ui/Cinzel-SemiBold.ttf"
)
const AVATAR_TEXTURE: Texture2D = preload(
	"res://resources/characters/protagonist/skaterMaleA.png"
)
const SKILL_ICONS: Dictionary = {
	1: preload("res://data/skills/icons/melee_strike.png"),
	2: preload("res://data/skills/icons/aoe_field.png"),
	3: preload("res://data/skills/icons/dash.png"),
	4: preload("res://data/skills/icons/channel_burst.png"),
}

const HUD_BACKGROUND := Color(0.114, 0.114, 0.123, 1.0)
const PANEL_BACKGROUND := Color(0.12, 0.12, 0.14, 0.92)
const TRANSPARENT_BACKGROUND := Color(0.6, 0.6, 0.6, 0.0)
const BAR_BACKGROUND := Color(0.1, 0.1, 0.1, 0.8)
const HEALTH_FILL := Color(0.2, 1.0, 0.2, 1.0)
const MANA_FILL := Color(0.2, 0.4, 1.0, 1.0)
const DARK_SLOT := Color(0.12, 0.12, 0.12, 1.0)
const SKILL_SLOT_BACKGROUND := Color(0.045, 0.055, 0.08, 0.98)
const SKILL_SLOT_BORDER := Color(0.36, 0.42, 0.52, 1.0)
const SKILL_KEY_ACCENT := Color(1.0, 0.78, 0.4, 1.0)
const SKILL_MANA_ACCENT := Color(0.32, 0.68, 1.0, 1.0)


func panel_style(color: Color, radius: int = 0) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.corner_radius_top_left = radius
	style.corner_radius_top_right = radius
	style.corner_radius_bottom_left = radius
	style.corner_radius_bottom_right = radius
	return style


func skill_slot_style() -> StyleBoxFlat:
	var style := panel_style(SKILL_SLOT_BACKGROUND, 7)
	style.border_width_left = 1
	style.border_width_top = 1
	style.border_width_right = 1
	style.border_width_bottom = 1
	style.border_color = SKILL_SLOT_BORDER
	style.shadow_color = Color(0, 0, 0, 0.55)
	style.shadow_size = 2
	return style


func skill_badge_style(accent: Color) -> StyleBoxFlat:
	var style := panel_style(Color(0.02, 0.03, 0.05, 0.94), 4)
	style.border_width_left = 1
	style.border_width_top = 1
	style.border_width_right = 1
	style.border_width_bottom = 1
	style.border_color = Color(accent.r, accent.g, accent.b, 0.8)
	style.content_margin_left = 2.0
	style.content_margin_top = 1.0
	style.content_margin_right = 2.0
	style.content_margin_bottom = 1.0
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


func get_skill_icon(skill_id: int) -> Texture2D:
	return SKILL_ICONS.get(skill_id) as Texture2D
