extends Node

var _result: Dictionary = {}


func capture_snapshot(snapshot: SimSnapshot) -> void:
	if snapshot == null:
		clear()
		return

	var participants: Array[Dictionary] = []
	for hero in snapshot.heroes:
		participants.append(
			{
				"id": int(hero.id),
				"hero_def_id": int(hero.hero_def_id) if hero.get("hero_def_id") != null else 0,
				"hero_name": str(hero.hero_name) if hero.get("hero_name") != null else "",
				"prefab_id": int(hero.prefab_id) if hero.get("prefab_id") != null else 0,
				"is_local": bool(hero.is_local),
				"dead": bool(hero.dead),
				"tier": int(hero.tier),
				"level": int(hero.level),
				"hp": int(hero.hp),
				"max_hp": int(hero.max_hp),
				"kills": int(hero.kills),
				"deaths": int(hero.deaths),
				"damage_dealt": int(hero.damage_dealt),
				"damage_taken": int(hero.damage_taken),
				"healing_done": int(hero.healing_done),
				"xp_earned": int(hero.xp_earned),
				"skill_casts": int(hero.skill_casts),
				"score": int(hero.score),
			}
		)

	_result = {
		"result": int(snapshot.result),
		"match_time": float(snapshot.match_time),
		"participants": participants,
	}


func has_result() -> bool:
	return not _result.is_empty()


func take_result() -> Dictionary:
	var result := _result.duplicate(true)
	clear()
	return result


func clear() -> void:
	_result.clear()
