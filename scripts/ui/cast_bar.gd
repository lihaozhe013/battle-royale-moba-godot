extends CanvasLayer

const BAR_WIDTH := 300.0

@onready var _bar_fill: ColorRect = $CastBarPanel/CastBarFill
@onready var _channeling_label: Label = $CastBarPanel/CastBarChannelingLabel


func sync_cast(progress: float) -> void:
	visible = true
	_channeling_label.visible = true
	_bar_fill.offset_right = progress * BAR_WIDTH


func hide_cast() -> void:
	_bar_fill.offset_right = 0
	visible = false
	_channeling_label.visible = false
