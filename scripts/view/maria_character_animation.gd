extends Node3D

enum State {
	IDLE,
	RUN,
	SHIELD_CAST,
	ATTACK_CAST,
	UNDER_ATTACK,
	HIDDEN,
}

const MODEL_SCENE: PackedScene = preload(
	"res://resources/characters/Maria_W_Magic_Pose/Maria_W.glb"
)
const IDLE_CLIP_SCENE: PackedScene = preload(
	"res://resources/characters/Maria_W_Magic_Pose/standing idle_normalized.glb"
)
const RUN_CLIP_SCENE: PackedScene = preload(
	"res://resources/characters/Maria_W_Magic_Pose/Standing Run Forward_normalized.glb"
)
const SHIELD_CLIP_SCENE: PackedScene = preload(
	"res://resources/characters/Maria_W_Magic_Pose/Standing 2H Magic Area Attack 02_normalized.glb"
)
const ATTACK_CLIP_SCENE: PackedScene = preload(
	"res://resources/characters/Maria_W_Magic_Pose/Standing 1H Magic Attack 01_normalized.glb"
)
const UNDER_ATTACK_CLIP_SCENE: PackedScene = preload(
	"res://resources/characters/Maria_W_Magic_Pose/Standing React Small From Front_normalized.glb"
)
const LOCOMOTION_BOB_SCRIPT: Script = preload("res://scripts/view/arcane_locomotion_bob.gd")

const CHARACTER_SCALE := 1.0
const RUN_SPEED := 1.2
const ATTACK_CAST_SPEED := 1.5
const RUN_BOB_CYCLES_PER_LOOP := 2.0
const MOVEMENT_EPSILON_SQUARED := 0.0001

const IDLE_ANIMATION: StringName = &"maria_idle"
const RUN_ANIMATION: StringName = &"maria_run"
const SHIELD_ANIMATION: StringName = &"maria_shield"
const ATTACK_ANIMATION: StringName = &"maria_attack"
const UNDER_ATTACK_ANIMATION: StringName = &"maria_under_attack"

var body: Node3D
var animation_player: AnimationPlayer
var locomotion_bob: Node
var movement := Vector2.ZERO
var state: int = State.IDLE


func initialize() -> void:
	scale = Vector3.ONE * CHARACTER_SCALE
	animation_player = _build_body()
	if animation_player == null:
		DebugLogger.log("[WARNING] [maria_character] animation player is unavailable")
		return
	animation_player.animation_finished.connect(_on_animation_finished)
	locomotion_bob = LOCOMOTION_BOB_SCRIPT.new()
	locomotion_bob.name = "LocomotionBob"
	add_child(locomotion_bob)
	locomotion_bob.initialize([body])
	locomotion_bob.set_run_frequency(_get_run_bob_frequency())
	_play_idle()


func set_movement(next_movement: Vector2) -> void:
	var normalized_movement := Vector2.ZERO
	if next_movement.length_squared() > MOVEMENT_EPSILON_SQUARED:
		normalized_movement = next_movement.normalized()
	movement = normalized_movement
	if state == State.SHIELD_CAST or state == State.ATTACK_CAST or state == State.UNDER_ATTACK:
		return
	var is_moving := normalized_movement.length_squared() > 0.0
	if is_moving:
		if state != State.RUN:
			_play_run()
	elif state != State.IDLE:
		_play_idle()


func play_w_cast() -> void:
	if not _play_one_shot(SHIELD_ANIMATION, State.SHIELD_CAST):
		DebugLogger.log("[WARNING] [maria_character] shield cast animation is unavailable")


func play_attack_cast() -> void:
	if not _play_one_shot(ATTACK_ANIMATION, State.ATTACK_CAST, ATTACK_CAST_SPEED):
		DebugLogger.log("[WARNING] [maria_character] attack animation is unavailable")


func play_under_attack() -> void:
	if not _play_one_shot(UNDER_ATTACK_ANIMATION, State.UNDER_ATTACK):
		DebugLogger.log("[WARNING] [maria_character] under-attack animation is unavailable")


func finish_cast() -> void:
	if state == State.SHIELD_CAST or state == State.ATTACK_CAST:
		_return_to_locomotion()


func reset() -> void:
	movement = Vector2.ZERO
	_play_idle()


