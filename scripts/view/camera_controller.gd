extends Node3D

@onready var _camera: Camera3D = $Camera3D

var _target_x: float
var _target_z: float

func _ready() -> void:
	_camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	_camera.size = 40

func follow_target(x: float, z: float) -> void:
	_target_x = x
	_target_z = z

func _process(delta: float) -> void:
	var target := Vector3(_target_x, _camera.position.y, _target_z)
	_camera.position = _camera.position.lerp(target, delta * 5.0)
