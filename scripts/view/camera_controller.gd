extends Node3D
## 2.5D 相机：透视投影 + 55° 倾斜俯视，对照 Unity CameraController.cs
## 模式：父节点跟踪玩家位置，Camera3D 子节点固定局部偏移 + 固定旋转

@onready var _camera: Camera3D = $Camera3D

const HEIGHT: float = 30.0
const DISTANCE: float = 25.0
const FOV: float = 40.0
const TILT_DEG: float = 55.0
const FOLLOW_SPEED: float = 5.0
const SCROLL_SPEED: float = 5.0
const ZOOM_MIN: float = 5.0
const ZOOM_MAX: float = 100.0

var _height: float = HEIGHT
var _distance: float = DISTANCE
var _follow: bool = true
var _look_at: Vector3 = Vector3.ZERO
var _target_x: float = 0.0
var _target_z: float = 0.0
var _rotation_initialized: bool = false

var _dragging: bool = false
var _drag_start_mouse: Vector2 = Vector2.ZERO
var _drag_start_look_at: Vector3 = Vector3.ZERO

func _ready() -> void:
	_camera.projection = Camera3D.PROJECTION_PERSPECTIVE
	_camera.fov = FOV
	_camera.near = 0.1
	_camera.far = 500.0
	_camera.position = Vector3(0, _height, -_distance)

func follow_target(x: float, z: float) -> void:
	_target_x = x
	_target_z = z

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_Y:
		_follow = not _follow

	if event is InputEventMouseButton:
		match event.button_index:
			MOUSE_BUTTON_WHEEL_UP:
				_zoom(SCROLL_SPEED)
			MOUSE_BUTTON_WHEEL_DOWN:
				_zoom(-SCROLL_SPEED)
			MOUSE_BUTTON_MIDDLE:
				if event.pressed:
					_dragging = true
					_drag_start_mouse = event.position
					_drag_start_look_at = _look_at
				else:
					_dragging = false

	if event is InputEventMouseMotion and _dragging:
		var delta = event.position - _drag_start_mouse
		if delta.length() > 2.0:
			_follow = false
			var world_a = _screen_to_ground(_drag_start_mouse)
			var world_b = _screen_to_ground(event.position)
			_look_at = _drag_start_look_at - (world_b - world_a)

func _zoom(amount: float) -> void:
	var ratio = _height / _distance
	_height -= amount
	_height = clampf(_height, ZOOM_MIN, ZOOM_MAX)
	_distance = _height / ratio

func _process(delta: float) -> void:
	if not _rotation_initialized:
		_camera.look_at(global_position, Vector3.UP)
		_rotation_initialized = true

	if _follow:
		_look_at = Vector3(_target_x, 0, _target_z)
	position = position.lerp(_look_at, delta * FOLLOW_SPEED)
	_camera.position = Vector3(0, _height, -_distance)

func _screen_to_ground(screen_pos: Vector2) -> Vector3:
	var from = _camera.project_ray_origin(screen_pos)
	var dir = _camera.project_ray_normal(screen_pos)
	if absf(dir.y) > 0.001:
		var t = -from.y / dir.y
		return from + dir * t
	return Vector3.ZERO
