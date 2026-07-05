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

var _anim_player: AnimationPlayer = null
var _anim_idle := ""
var _anim_run := ""
var _moving := false

func init(id: int, type: int, ptype: int = 0) -> void:
	entity_id = id
	entity_type = type
	pickup_type = ptype

func _ready() -> void:
	_anim_player = find_child("AnimationPlayer", true, false) as AnimationPlayer
	if not _anim_player:
		return
	_anim_idle = _find_anim_path("idle")
	_anim_run = _find_anim_path("run")
	if _anim_idle != "":
		_anim_player.play(_anim_idle)

func _find_anim_path(anim_name: String) -> String:
	if not _anim_player:
		return ""
	for lib_name in _anim_player.get_animation_library_list():
		var lib = _anim_player.get_animation_library(lib_name)
		if lib and lib.has_animation(anim_name):
			return anim_name if lib_name == "" else lib_name + "/" + anim_name
	return ""

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
		_moving = false
		return

	_prev_pos = _curr_pos
	_curr_pos = new_pos
	_prev_ang = _curr_ang
	_curr_ang = new_ang
	_snap_time = Time.get_ticks_msec() / 1000.0

	_moving = _curr_pos.distance_to(_prev_pos) > 0.01

func _process(_delta: float) -> void:
	if _first_snap:
		return
	var elapsed := Time.get_ticks_msec() / 1000.0 - _snap_time
	var t := clampf(elapsed / LERP_DURATION, 0.0, 1.0)
	position = _prev_pos.lerp(_curr_pos, t)
	rotation = Vector3(0, lerp_angle(_prev_ang, _curr_ang, t), 0)

	if _anim_player and _anim_run != "" and _anim_idle != "":
		var target := _anim_run if _moving else _anim_idle
		if _anim_player.current_animation != target:
			_anim_player.play(target)

func _create_fallback_mesh(type: int, ptype: int) -> void:
	var m = MeshInstance3D.new()
	match type:
		0:
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.8, 1.5, 0.8)
			var mat = StandardMaterial3D.new()
			mat.albedo_color = Color(0.2, 0.6, 1.0)
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.75
		1:
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.8, 1.5, 0.8)
			var mat = StandardMaterial3D.new()
			mat.albedo_color = Color(1.0, 0.3, 0.3)
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.75
		2:
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
		3:
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.5, 0.5, 0.5)
			var mat = StandardMaterial3D.new()
			match ptype:
				0: mat.albedo_color = Color(0.6, 0.2, 0.8)
				1: mat.albedo_color = Color(0.2, 1.0, 0.2)
				2: mat.albedo_color = Color(0.2, 0.8, 0.6)
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.5
	add_child(m)
