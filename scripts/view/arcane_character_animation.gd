extends Node3D

enum State {
	IDLE,
	RUN_BEGIN,
	RUN_LOOP,
	RUN_END,
	SHIELD_CAST,
	ATTACK_CAST,
	UNDER_ATTACK,
	HIDDEN,
}

const IDLE_SCENE: PackedScene = preload("res://resources/characters/arcane_duel/idle3.glb")
const RUN_BEGIN_SCENE: PackedScene = preload(
	"res://resources/characters/arcane_duel/run_2_begin_character.glb"
)
const RUN_LOOP_SCENE: PackedScene = preload(
	"res://resources/characters/arcane_duel/run_2_loop_character.glb"
)
const RUN_END_SCENE: PackedScene = preload(
	"res://resources/characters/arcane_duel/run_2_end_character.glb"
)
const SHIELD_SCENE: PackedScene = preload(
	"res://resources/characters/arcane_duel/shield_1_character.glb"
)
const FIREBALL_CAST_SCENE: PackedScene = preload(
	"res://resources/characters/arcane_duel/fireball_cast_character.glb"
)
const UNDER_ATTACK_SCENE: PackedScene = preload(
	"res://resources/characters/arcane_duel/under_attack_1_character.glb"
)
const LOCOMOTION_BOB_SCRIPT: Script = preload("res://scripts/view/arcane_locomotion_bob.gd")

const IDLE_ANIMATION: StringName = &"idle3_processed"
const RUN_BEGIN_ANIMATION: StringName = &"run_2_begin"
const RUN_LOOP_ANIMATION: StringName = &"run_2_loop"
const RUN_END_ANIMATION: StringName = &"run_2_end"
const SHIELD_ANIMATION: StringName = &"shield_1"
const FIREBALL_CAST_ANIMATION: StringName = &"fireball_cast"
const UNDER_ATTACK_ANIMATION: StringName = &"under_attack_1"

const CHARACTER_SCALE := 2.0
const RUN_BEGIN_SPEED := 5.0
const RUN_LOOP_SPEED := 1.2
const RUN_END_SPEED := 10.0
const RUN_BOB_CYCLES_PER_LOOP := 2.0
const MOVEMENT_EPSILON_SQUARED := 0.0001
const SHIELD_CLIP_SPLIT_RATIO := 0.5
const FIREBALL_CAST_SPEED := 5.0

var idle_body: Node3D
var run_begin_body: Node3D
var run_loop_body: Node3D
var run_end_body: Node3D
var shield_body: Node3D
var fireball_cast_body: Node3D
var under_attack_body: Node3D
var idle_animation_player: AnimationPlayer
var run_begin_animation_player: AnimationPlayer
var run_loop_animation_player: AnimationPlayer
var run_end_animation_player: AnimationPlayer
var shield_animation_player: AnimationPlayer
var fireball_cast_animation_player: AnimationPlayer
var under_attack_animation_player: AnimationPlayer
var locomotion_bob: Node
var idle_animation_name: StringName = IDLE_ANIMATION
var run_begin_animation_name: StringName = RUN_BEGIN_ANIMATION
var run_loop_animation_name: StringName = RUN_LOOP_ANIMATION
var run_end_animation_name: StringName = RUN_END_ANIMATION
var shield_animation_name: StringName = SHIELD_ANIMATION
var fireball_cast_animation_name: StringName = FIREBALL_CAST_ANIMATION
var under_attack_animation_name: StringName = UNDER_ATTACK_ANIMATION
var movement := Vector2.ZERO
var state: int = State.IDLE


func initialize() -> void:
	scale = Vector3.ONE * CHARACTER_SCALE
	idle_animation_player = _add_animation_body(
		IDLE_SCENE, "IdleBody", IDLE_ANIMATION, "idle", true
	)
	run_begin_animation_player = _add_animation_body(
		RUN_BEGIN_SCENE, "RunBeginBody", RUN_BEGIN_ANIMATION, "run_begin", false
	)
	run_loop_animation_player = _add_animation_body(
		RUN_LOOP_SCENE, "RunLoopBody", RUN_LOOP_ANIMATION, "run_loop", true
	)
	run_end_animation_player = _add_animation_body(
		RUN_END_SCENE, "RunEndBody", RUN_END_ANIMATION, "run_end", false
	)
	shield_animation_player = _add_animation_body(
		SHIELD_SCENE, "ShieldBody", SHIELD_ANIMATION, "shield", false
	)
	fireball_cast_animation_player = _add_animation_body(
		FIREBALL_CAST_SCENE, "FireballCastBody", FIREBALL_CAST_ANIMATION, "fireball_cast", false
	)
	under_attack_animation_player = _add_animation_body(
		UNDER_ATTACK_SCENE, "UnderAttackBody", UNDER_ATTACK_ANIMATION, "under_attack", false
	)
	locomotion_bob = LOCOMOTION_BOB_SCRIPT.new()
	locomotion_bob.name = "LocomotionBob"
	add_child(locomotion_bob)
	locomotion_bob.initialize([idle_body, run_begin_body, run_loop_body, run_end_body])
	locomotion_bob.set_run_frequency(_get_run_bob_frequency())
	if run_begin_animation_player:
		run_begin_animation_player.animation_finished.connect(_on_animation_finished)
	if run_end_animation_player:
		run_end_animation_player.animation_finished.connect(_on_animation_finished)
	if shield_animation_player:
		shield_animation_player.animation_finished.connect(_on_animation_finished)
	if fireball_cast_animation_player:
		fireball_cast_animation_player.animation_finished.connect(_on_animation_finished)
	if under_attack_animation_player:
		under_attack_animation_player.animation_finished.connect(_on_animation_finished)
	_play_idle()