func _build_body() -> AnimationPlayer:
	var model := MODEL_SCENE.instantiate() as Node3D
	if model == null:
		DebugLogger.log("[WARNING] [maria_character] failed to instantiate model")
		return null
	body = Node3D.new()
	body.name = "Body"
	add_child(body)
	model.name = "Model"
	body.add_child(model)
	var player := _find_animation_player(model)
	if player == null:
		DebugLogger.log("[WARNING] [maria_character] model animation player is missing")
		return null
	var library := _ensure_default_library(player)
	# The Maria clips ship as animation-only GLBs on a matching Mixamo skeleton,
	# so their track paths resolve against the model's own skeleton.
	_add_clip(library, IDLE_CLIP_SCENE, IDLE_ANIMATION, true)
	_add_clip(library, RUN_CLIP_SCENE, RUN_ANIMATION, true)
	_add_clip(library, SHIELD_CLIP_SCENE, SHIELD_ANIMATION, false)
	_add_clip(library, ATTACK_CLIP_SCENE, ATTACK_ANIMATION, false)
	_add_clip(library, UNDER_ATTACK_CLIP_SCENE, UNDER_ATTACK_ANIMATION, false)
	return player


func _find_animation_player(root: Node) -> AnimationPlayer:
	for node in root.find_children("*", "AnimationPlayer", true, false):
		return node as AnimationPlayer
	return null


func _ensure_default_library(player: AnimationPlayer) -> AnimationLibrary:
	if player.has_animation_library(&""):
		return player.get_animation_library(&"")
	var library := AnimationLibrary.new()
	player.add_animation_library(&"", library)
	return library


func _add_clip(
	library: AnimationLibrary,
	clip_scene: PackedScene,
	animation_name: StringName,
	should_loop: bool
) -> void:
	var clip_instance := clip_scene.instantiate() as Node
	if clip_instance == null:
		DebugLogger.log(
			"[WARNING] [maria_character] failed to instantiate clip=%s" % animation_name
		)
		return
	var animation := _extract_animation(clip_instance)
	if animation == null:
		DebugLogger.log("[WARNING] [maria_character] clip has no animation=%s" % animation_name)
		clip_instance.free()
		return
	animation.loop_mode = Animation.LOOP_LINEAR if should_loop else Animation.LOOP_NONE
	if library.has_animation(animation_name):
		library.remove_animation(animation_name)
	library.add_animation(animation_name, animation)
	clip_instance.free()


func _extract_animation(clip_instance: Node) -> Animation:
	var player := _find_animation_player(clip_instance)
	if player == null:
		return null
	var fallback: Animation = null
	for animation_name in player.get_animation_list():
		if String(animation_name).begins_with("RESET"):
			continue
		var animation := player.get_animation(animation_name)
		if animation == null:
			continue
		if String(animation_name) == "Action":
			return animation
		if fallback == null:
			fallback = animation
	return fallback


func _play_one_shot(
	animation_name: StringName, next_state: int, playback_speed: float = 1.0
) -> bool:
	if animation_player == null or not animation_player.has_animation(animation_name):
		return false
	if locomotion_bob:
		locomotion_bob.hide_all()
	state = next_state
	animation_player.play(animation_name, -1.0, playback_speed)
	return true


func _play_idle() -> void:
	state = State.IDLE
	if locomotion_bob:
		locomotion_bob.set_idle(body)
	if animation_player and animation_player.has_animation(IDLE_ANIMATION):
		animation_player.play(IDLE_ANIMATION)


func _play_run() -> void:
	state = State.RUN
	if locomotion_bob:
		locomotion_bob.set_running(body)
	if animation_player and animation_player.has_animation(RUN_ANIMATION):
		animation_player.play(RUN_ANIMATION, -1.0, RUN_SPEED)


func _return_to_locomotion() -> void:
	if locomotion_bob:
		locomotion_bob.hide_all()
	set_movement(movement)


func _on_animation_finished(animation_name: StringName) -> void:
	if animation_name == IDLE_ANIMATION or animation_name == RUN_ANIMATION:
		return
	if state == State.SHIELD_CAST or state == State.ATTACK_CAST or state == State.UNDER_ATTACK:
		_return_to_locomotion()


func _get_run_bob_frequency() -> float:
	if animation_player == null or not animation_player.has_animation(RUN_ANIMATION):
		return 2.0
	var animation := animation_player.get_animation(RUN_ANIMATION)
	if animation == null or animation.length <= 0.0:
		return 2.0
	return RUN_BOB_CYCLES_PER_LOOP * RUN_SPEED / animation.length
