extends Node

const IDLE_AMPLITUDE := 0.035
const RUN_AMPLITUDE := 0.085
const IDLE_FREQUENCY := 1.25
const DEFAULT_RUN_FREQUENCY := 2.0
const AMPLITUDE_BLEND_SPEED := 5.0

var bodies: Array[Node3D] = []
var base_y_positions: Array[float] = []
var active_body: Node3D
var target_amplitude := 0.0
var amplitude := 0.0
var frequency := IDLE_FREQUENCY
var run_frequency := DEFAULT_RUN_FREQUENCY
var phase := 0.0


func initialize(next_bodies: Array) -> void:
	for body in next_bodies:
		if body == null or not is_instance_valid(body):
			continue
		bodies.append(body)
		base_y_positions.append(body.position.y)


func set_run_frequency(next_frequency: float) -> void:
	run_frequency = maxf(next_frequency, 0.1)


func set_idle(body: Node3D) -> void:
	_activate(body, IDLE_AMPLITUDE, IDLE_FREQUENCY)


func set_running(body: Node3D) -> void:
	_activate(body, RUN_AMPLITUDE, run_frequency)


func hide_all() -> void:
	active_body = null
	target_amplitude = 0.0
	_clear_offsets()


func _process(delta: float) -> void:
	amplitude = move_toward(amplitude, target_amplitude, AMPLITUDE_BLEND_SPEED * delta)
	if frequency > 0.0:
		phase = fmod(phase + TAU * frequency * delta, TAU)
	_clear_offsets()
	if active_body == null or not is_instance_valid(active_body):
		return
	var body_index := bodies.find(active_body)
	if body_index < 0:
		return
	active_body.position.y = base_y_positions[body_index] + sin(phase) * amplitude


func _activate(body: Node3D, next_amplitude: float, next_frequency: float) -> void:
	active_body = body
	target_amplitude = next_amplitude
	frequency = next_frequency


func _clear_offsets() -> void:
	for index in range(bodies.size()):
		var body := bodies[index]
		if body and is_instance_valid(body):
			body.position.y = base_y_positions[index]
