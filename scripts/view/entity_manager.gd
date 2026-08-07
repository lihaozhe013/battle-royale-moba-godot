class_name EntityManager
extends Node

const PREFAB_PATHS := {
	0: "res://scenes/entities/player.tscn",
	1: "res://scenes/entities/bot.tscn",
	2: "res://scenes/entities/arrow.tscn",
}

const PICKUP_PATHS := {
	0: "res://scenes/entities/pickups/pickup_xp.tscn",
	1: "res://scenes/entities/pickups/pickup_heal.tscn",
	2: "res://scenes/entities/pickups/pickup_small_heal.tscn",
}
const IMPACT_EVENT_TYPE := 1

var _entities = {}  # id -> EntityView
var _hovered_id := -1
var _attack_target_id := -1
var _visible_arrow_ids := {}


func sync_entities(snap: SimSnapshot) -> void:
	var seen = {}

	if snap.heroes.size() > 0:
		for h in snap.heroes:
			seen[h.id] = true
			var entity_type := 0 if h.is_local else 1
			var view = _get_or_spawn(h.id, entity_type, 0)
			view.apply_snapshot(
				h.x, h.y, h.ang, h.hp, h.max_hp, h.dead, h.cast_state, h.cast_slot, h.is_moving
			)
	else:
		for p in snap.players:
			seen[p.id] = true
			var view = _get_or_spawn(p.id, 0, 0)
			view.apply_snapshot(
				p.x, p.y, p.ang, p.hp, p.max_hp, false, p.cast_state, p.cast_slot, p.is_moving
			)

		for b in snap.bots:
			seen[b.id] = true
			var view = _get_or_spawn(b.id, 1, 0)
			view.apply_snapshot(
				b.x, b.y, b.ang, b.hp, b.max_hp, b.dead, b.cast_state, b.cast_slot, b.is_moving
			)

	var current_arrow_ids := {}
	for a in snap.arrows:
		seen[a.id] = true
		current_arrow_ids[a.id] = true
		var view = _get_or_spawn(a.id, 2, 0)
		view.apply_snapshot(a.x, a.y, a.ang, 0, 0, false)
		if not _visible_arrow_ids.has(a.id) and a.source_skill_id == 0:
			var owner_view := get_entity(a.owner_id)
			if owner_view:
				owner_view.play_attack_cast()
	_visible_arrow_ids = current_arrow_ids

	for event in snap.events:
		if event.type != IMPACT_EVENT_TYPE or event.source_skill_id != 0:
			continue
		var victim_view := get_entity(event.victim_id)
		if victim_view:
			victim_view.play_under_attack()

	for pk in snap.pickups:
		seen[pk.id] = true
		var view = _get_or_spawn(pk.id, 3, pk.type)
		view.apply_snapshot(pk.x, pk.y, 0, 0, 0, false)

	var to_remove = []
	for id in _entities:
		if not seen.has(id):
			to_remove.append(id)
	for id in to_remove:
		_entities[id].queue_free()
		_entities.erase(id)


func _get_or_spawn(id: int, type: int, ptype: int) -> EntityView:
	if _entities.has(id):
		return _entities[id]
	var view := _instantiate_prefab(type, ptype)
	view.init(id, type, ptype)
	add_child(view)
	_entities[id] = view
	return view


func get_entity(id: int) -> EntityView:
	return _entities.get(id)


func set_hover_id(id: int) -> void:
	if _hovered_id == id:
		return
	if _hovered_id >= 0 and _entities.has(_hovered_id):
		_entities[_hovered_id].set_hovered(false)
	_hovered_id = id
	if id >= 0 and _entities.has(id):
		_entities[id].set_hovered(true)


func get_hovered_id() -> int:
	return _hovered_id


func set_attack_target_id(id: int) -> void:
	if _attack_target_id == id:
		return
	if _attack_target_id >= 0 and _entities.has(_attack_target_id):
		_entities[_attack_target_id].set_attack_targeted(false)
	_attack_target_id = id
	if id >= 0 and _entities.has(id):
		_entities[id].set_attack_targeted(true)


func _instantiate_prefab(type: int, ptype: int) -> EntityView:
	var path := ""
	if type == 3:
		path = PICKUP_PATHS.get(ptype, "")
	else:
		path = PREFAB_PATHS.get(type, "")
	if path != "" and ResourceLoader.exists(path):
		var prefab := load(path) as PackedScene
		if prefab:
			var view := prefab.instantiate() as EntityView
			if view:
				return view
	var fallback := EntityView.new()
	fallback._create_fallback_mesh(type, ptype)
	return fallback
