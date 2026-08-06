extends Node3D

const RANGE_INDICATOR_SCRIPT = preload(
	"res://scripts/view/skill_range_indicator.gd"
)
const MELEE_STRIKE_VFX_SCENE: PackedScene = preload(
	"res://resources/vfx/skills/melee_strike/melee_strike_vfx.tscn"
)
const SKILL_VFX_SCENES := {
	1: MELEE_STRIKE_VFX_SCENE,
}

# AoE 灰圈池
var _aoe_pool: Array[MeshInstance3D]

# Dash 路径线
var _dash_mesh: MeshInstance3D
var _dash_material: Material
var _range_indicator: Node3D
var _initialized := false
var _last_vfx_snapshot_seq := -1
var _previous_cast_state := 0
var _previous_cast_slot := -1


func _ready() -> void:
	_initialize()


func _initialize() -> void:
	if _initialized:
		return
	_initialized = true
	_dash_material = StandardMaterial3D.new()
	_dash_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_dash_material.albedo_color = Color(0.3, 0.5, 1.0, 0.5)
	_range_indicator = Node3D.new()
	_range_indicator.name = "SkillRangeIndicator"
	_range_indicator.set_script(RANGE_INDICATOR_SCRIPT)
	add_child(_range_indicator)
	_range_indicator.initialize()


func sync(snap: SimSnapshot, entity_manager = null, input_fsm = null) -> void:
	_initialize()
	var p = null

	if snap.heroes.size() > 0:
		var local_idx = -1
		if snap.has_method("get_local_hero_index"):
			local_idx = snap.get_local_hero_index()
		if local_idx >= 0 and local_idx < snap.heroes.size():
			p = snap.heroes[local_idx]
	elif snap.players.size() > 0:
		p = snap.players[0]

	if not p:
		_clear_dash_line()
		_clear_aoes()
		_range_indicator.sync_range(Vector2.ZERO, 0.0, false)
		return

	_sync_cast_vfx(snap, p, entity_manager)

	_sync_targeting_range(p, input_fsm)

	if p.cast_state == 4:
		_draw_dash_path(p.dash_sx, p.dash_sy, p.x, p.y)
	else:
		_clear_dash_line()

	_sync_aoes(snap.aoes)


func _sync_cast_vfx(snap: SimSnapshot, p, entity_manager) -> void:
	if snap.seq == _last_vfx_snapshot_seq:
		return
	_last_vfx_snapshot_seq = snap.seq

	if (
		_previous_cast_state != 0
		and p.cast_state == 0
		and p.hit_target_id >= 0
		and entity_manager
	):
		var skill_id := _get_skill_id(p, _previous_cast_slot)
		_play_skill_vfx(skill_id, p.hit_target_id, entity_manager)

	_previous_cast_state = p.cast_state
	_previous_cast_slot = p.cast_slot


func _get_skill_id(p, cast_slot: int) -> int:
	if cast_slot < 0 or cast_slot >= p.skills.size():
		return -1
	return p.skills[cast_slot].skill_id


func _play_skill_vfx(skill_id: int, target_id: int, entity_manager) -> void:
	var effect_scene: PackedScene = SKILL_VFX_SCENES.get(skill_id)
	if not effect_scene:
		return
	var target_view = entity_manager.get_entity(target_id)
	if not target_view or not is_instance_valid(target_view):
		return
	var attachment = target_view.skill_vfx_attachment
	if not attachment or not is_instance_valid(attachment):
		return
	var effect := effect_scene.instantiate()
	if effect:
		attachment.add_child(effect)


func _sync_targeting_range(p, input_fsm) -> void:
	var cast_mode := false
	var active_slot := -1
	if input_fsm and input_fsm.has_method("is_targeting_mode"):
		cast_mode = input_fsm.is_targeting_mode()
		if (
			input_fsm.command_axis
			== InputStateMachine.CommandAxis.ATTACK_AIMING
		):
			_range_indicator.sync_range(
				Vector2(p.x, p.y), p.attack_range, cast_mode
			)
			return
		if input_fsm.command_axis == InputStateMachine.CommandAxis.SKILL_AIMING:
			active_slot = input_fsm.active_skill_slot
		elif (
			input_fsm.command_axis == InputStateMachine.CommandAxis.CAST_LOCKED
		):
			active_slot = p.cast_slot
	else:
		cast_mode = p.cast_state != 0
		active_slot = p.cast_slot

	var cast_range := 0.0
	if active_slot >= 0 and active_slot < p.skills.size():
		cast_range = p.skills[active_slot].cast_range
	_range_indicator.sync_range(Vector2(p.x, p.y), cast_range, cast_mode)


func _draw_dash_path(sx: float, sy: float, cx: float, cy: float) -> void:
	if not _dash_mesh:
		var mi := ImmediateMesh.new()
		_dash_mesh = MeshInstance3D.new()
		_dash_mesh.mesh = mi
		_dash_mesh.material_override = _dash_material
		add_child(_dash_mesh)

	var im := _dash_mesh.mesh as ImmediateMesh
	im.clear_surfaces()
	im.surface_begin(Mesh.PRIMITIVE_LINES, _dash_material)
	im.surface_add_vertex(Vector3(sx, 0.05, sy))
	im.surface_add_vertex(Vector3(cx, 0.05, cy))
	im.surface_end()


func _clear_dash_line() -> void:
	if _dash_mesh:
		var im := _dash_mesh.mesh as ImmediateMesh
		im.clear_surfaces()


func _sync_aoes(aoe_snaps) -> void:
	# Grow pool if needed
	while _aoe_pool.size() < aoe_snaps.size():
		var m := MeshInstance3D.new()
		var cyl := CylinderMesh.new()
		cyl.top_radius = 1.0
		cyl.bottom_radius = 1.0
		cyl.height = 0.1
		var mat := StandardMaterial3D.new()
		mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		mat.albedo_color = Color(0.5, 0.5, 0.5, 0.25)
		mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		m.mesh = cyl
		m.material_override = mat
		add_child(m)
		_aoe_pool.append(m)

	# Hide excess
	for i in _aoe_pool.size():
		_aoe_pool[i].visible = i < aoe_snaps.size()

	# Position visible ones
	for i in aoe_snaps.size():
		var a = aoe_snaps[i]
		var m = _aoe_pool[i]
		var cyl := m.mesh as CylinderMesh
		cyl.top_radius = a.radius
		cyl.bottom_radius = a.radius
		# Alpha fade based on remaining time
		var alpha := 0.25
		if a.remaining < 0.5:
			alpha = 0.25 * (a.remaining / 0.5)
		var mat := m.material_override as StandardMaterial3D
		mat.albedo_color = Color(0.5, 0.5, 0.5, alpha)
		m.position = Vector3(a.x, 0.025, a.y)


func _clear_aoes() -> void:
	for m in _aoe_pool:
		m.visible = false
