extends Node3D

var _ring: MeshInstance3D
var _ring_material: Material
var _alpha := 0.0
var _pos := Vector3.ZERO
var _active := false


func _ready() -> void:
	var cyl := CylinderMesh.new()
	cyl.top_radius = 0.5
	cyl.bottom_radius = 0.5
	cyl.height = 0.05

	_ring_material = StandardMaterial3D.new()
	_ring_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_ring_material.albedo_color = Color(0.3, 0.6, 1.0, 0.6)
	_ring_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA

	_ring = MeshInstance3D.new()
	_ring.mesh = cyl
	_ring.material_override = _ring_material
	add_child(_ring)
	_ring.visible = false

	# Listen for move commands
	if get_parent() and get_parent().has_node("InputCollector"):
		get_parent().get_node("InputCollector").move_issued.connect(_on_move_issued)


func _on_move_issued(target: Vector2) -> void:
	_pos = Vector3(target.x, 0.025, target.y)
	_alpha = 0.6
	_active = true
	_ring.position = _pos
	_ring.visible = true


func _process(delta: float) -> void:
	if not _active:
		return
	_alpha -= delta * 1.2
	if _alpha <= 0.0:
		_alpha = 0.0
		_active = false
		_ring.visible = false
		return
	var mat := _ring_material as StandardMaterial3D
	mat.albedo_color = Color(0.3, 0.6, 1.0, _alpha)
	var s := 1.0 + (1.0 - _alpha / 0.6) * 0.5
	_ring.scale = Vector3(s, 1.0, s)
