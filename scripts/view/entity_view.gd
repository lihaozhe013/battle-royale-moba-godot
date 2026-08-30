extends Node3D

class_name EntityView

var entity_id: int
var entity_type: int  # 0=Player, 1=Bot, 2=Arrow, 3=Pickup
var pickup_type: int  # 0=XP, 1=Heal, 2=SmallHeal
var hero_prefab_id: int = 0

var _prev_pos := Vector3.ZERO
var _curr_pos := Vector3.ZERO
var _prev_ang := 0.0
var _curr_ang := 0.0
var _snap_time := 0.0
var _first_snap := true
const LERP_DURATION := 1.0 / 30.0

var _moving := false
var _previous_cast_state := 0
var _previous_cast_slot := -1
var _character_animation

# 受击红闪
var _prev_hp := -1
var _flash_timer := 0.0
var _red_mat: Material
var _child_meshes: Array[MeshInstance3D]
var _dead := false

# 悬停高亮
var _hovered := false
var _highlight_mat: Material

# 攻击锁定指示器
var _attack_targeted := false
var _attack_target_mat: Material

var skill_vfx_attachment: Node3D

const SKILL_VFX_ATTACHMENT_SCRIPT := preload("res://scripts/view/skill_vfx_attachment.gd")

# Sim uses 2D math angles: atan2(y, x) where 0=+x, π/2=+y.
# Godot rotation.y rotates +X toward -Z (not +Z), so we negate to fix the Z flip.
# Model faces +Z at rest, so offset by +π/2 to align +X as the zero-angle reference.
const MODEL_FACING_OFFSET := PI / 2.0


static func sim_to_godot_yaw(sim_ang: float) -> float:
	return -sim_ang + MODEL_FACING_OFFSET


func init(id: int, type: int, ptype: int = 0, prefab_id: int = 0) -> void:
	entity_id = id
	entity_type = type
	pickup_type = ptype
	hero_prefab_id = prefab_id


func _ready() -> void:
	_red_mat = StandardMaterial3D.new()
	_red_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_red_mat.albedo_color = Color(1.0, 0.0, 0.0, 0.6)
	_red_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA

	_highlight_mat = StandardMaterial3D.new()
	_highlight_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_highlight_mat.albedo_color = Color(1.0, 0.9, 0.4, 0.35)
	_highlight_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA

	_attack_target_mat = StandardMaterial3D.new()
	_attack_target_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_attack_target_mat.albedo_color = Color(1.0, 0.25, 0.25, 0.45)
	_attack_target_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA

	for child in find_children("*", "MeshInstance3D", true, false):
		_child_meshes.append(child as MeshInstance3D)

	if entity_type == 0 or entity_type == 1:
		var animation_script: Script = HeroVisualCatalog.animation_script(hero_prefab_id)
		_character_animation = animation_script.new()
		_character_animation.name = "HeroCharacterAnimation"
		add_child(_character_animation)
		_character_animation.initialize()
		_child_meshes.clear()
		for child in find_children("*", "MeshInstance3D", true, false):
			_child_meshes.append(child as MeshInstance3D)
	else:
		for child in find_children("*", "GPUParticles3D", true, false):
			var particles := child as GPUParticles3D
			particles.emitting = true
			particles.restart()

	skill_vfx_attachment = Node3D.new()
	skill_vfx_attachment.name = "SkillVfxAttachment"
	skill_vfx_attachment.set_script(SKILL_VFX_ATTACHMENT_SCRIPT)
	add_child(skill_vfx_attachment)


func apply_snapshot(
	x: float,
	z: float,
	ang: float,
	hp: int,
	max_hp: int,
	dead: bool,
	cast_state: int = 0,
	cast_slot: int = -1,
	cast_moving: bool = false
) -> void:
	if dead:
		if not _dead:
			_dead = true
			_previous_cast_state = 0
			_previous_cast_slot = -1
			for m in _child_meshes:
				m.visible = false
		return

	if _dead:
		_dead = false
		_first_snap = true
		_prev_hp = hp
		for m in _child_meshes:
			m.visible = true
		if _character_animation:
			_character_animation.reset()

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
		rotation = Vector3(0, _entity_yaw(ang), 0)
		_first_snap = false
		_snap_time = Time.get_ticks_msec() / 1000.0
		_moving = cast_moving
		_sync_character_animation(cast_state, cast_slot)
		return

	_prev_pos = _curr_pos
	_curr_pos = new_pos
	_prev_ang = _curr_ang
	_curr_ang = new_ang
	_snap_time = Time.get_ticks_msec() / 1000.0

	_moving = cast_moving or _curr_pos.distance_to(_prev_pos) > 0.01
	_sync_character_animation(cast_state, cast_slot)


func _process(delta: float) -> void:
	if _first_snap:
		return
	var elapsed := Time.get_ticks_msec() / 1000.0 - _snap_time
	var t := clampf(elapsed / LERP_DURATION, 0.0, 1.0)
	position = _prev_pos.lerp(_curr_pos, t)
	rotation = Vector3(0, _entity_yaw(lerp_angle(_prev_ang, _curr_ang, t)), 0)

	# 材质优先级：受击红闪 > 攻击锁定(红) > 悬停高亮(黄) > 无
	if _flash_timer > 0.0:
		_flash_timer -= delta
		for m in _child_meshes:
			m.material_override = _red_mat
		if _flash_timer <= 0.0:
			var mat = (
				_attack_target_mat if _attack_targeted else (_highlight_mat if _hovered else null)
			)
			for m in _child_meshes:
				m.material_override = mat
	else:
		var mat = _attack_target_mat if _attack_targeted else (_highlight_mat if _hovered else null)
		for m in _child_meshes:
			m.material_override = mat

	if _character_animation:
		_character_animation.set_movement(_movement_vector())


func _sync_character_animation(cast_state: int, cast_slot: int) -> void:
	if _character_animation == null:
		return
	_character_animation.set_movement(_movement_vector())
	if (
		cast_state == 3
		and cast_slot == 1
		and (_previous_cast_state != 3 or _previous_cast_slot != 1)
	):
		_character_animation.play_w_cast()
	elif cast_state == 0 and _previous_cast_state != 0:
		_character_animation.finish_cast()
	_previous_cast_state = cast_state
	_previous_cast_slot = cast_slot


func _movement_vector() -> Vector2:
	return Vector2.RIGHT if _moving else Vector2.ZERO


func play_attack_cast() -> void:
	if _dead or _character_animation == null:
		return
	_character_animation.play_attack_cast()


func play_under_attack() -> void:
	if _dead or _character_animation == null:
		return
	_character_animation.play_under_attack()


func _entity_yaw(sim_ang: float) -> float:
	if entity_type == 2:
		return -sim_ang - PI / 2.0
	return sim_to_godot_yaw(sim_ang)


func set_hovered(v: bool) -> void:
	_hovered = v


func set_attack_targeted(v: bool) -> void:
	_attack_targeted = v


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
			mat.albedo_color = Color(0.2, 0.6, 1.0)
			m.mesh.surface_set_material(0, mat)
			m.rotation = Vector3(0, 0, -PI / 2)
			m.position.y = 0.8
		3:
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.5, 0.5, 0.5)
			var mat = StandardMaterial3D.new()
			match ptype:
				0:
					mat.albedo_color = Color(0.6, 0.2, 0.8)
				1:
					mat.albedo_color = Color(0.2, 1.0, 0.2)
				2:
					mat.albedo_color = Color(0.2, 0.8, 0.6)
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.5
	add_child(m)
