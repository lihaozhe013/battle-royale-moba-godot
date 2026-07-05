extends Node

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
		view.apply_snapshot(b.x, b.y, 0, b.hp, b.max_hp, b.dead)

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
	var view = EntityView.new()
	view.init(id, type, ptype)
	add_child(view)
	_entities[id] = view
	return view
