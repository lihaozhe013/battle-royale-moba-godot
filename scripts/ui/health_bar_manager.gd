class_name HealthBarManager
extends Node

var entity_manager: EntityManager
var health_bar_scene: PackedScene

var _camera: Camera3D
var _active_bars := {}
var _pool := []
var _canvas_layer: CanvasLayer

const HEAD_OFFSET := Vector3(0, 4.8, 0)


func _ready() -> void:
	_canvas_layer = CanvasLayer.new()
	_canvas_layer.layer = 10
	add_child(_canvas_layer)

	_camera = get_node("../CameraController/Camera3D") as Camera3D
	if not _camera:
		push_error("HealthBarManager: Camera3D not found")


func sync_bars(snap: SimSnapshot) -> void:
	var seen := {}

	for p in snap.players:
		seen[p.id] = true
		var bar := _get_or_create(p.id)
		bar.update_hp(p.hp, p.max_hp)
		bar.set_team(0)

	for b in snap.bots:
		seen[b.id] = true
		var bar := _get_or_create(b.id)
		if b.dead:
			bar.visible = false
		else:
			bar.visible = true
			bar.update_hp(b.hp, b.max_hp)
			bar.set_team(2)

	var to_release := []
	for id in _active_bars:
		if not seen.has(id):
			to_release.append(id)
	for id in to_release:
		_release_bar(id)


func _get_or_create(id: int) -> HealthBarUI:
	if _active_bars.has(id):
		return _active_bars[id]
	var bar: HealthBarUI
	if _pool.size() > 0:
		bar = _pool.pop_back()
		bar.reset()
	else:
		bar = _create_bar()
		_canvas_layer.add_child(bar)
	bar.visible = true
	_active_bars[id] = bar
	return bar


func _create_bar() -> HealthBarUI:
	if health_bar_scene:
		var node = health_bar_scene.instantiate()
		if node is HealthBarUI:
			return node

	var bar := HealthBarUI.new()
	bar.custom_minimum_size = Vector2(100, 10)
	bar.mouse_filter = Control.MOUSE_FILTER_IGNORE

	var bg := ColorRect.new()
	bg.name = "Background"
	bg.anchors_preset = Control.PRESET_FULL_RECT
	bg.color = Color(0.1, 0.1, 0.1, 0.8)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bar.add_child(bg)

	var db := ColorRect.new()
	db.name = "DamageBar"
	db.anchors_preset = Control.PRESET_TOP_LEFT
	db.position = Vector2.ZERO
	db.size = Vector2(100, 10)
	db.color = Color(1.0, 0.8, 0.0)
	db.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bar.add_child(db)

	var fill := ColorRect.new()
	fill.name = "Fill"
	fill.anchors_preset = Control.PRESET_TOP_LEFT
	fill.position = Vector2.ZERO
	fill.size = Vector2(100, 10)
	fill.color = Color(0.2, 1.0, 0.2)
	fill.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bar.add_child(fill)

	return bar


func _release_bar(id: int) -> void:
	var bar := _active_bars.get(id) as HealthBarUI
	if not bar:
		return
	bar.reset()
	_pool.append(bar)
	_active_bars.erase(id)


func _process(_delta: float) -> void:
	if not _camera or not entity_manager:
		return

	for id in _active_bars:
		var bar := _active_bars[id] as HealthBarUI
		if not bar.visible:
			continue
		var view := entity_manager.get_entity(id) as EntityView
		if not view:
			continue
		var world_pos := view.global_position + HEAD_OFFSET
		var screen_pos := _camera.unproject_position(world_pos)
		bar.set_screen_position(screen_pos)
