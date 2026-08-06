extends Node3D
class_name MeleeStrikeVfx

const BURST_COUNT := 3
const BURST_DURATION := 0.28
const BURST_GAP := 0.05
const FINAL_TAIL := 0.2
const BURST_SCALES: Array[Vector3] = [
	Vector3(1.0, 1.0, 1.0),
	Vector3(1.08, 1.12, 1.08),
	Vector3(0.96, 1.04, 0.96),
]

@onready var _lightning: Node3D = $Lightning
var _animation_player: AnimationPlayer
var _base_lightning_scale := Vector3.ONE


func _ready() -> void:
	_animation_player = (
		_lightning.get_node_or_null("AnimationPlayer") as AnimationPlayer
	)
	if not _animation_player:
		queue_free()
		return
	_base_lightning_scale = _lightning.scale
	_animation_player.play("RESET")
	_animation_player.seek(0.0, true)
	_play_burst_sequence.call_deferred()


func _play_burst_sequence() -> void:
	for burst_index in BURST_COUNT:
		if not is_instance_valid(_lightning):
			return
		_restart_particles()
		_lightning.scale = _base_lightning_scale * BURST_SCALES[burst_index]
		_animation_player.play("main")
		_animation_player.seek(0.0, true)
		await get_tree().create_timer(BURST_DURATION).timeout

		if burst_index < BURST_COUNT - 1:
			_animation_player.stop()
			_animation_player.play("RESET")
			_animation_player.seek(0.0, true)
			await get_tree().create_timer(BURST_GAP).timeout

	await get_tree().create_timer(FINAL_TAIL).timeout
	queue_free()


func _restart_particles() -> void:
	for child in _lightning.get_children():
		if child is GPUParticles3D:
			(child as GPUParticles3D).restart()
