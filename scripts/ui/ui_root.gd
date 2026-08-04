class_name UIRoot
extends Node

var bottom_hud: BottomHUD
var health_bar_manager: HealthBarManager
var cast_bar: CastBarUI
var cast_error: CastErrorUI
var settings_panel: SettingsPanelUI

var _style: UIStyle
var _world_layer: CanvasLayer
var _hud_layer: CanvasLayer
var _feedback_layer: CanvasLayer
var _modal_layer: CanvasLayer
var _initialized := false
var _health_bars_prewarmed := false


func _ready() -> void:
	initialize()


func initialize() -> void:
	if _initialized:
		return
	_initialized = true
	_style = UIStyle.new()
	print(
		(
			"[ui_bootstrap] UIRoot.initialize viewport=%s process_mode=%d"
			% [get_viewport().get_visible_rect().size, process_mode]
		)
	)

	_world_layer = _create_layer("WorldOverlay", 10)
	_hud_layer = _create_layer("HUDLayer", 100)
	_feedback_layer = _create_layer("FeedbackLayer", 101)
	_modal_layer = _create_layer("ModalLayer", 200)

	health_bar_manager = HealthBarManager.new()
	health_bar_manager.name = "HealthBarManager"
	add_child(health_bar_manager)

	bottom_hud = BottomHUD.new()
	bottom_hud.name = "BottomHUD"
	bottom_hud.build(_style)
	_hud_layer.add_child(bottom_hud)

	cast_bar = CastBarUI.new()
	cast_bar.name = "CastBar"
	cast_bar.build(_style)
	_feedback_layer.add_child(cast_bar)

	cast_error = CastErrorUI.new()
	cast_error.name = "CastError"
	cast_error.build(_style)
	_feedback_layer.add_child(cast_error)

	settings_panel = SettingsPanelUI.new()
	settings_panel.name = "SettingsPanel"
	settings_panel.build(_style)
	_modal_layer.add_child(settings_panel)
	print(
		(
			"[ui_bootstrap] UIRoot.initialize complete layers=(%d,%d,%d,%d) bottom_visible=%s bottom_size=%s"
			% [
				_world_layer.layer,
				_hud_layer.layer,
				_feedback_layer.layer,
				_modal_layer.layer,
				bottom_hud.visible,
				bottom_hud.size,
			]
		)
	)
	call_deferred("_log_layout")


func _log_layout() -> void:
	if bottom_hud:
		bottom_hud.log_layout()


func _create_layer(node_name: String, layer_index: int) -> CanvasLayer:
	var layer := CanvasLayer.new()
	layer.name = node_name
	layer.layer = layer_index
	add_child(layer)
	return layer


func bind_runtime(
	entity_manager: EntityManager, camera: Camera3D, cast_settings: CastSettings
) -> void:
	if not _initialized:
		initialize()
	health_bar_manager.setup(_world_layer, entity_manager, camera, _style)
	settings_panel.bind_cast_settings(cast_settings)


func prewarm_health_bars(capacity: int) -> void:
	if not _initialized:
		initialize()
	if _health_bars_prewarmed:
		push_error("[ui_bootstrap] UIRoot health bars were already prewarmed")
		return
	_health_bars_prewarmed = true
	print("[ui_bootstrap] UIRoot.prewarm_health_bars capacity=%d" % capacity)
	health_bar_manager.prewarm(capacity)
