extends Node3D

const RANGE_INDICATOR_SCRIPT = preload(
	"res://scripts/view/skill_range_indicator.gd"
)

# AoE 灰圈池
var _aoe_pool: Array[MeshInstance3D]

# Dash 路径线
var _dash_mesh: MeshInstance3D
var _dash_material: Material
var _range_indicator: Node3D
var _initialized := false

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


func sync(snap: SimSnapshot, _player_view = null, input_fsm = null) -> void:
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

	_sync_targeting_range(p, input_fsm)

	if p.cast_state == 4:
		_draw_dash_path(p.dash_sx, p.dash_sy, p.x, p.y)
	else:
		_clear_dash_line()

	_sync_aoes(snap.aoes)


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
		elif input_fsm.command_axis == InputStateMachine.CommandAxis.CAST_LOCKED:
			active_slot = p.cast_slot
	else:
		cast_mode = p.cast_state != 0
		active_slot = p.cast_slot

	var cast_range := 0.0
	if active_slot >= 0 and active_slot < p.skills.size():
		cast_range = p.skills[active_slot].cast_range
	_range_indicator.sync_range(
		Vector2(p.x, p.y), cast_range, cast_mode
	)


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
