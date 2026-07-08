extends Node3D
## 2.5D 相机：透视投影 + 55° 倾斜俯视，对照 Unity CameraController.cs
## 模式：父节点跟踪玩家位置，Camera3D 子节点固定局部偏移 + 固定旋转
## 中键拖屏恒为像素精准模式
## 平滑拖屏开关：打开时 position 用 lerp 缓动，关闭时直接跳转

@onready var _camera: Camera3D = $Camera3D

const HEIGHT: float = 30.0
const DISTANCE: float = 25.0
const FOV: float = 40.0
const TILT_DEG: float = 55.0
const FOLLOW_SPEED: float = 5.0
const SCROLL_SPEED: float = 5.0
const ZOOM_MIN: float = 5.0
const ZOOM_MAX: float = 100.0
const EDGE_PAN_PIXEL: float = 8.0

const CAM_LOCKED := 0
const CAM_FREE := 1
const LERP_DURATION := 1.0 / 30.0

var _height: float = HEIGHT
var _distance: float = DISTANCE
var _mode: int = CAM_LOCKED
var _edge_pan: bool = false
var _edge_pan_speed: float = 14.0
var _smooth_pan: bool = true
var _look_at: Vector3 = Vector3.ZERO
var _target_x: float = 0.0
var _target_z: float = 0.0
var _interp_prev: Vector3 = Vector3.ZERO
var _interp_curr: Vector3 = Vector3.ZERO
var _interp_time: float = 0.0
var _needs_center_snap: bool = true
var _rotation_initialized: bool = false

var _center_held: bool = false
var _dragging: bool = false
var _drag_start_mouse: Vector2 = Vector2.ZERO
var _drag_start_look_at: Vector3 = Vector3.ZERO


func _ready() -> void:
	_camera.projection = Camera3D.PROJECTION_PERSPECTIVE
	_camera.fov = FOV
	_camera.near = 0.1
	_camera.far = 500.0
	_camera.position = Vector3(0, _height, -_distance)

	_mode = GameSettings.camera_mode
	_edge_pan = GameSettings.edge_pan
	_edge_pan_speed = GameSettings.edge_pan_speed
	_smooth_pan = GameSettings.smooth_pan
	GameSettings.camera_mode_changed.connect(_on_camera_mode_changed)
	GameSettings.edge_pan_changed.connect(_on_edge_pan_changed)
	GameSettings.edge_pan_speed_changed.connect(_on_edge_pan_speed_changed)
	GameSettings.smooth_pan_changed.connect(_on_smooth_pan_changed)


func _on_camera_mode_changed(m: int) -> void:
	_mode = m
	if m == CAM_LOCKED:
		_needs_center_snap = true


func _on_edge_pan_changed(on: bool) -> void:
	_edge_pan = on


func _on_edge_pan_speed_changed(v: float) -> void:
	_edge_pan_speed = v


func _on_smooth_pan_changed(on: bool) -> void:
	_smooth_pan = on


func follow_target(x: float, z: float) -> void:
	if not _needs_center_snap and absf(_target_x - x) < 0.001 and absf(_target_z - z) < 0.001:
		return
	_target_x = x
	_target_z = z
	_interp_prev = _interp_curr
	_interp_curr = Vector3(x, 0, z)
	_interp_time = Time.get_ticks_msec() / 1000.0
	if _needs_center_snap:
		_interp_prev = _interp_curr
		_needs_center_snap = false


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_Y:
		GameSettings.camera_mode = (_mode + 1) % 2

	if event is InputEventKey and (event.keycode == KEY_F1 or event.keycode == KEY_SPACE) and GameSettings.move_mode == GameSettings.MoveMode.MOBA and not event.echo:
		if event.pressed:
			if _mode != CAM_LOCKED:
				GameSettings.camera_mode = CAM_LOCKED
				_center_held = true
		else:
			if _center_held:
				GameSettings.camera_mode = CAM_FREE
				_center_held = false

	if event is InputEventMouseButton:
		match event.button_index:
			MOUSE_BUTTON_WHEEL_UP:
				_zoom(SCROLL_SPEED)
			MOUSE_BUTTON_WHEEL_DOWN:
				_zoom(-SCROLL_SPEED)
			MOUSE_BUTTON_MIDDLE:
				if event.pressed:
					if _mode == CAM_LOCKED:
						GameSettings.camera_mode = CAM_FREE
					_dragging = true
					_drag_start_mouse = event.position
					_drag_start_look_at = _look_at
				else:
					_dragging = false

	if event is InputEventMouseMotion and _dragging:
		var delta: Vector2 = event.position - _drag_start_mouse
		if delta.length() > 2.0:
			var ratio: Vector2 = _world_per_pixel_ratio()
			_look_at = _drag_start_look_at + Vector3(delta.x * ratio.x, 0, delta.y * ratio.y)


func _zoom(amount: float) -> void:
	var ratio: float = _height / _distance
	_height -= amount
	_height = clampf(_height, ZOOM_MIN, ZOOM_MAX)
	_distance = _height / ratio


func _process(delta: float) -> void:
	if not _rotation_initialized:
		_camera.look_at(global_position, Vector3.UP)
		_rotation_initialized = true

	if _mode == CAM_LOCKED:
		var elapsed := Time.get_ticks_msec() / 1000.0 - _interp_time
		var t := clampf(elapsed / LERP_DURATION, 0.0, 1.0)
		_look_at = _interp_prev.lerp(_interp_curr, t)

	if _edge_pan and _ok_for_edge_pan():
		var mp: Vector2 = get_viewport().get_mouse_position()
		var sz: Vector2 = get_viewport().size
		var push: Vector2 = Vector2.ZERO
		if mp.x <= EDGE_PAN_PIXEL:
			push.x += 1.0
		elif mp.x >= sz.x - EDGE_PAN_PIXEL:
			push.x -= 1.0
		if mp.y <= EDGE_PAN_PIXEL:
			push.y += 1.0
		elif mp.y >= sz.y - EDGE_PAN_PIXEL:
			push.y -= 1.0
		if push != Vector2.ZERO:
			if _mode == CAM_LOCKED:
				GameSettings.camera_mode = CAM_FREE
			var spd: float = _edge_pan_speed * (_height / HEIGHT)
			_look_at += Vector3(push.x * spd * delta, 0, push.y * spd * delta)

	if _smooth_pan:
		position = position.lerp(_look_at, delta * FOLLOW_SPEED)
	else:
		position = _look_at
	_camera.position = Vector3(0, _height, -_distance)


func _world_per_pixel_ratio() -> Vector2:
	var vp: Vector2 = get_viewport().size
	var tan_h: float = tan(deg_to_rad(FOV * 0.5))
	var wpix_v: float = 2.0 * _height * tan_h / vp.y
	return Vector2(wpix_v, wpix_v)


func _ok_for_edge_pan() -> bool:
	if _dragging:
		return false
	var mode: int = DisplayServer.window_get_mode()
	return mode in [
		DisplayServer.WINDOW_MODE_FULLSCREEN,
		DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN,
		DisplayServer.WINDOW_MODE_MAXIMIZED,
	]
