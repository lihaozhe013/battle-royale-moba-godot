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

var _entities = {}  # id -> EntityView

func sync_entities(snap: SimSnapshot) -> void:
	var seen = {}

	for p in snap.players:
		seen[p.id] = true
		var view = _get_or_spawn(p.id, 0, 0)
		view.apply_snapshot(p.x, p.y, p.ang, p.hp, p.max_hp, false)

	for b in snap.bots:
		seen[b.id] = true
		var view = _get_or_spawn(b.id, 1, 0)
		view.apply_snapshot(b.x, b.y, b.ang, b.hp, b.max_hp, b.dead)

	for a in snap.arrows:
		seen[a.id] = true
		var view = _get_or_spawn(a.id, 2, 0)
		view.apply_snapshot(a.x, a.y, a.ang, 0, 0, false)

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
