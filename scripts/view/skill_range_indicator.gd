extends Node3D

const RING_Y := 0.06
const RING_COLOR := Color(0.227, 0.227, 0.227, 0.85)
const RING_OUTER_RADIUS := 1.0
const RING_INNER_RADIUS := 0.997

var _ring: MeshInstance3D
var _initialized := false


func _ready() -> void:
	initialize()


func initialize() -> void:
	if _initialized:
		return
	_initialized = true

	var mesh := TorusMesh.new()
	mesh.inner_radius = RING_INNER_RADIUS
	mesh.outer_radius = RING_OUTER_RADIUS
	mesh.rings = 48
	mesh.ring_segments = 8

	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = RING_COLOR
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA

	_ring = MeshInstance3D.new()
	_ring.mesh = mesh
	_ring.material_override = material
	_ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_ring.visible = false
	add_child(_ring)


func sync_range(
	center_position: Vector2, range_value: float, targeting_mode: bool
) -> void:
	initialize()

	var should_show := targeting_mode and range_value > 0.0
	_ring.visible = should_show
	if not should_show:
		return

	_ring.position = Vector3(center_position.x, RING_Y, center_position.y)
	var scale := range_value / RING_OUTER_RADIUS
	_ring.scale = Vector3(scale, 1.0, scale)
