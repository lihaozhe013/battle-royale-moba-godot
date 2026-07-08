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
const LERP_DURATION := 1.0 / 30.0

var _anim_player: AnimationPlayer = null
var _anim_idle := ""
var _anim_run := ""
var _moving := false

# 受击红闪
var _prev_hp := -1
var _flash_timer := 0.0
var _red_mat: Material
var _child_meshes: Array[MeshInstance3D]
var _dead := false

var skill_vfx_attachment: Node3D

const SKILL_VFX_ATTACHMENT_SCRIPT := preload("res://scripts/view/skill_vfx_attachment.gd")

# Sim uses 2D math angles: atan2(y, x) where 0=+x, π/2=+y.
# Godot rotation.y rotates +X toward -Z (not +Z), so we negate to fix the Z flip.
# Model faces +Z at rest, so offset by +π/2 to align +X as the zero-angle reference.
const MODEL_FACING_OFFSET := PI / 2.0

static func sim_to_godot_yaw(sim_ang: float) -> float:
	return -sim_ang + MODEL_FACING_OFFSET

func init(id: int, type: int, ptype: int = 0) -> void:
	entity_id = id
	entity_type = type
	pickup_type = ptype

func _ready() -> void:
	_red_mat = StandardMaterial3D.new()
	_red_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_red_mat.albedo_color = Color(1.0, 0.0, 0.0, 0.6)
	_red_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	for child in find_children("*", "MeshInstance3D", true, false):
		_child_meshes.append(child as MeshInstance3D)

	_anim_player = find_child("AnimationPlayer", true, false) as AnimationPlayer
	if _anim_player:
		_anim_idle = _find_anim_path("idle")
		_anim_run = _find_anim_path("run")
		if _anim_idle != "":
			_anim_player.play(_anim_idle)

	skill_vfx_attachment = Node3D.new()
	skill_vfx_attachment.name = "SkillVfxAttachment"
	skill_vfx_attachment.set_script(SKILL_VFX_ATTACHMENT_SCRIPT)
	add_child(skill_vfx_attachment)

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
		if not _dead:
			_dead = true
			for m in _child_meshes:
				m.visible = false
		return

	if _dead:
		_dead = false
		_first_snap = true
		_prev_hp = hp
		for m in _child_meshes:
			m.visible = true

	# 受击红闪检测
	if entity_type == 0 or entity_type == 1:
		if hp < _prev_hp:
			_flash_timer = 0.2
		_prev_hp = hp

	visible = true

	var new_pos := Vector3(x, 0, z)
	var new_ang := ang

	if _first_snap:
		_prev_pos = new_pos
		_curr_pos = new_pos
		_prev_ang = new_ang
		_curr_ang = new_ang
		position = new_pos
		rotation = Vector3(0, sim_to_godot_yaw(ang), 0)
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

func _process(delta: float) -> void:
	if _first_snap:
		return
	var elapsed := Time.get_ticks_msec() / 1000.0 - _snap_time
	var t := clampf(elapsed / LERP_DURATION, 0.0, 1.0)
	position = _prev_pos.lerp(_curr_pos, t)
	rotation = Vector3(0, sim_to_godot_yaw(lerp_angle(_prev_ang, _curr_ang, t)), 0)

	# 受击红闪
	if _flash_timer > 0.0:
		_flash_timer -= delta
		for m in _child_meshes:
			m.material_override = _red_mat
		if _flash_timer <= 0.0:
			for m in _child_meshes:
				m.material_override = null

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
