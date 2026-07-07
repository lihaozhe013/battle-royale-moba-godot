extends Node3D

# 施法线（ImmediateMesh）
var _cast_mesh: MeshInstance3D
var _cast_material: Material

# AoE 灰圈池
var _aoe_pool: Array[MeshInstance3D]

# Dash 路径线
var _dash_mesh: MeshInstance3D
var _dash_material: Material


func _ready() -> void:
	_cast_material = StandardMaterial3D.new()
	_cast_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_cast_material.albedo_color = Color(0.2, 1.0, 0.2, 0.6)

	_dash_material = StandardMaterial3D.new()
	_dash_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_dash_material.albedo_color = Color(0.3, 0.5, 1.0, 0.5)


func sync(snap: SimSnapshot, _player_view = null) -> void:
	if snap.players.size() == 0:
		_clear_cast_line()
		_clear_dash_line()
		_clear_aoes()
		return

	var p := snap.players[0] as SimPlayerSnap
	if not p:
		return

	# 1. 施法模式下绿线（Aiming）
	if p.cast_state == 1:  # Aiming
		_draw_cast_line(p.x, p.y, p.cast_aim_x, p.cast_aim_y)
	else:
		_clear_cast_line()

	# 2. Dash 路径（Dashing）
	if p.cast_state == 4:  # Dashing
		_draw_dash_path(p.dash_sx, p.dash_sy, p.x, p.y)
	else:
		_clear_dash_line()

	# 3. AoE 灰圈
	_sync_aoes(snap.aoes)


func _draw_cast_line(from_x: float, from_y: float, to_x: float, to_y: float) -> void:
	if not _cast_mesh:
		var mi := ImmediateMesh.new()
		_cast_mesh = MeshInstance3D.new()
		_cast_mesh.mesh = mi
		_cast_mesh.material_override = _cast_material
		add_child(_cast_mesh)

	var im := _cast_mesh.mesh as ImmediateMesh
	im.clear_surfaces()
	im.surface_begin(Mesh.PRIMITIVE_LINES, _cast_material)
	im.surface_add_vertex(Vector3(from_x, 0.05, from_y))
	im.surface_add_vertex(Vector3(to_x, 0.05, to_y))
	im.surface_end()


func _clear_cast_line() -> void:
	if _cast_mesh:
		var im := _cast_mesh.mesh as ImmediateMesh
		im.clear_surfaces()


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