func set_movement(next_movement: Vector2) -> void:
	var normalized_movement := Vector2.ZERO
	if next_movement.length_squared() > MOVEMENT_EPSILON_SQUARED:
		normalized_movement = next_movement.normalized()

	var was_moving := movement.length_squared() > MOVEMENT_EPSILON_SQUARED
	var is_moving := normalized_movement.length_squared() > 0.0
	movement = normalized_movement

	if state == State.SHIELD_CAST or state == State.ATTACK_CAST or state == State.UNDER_ATTACK:
		return
	if is_moving:
		if state == State.IDLE or state == State.RUN_END or state == State.HIDDEN:
			_play_run_begin()
		return
	if was_moving and (state == State.RUN_BEGIN or state == State.RUN_LOOP):
		_play_run_end()
	elif state == State.HIDDEN:
		_play_idle()


func play_w_cast() -> void:
	if shield_body == null or shield_animation_player == null:
		DebugLogger.log("[WARNING] [arcane_character] shield cast animation is unavailable")
		return
	_hide_all()
	shield_body.visible = true
	state = State.SHIELD_CAST
	shield_animation_player.play(shield_animation_name)
	var animation := shield_animation_player.get_animation(shield_animation_name)
	if animation:
		shield_animation_player.seek(animation.length * SHIELD_CLIP_SPLIT_RATIO, true)


func play_attack_cast() -> void:
	if fireball_cast_body == null or fireball_cast_animation_player == null:
		DebugLogger.log("[WARNING] [arcane_character] fireball cast animation is unavailable")
		return
	_hide_all()
	fireball_cast_body.visible = true
	state = State.ATTACK_CAST
	fireball_cast_animation_player.play(fireball_cast_animation_name, -1.0, FIREBALL_CAST_SPEED)


func play_under_attack() -> void:
	if under_attack_body == null or under_attack_animation_player == null:
		DebugLogger.log("[WARNING] [arcane_character] under-attack animation is unavailable")
		return
	_hide_all()
	under_attack_body.visible = true
	state = State.UNDER_ATTACK
	under_attack_animation_player.play(under_attack_animation_name)


func finish_cast() -> void:
	if state == State.SHIELD_CAST or state == State.ATTACK_CAST:
		_return_to_locomotion()


func reset() -> void:
	movement = Vector2.ZERO
	_play_idle()


func _return_to_locomotion() -> void:
	_hide_all()
	set_movement(movement)


func _play_idle() -> void:
	if idle_body == null or idle_animation_player == null:
		DebugLogger.log("[WARNING] [arcane_character] idle animation is unavailable")
		return
	_hide_all()
	idle_body.visible = true
	state = State.IDLE
	locomotion_bob.set_idle(idle_body)
	idle_animation_player.play(idle_animation_name)


func _play_run_begin() -> void:
	if run_begin_body == null or run_begin_animation_player == null:
		_play_run_loop()
		return
	_hide_all()
	run_begin_body.visible = true
	state = State.RUN_BEGIN
	locomotion_bob.set_running(run_begin_body)
	run_begin_animation_player.play(run_begin_animation_name, -1.0, RUN_BEGIN_SPEED)


func _play_run_loop() -> void:
	if run_loop_body == null or run_loop_animation_player == null:
		_play_idle()
		return
	_hide_all()
	run_loop_body.visible = true
	state = State.RUN_LOOP
	locomotion_bob.set_running(run_loop_body)
	run_loop_animation_player.play(run_loop_animation_name, -1.0, RUN_LOOP_SPEED)


