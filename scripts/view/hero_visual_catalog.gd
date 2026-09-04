class_name HeroVisualCatalog
extends RefCounted

const HERO_SCENE_PATHS: Dictionary = {
	0: "res://scenes/entities/player.tscn",
	1: "res://scenes/entities/player.tscn",
}

const HERO_PORTRAIT_PATHS: Dictionary = {
	0: "res://resources/characters/Maria_W_Magic_Pose/Maria_W_maria_diffuse.png",
	1: "res://resources/characters/Maria_W_Magic_Pose/Maria_W_maria_diffuse.png",
}
const HERO_ANIMATION_SCRIPTS: Dictionary = {
	0: preload("res://scripts/view/maria_character_animation.gd"),
	1: preload("res://scripts/view/maria_character_animation.gd"),
}


static func scene_path(prefab_id: int) -> String:
	return HERO_SCENE_PATHS.get(prefab_id, HERO_SCENE_PATHS[0])


static func portrait(prefab_id: int) -> Texture2D:
	var path: String = HERO_PORTRAIT_PATHS.get(prefab_id, HERO_PORTRAIT_PATHS[0])
	if not ResourceLoader.exists(path):
		return null
	return load(path) as Texture2D


static func animation_script(prefab_id: int) -> Script:
	return HERO_ANIMATION_SCRIPTS.get(prefab_id, HERO_ANIMATION_SCRIPTS[0]) as Script
