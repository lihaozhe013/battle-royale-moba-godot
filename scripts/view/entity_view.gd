extends Node3D

class_name EntityView

var entity_id: int
var entity_type: int  # 0=Player, 1=Bot, 2=Arrow, 3=Pickup
var pickup_type: int  # 0=XP, 1=Heal, 2=SmallHeal

var _prev_pos := Vector3.ZERO
var _curr_pos := Vector3.ZERO
var _prev_ang := 0.0
var _curr_ang := 0.0
var _snap_time := 0.0
var _first_snap := true
const LERP_DURATION := 1.0 / 20.0

func init(id: int, type: int, ptype: int = 0) -> void:
	entity_id = id
	entity_type = type
	pickup_type = ptype
	_create_mesh()

func _create_mesh() -> void:
	match entity_type:
		0:  # Player — 蓝色方块，高度 1.5
			var m = MeshInstance3D.new()
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.8, 1.5, 0.8)
			var mat = StandardMaterial3D.new()
			mat.albedo_color = Color(0.2, 0.6, 1.0)
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.75
			add_child(m)
		1:  # Bot — 红色方块，高度 1.5
			var m = MeshInstance3D.new()
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.8, 1.5, 0.8)
			var mat = StandardMaterial3D.new()
			mat.albedo_color = Color(1.0, 0.3, 0.3)
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.75
			add_child(m)
		2:  # Arrow — 黄色圆柱，沿 X 轴躺平
			var m = MeshInstance3D.new()
			var cyl = CylinderMesh.new()
			cyl.top_radius = 0.05
			cyl.bottom_radius = 0.05
			cyl.height = 0.6
			m.mesh = cyl
			var mat = StandardMaterial3D.new()
			mat.albedo_color = Color(1.0, 0.8, 0.0)
			m.mesh.surface_set_material(0, mat)
			m.rotation = Vector3(0, 0, -PI / 2)
			m.position.y = 0.8
			add_child(m)
		3:  # Pickup — 彩色小方块
			var m = MeshInstance3D.new()
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.5, 0.5, 0.5)
			var mat = StandardMaterial3D.new()
			match pickup_type:
				0: mat.albedo_color = Color(0.6, 0.2, 0.8)  # XP purple
				1: mat.albedo_color = Color(0.2, 1.0, 0.2)  # Heal green
				2: mat.albedo_color = Color(0.2, 0.8, 0.6)  # SmallHeal teal
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.5
			add_child(m)

func apply_snapshot(x: float, z: float, ang: float, hp: int, max_hp: int, dead: bool) -> void:
	if dead:
		visible = false
		return

	if not visible:
		_first_snap = true

	visible = true

	var new_pos := Vector3(x, 0, z)
	var new_ang := ang

	if _first_snap:
		_prev_pos = new_pos
		_curr_pos = new_pos
		_prev_ang = new_ang
		_curr_ang = new_ang
		position = new_pos
		rotation = Vector3(0, ang, 0)
		_first_snap = false
		_snap_time = Time.get_ticks_msec() / 1000.0
		return

	_prev_pos = _curr_pos
	_curr_pos = new_pos
	_prev_ang = _curr_ang
	_curr_ang = new_ang
	_snap_time = Time.get_ticks_msec() / 1000.0

func _process(_delta: float) -> void:
	if _first_snap:
		return
	var elapsed := Time.get_ticks_msec() / 1000.0 - _snap_time
	var t := clampf(elapsed / LERP_DURATION, 0.0, 1.0)
	position = _prev_pos.lerp(_curr_pos, t)
	rotation = Vector3(0, lerp_angle(_prev_ang, _curr_ang, t), 0)