func _play_run_end() -> void:
	if run_end_body == null or run_end_animation_player == null:
		_play_idle()
		return
	_hide_all()
	run_end_body.visible = true
	state = State.RUN_END
	locomotion_bob.set_running(run_end_body)
	run_end_animation_player.play(run_end_animation_name, -1.0, RUN_END_SPEED)


func _hide_all() -> void:
	if locomotion_bob:
		locomotion_bob.hide_all()
	for player in [
		idle_animation_player,
		run_begin_animation_player,
		run_loop_animation_player,
		run_end_animation_player,
		shield_animation_player,
		fireball_cast_animation_player,
		under_attack_animation_player,
	]:
		if player:
			player.stop()
	for body in [
		idle_body,
		run_begin_body,
		run_loop_body,
		run_end_body,
		shield_body,
		fireball_cast_body,
		under_attack_body,
	]:
		if body:
			body.visible = false
	state = State.HIDDEN


func _on_animation_finished(animation_name: StringName) -> void:
	if animation_name == run_begin_animation_name and state == State.RUN_BEGIN:
		if movement.length_squared() > MOVEMENT_EPSILON_SQUARED:
			_play_run_loop()
		else:
			_play_run_end()
	elif animation_name == run_end_animation_name and state == State.RUN_END:
		if movement.length_squared() > MOVEMENT_EPSILON_SQUARED:
			_play_run_begin()
		else:
			_play_idle()
	elif animation_name == shield_animation_name and state == State.SHIELD_CAST:
		_return_to_locomotion()
	elif animation_name == fireball_cast_animation_name and state == State.ATTACK_CAST:
		_return_to_locomotion()
	elif animation_name == under_attack_animation_name and state == State.UNDER_ATTACK:
		_return_to_locomotion()


func _add_animation_body(
	scene: PackedScene,
	body_name: String,
	animation_name: StringName,
	label: String,
	should_loop: bool
) -> AnimationPlayer:
	var instance := scene.instantiate() as Node3D
	if instance == null:
		DebugLogger.log("[WARNING] [arcane_character] failed to instantiate %s" % label)
		return null
	var body_wrapper := Node3D.new()
	body_wrapper.name = body_name
	body_wrapper.visible = false
	add_child(body_wrapper)
	instance.name = "Model"
	instance.visible = true
	body_wrapper.add_child(instance)
	match label:
		"idle":
			idle_body = body_wrapper
		"run_begin":
			run_begin_body = body_wrapper
		"run_loop":
			run_loop_body = body_wrapper
		"run_end":
			run_end_body = body_wrapper
		"shield":
			shield_body = body_wrapper
		"fireball_cast":
			fireball_cast_body = body_wrapper
		"under_attack":
			under_attack_body = body_wrapper
	for node in instance.find_children("*", "AnimationPlayer", true, false):
		var player := node as AnimationPlayer
		if player == null:
			continue
		var resolved_animation_name := _resolve_animation_name(player, animation_name)
		if resolved_animation_name.is_empty():
			continue
		_set_animation_name(label, resolved_animation_name)
		var animation := player.get_animation(resolved_animation_name)
		if animation:
			animation.loop_mode = (Animation.LOOP_LINEAR if should_loop else Animation.LOOP_NONE)
			return player
	DebugLogger.log(
		"[WARNING] [arcane_character] missing animation=%s asset=%s"
		% [animation_name, label]
	)
	return null


func _get_run_bob_frequency() -> float:
	if run_loop_animation_player == null:
		return 2.0
	var animation := run_loop_animation_player.get_animation(run_loop_animation_name)
	if animation == null or animation.length <= 0.0:
		return 2.0
	return RUN_BOB_CYCLES_PER_LOOP * RUN_LOOP_SPEED / animation.length


func _resolve_animation_name(player: AnimationPlayer, requested_name: StringName) -> StringName:
	if player.has_animation(requested_name):
		return requested_name

	var requested_text := String(requested_name)
	for available_name in player.get_animation_list():
		var available_text := String(available_name)
		if available_text.ends_with(requested_text):
			return StringName(available_text)

	var available := player.get_animation_list()
	if available.size() == 1 and String(available[0]) != "RESET":
		return StringName(available[0])
	return &""


func _set_animation_name(label: String, animation_name: StringName) -> void:
	match label:
		"idle":
			idle_animation_name = animation_name
		"run_begin":
			run_begin_animation_name = animation_name
		"run_loop":
			run_loop_animation_name = animation_name
		"run_end":
			run_end_animation_name = animation_name
		"shield":
			shield_animation_name = animation_name
		"fireball_cast":
			fireball_cast_animation_name = animation_name
		"under_attack":
			under_attack_animation_name = animation_name
