class_name HealthBarManager
extends Node

var entity_manager: EntityManager

var _camera: Camera3D
var _active_bars := {}
var _pool: Array[HealthBarUI] = []
var _world_layer: CanvasLayer
var _style: UIStyle
var _prewarmed := false
var _capacity := 0

const HEAD_OFFSET := Vector3(0, 4.8, 0)


func setup(
	world_layer: CanvasLayer,
	manager: EntityManager,
	camera: Camera3D,
	style: UIStyle
) -> void:
	_world_layer = world_layer
	entity_manager = manager
	_camera = camera
	_style = style


func prewarm(capacity: int) -> void:
	if _prewarmed:
		if capacity != _capacity:
			DebugLogger.log(
				"[ERROR] [ui_bootstrap] Health-bar capacity changed after prewarm"
			)
		return
	if not _world_layer or not _style:
		DebugLogger.log("[ERROR] [ui_bootstrap] HealthBarManager is not configured")
		return
	if capacity < 0:
		DebugLogger.log("[ERROR] [ui_bootstrap] Health-bar capacity cannot be negative")
		return

	_capacity = capacity
	_prewarmed = true
	DebugLogger.log("[ui_bootstrap] HealthBarManager.prewarm capacity=%d" % capacity)
	for _i in capacity:
		var bar := _create_bar()
		_world_layer.add_child(bar)
		bar.reset()
		_pool.append(bar)


func sync_bars(snap: SimSnapshot) -> void:
	if not _prewarmed:
		DebugLogger.log("[ERROR] [ui_bootstrap] Health bars used before prewarm")
		return

	var seen := {}

	if snap.heroes.size() > 0:
		for h in snap.heroes:
			seen[h.id] = true
			var bar := _get_or_create(h.id)
			if not bar:
				continue
			if h.dead:
				bar.visible = false
			else:
				bar.visible = true
				bar.update_hp(h.hp, h.max_hp)
				bar.update_mana(h.mana, h.max_mana)
				bar.update_level(h.level, h.tier)
				bar.update_status(h.status)
				bar.set_team(0 if h.is_local else 2)
	else:
		for p in snap.players:
			seen[p.id] = true
			var player_bar := _get_or_create(p.id)
			if player_bar:
				player_bar.update_hp(p.hp, p.max_hp)
				player_bar.update_mana(p.mana, p.max_mana)
				player_bar.update_level(p.level, 0)
				player_bar.update_status(p.status)
				player_bar.set_team(0)

		for b in snap.bots:
			seen[b.id] = true
			var bot_bar := _get_or_create(b.id)
			if not bot_bar:
				continue
			if b.dead:
				bot_bar.visible = false
			else:
				bot_bar.visible = true
				bot_bar.update_hp(b.hp, b.max_hp)
				bot_bar.update_mana(b.mana, b.max_mana)
				bot_bar.update_level(b.level, b.tier)
				bot_bar.update_status(b.status)
				bot_bar.set_team(2)

	var to_release := []
	for id in _active_bars:
		if not seen.has(id):
			to_release.append(id)
	for id in to_release:
		_release_bar(id)


func _get_or_create(id: int) -> HealthBarUI:
	if _active_bars.has(id):
		return _active_bars[id]
	if _pool.is_empty():
		DebugLogger.log(
			(
				"[ERROR] [ui_bootstrap] Health-bar pool exhausted for entity %d (capacity %d)"
				% [id, _capacity]
			)
		)
		return null

	var bar: HealthBarUI = _pool.pop_back()
	bar.reset()
	bar.visible = true
	_active_bars[id] = bar
	return bar


func _create_bar() -> HealthBarUI:
	var bar := HealthBarUI.new()
	bar.build(_style)
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
