extends Node3D
class_name MeleeStrikeVfx

const IMPACT_Y := 0.05
const HIT_FLASH_Y := 0.9
const PILLAR_HEIGHT := 9.0
const PILLAR_START_Y := 10.5
const PILLAR_IMPACT_Y := 4.0
const PILLAR_DROP_DURATION := 0.34
const PILLAR_HOLD_DURATION := 0.14
const PILLAR_FADE_DURATION := 0.35

@onready var _lightning: Node3D = $Lightning
var _impact_ring: MeshInstance3D
var _inner_ring: MeshInstance3D
var _hit_flash: MeshInstance3D
var _flash_light: OmniLight3D
var _pillar_root: Node3D
var _pillar_outer_material: StandardMaterial3D
var _pillar_core_material: StandardMaterial3D
var _impact_ring_material: StandardMaterial3D
var _inner_ring_material: StandardMaterial3D
var _hit_flash_material: StandardMaterial3D


func _ready() -> void:
	if not _lightning.has_method("play"):
		queue_free()
		return
	_create_impact_feedback()
	if _lightning.has_signal("finished"):
		_lightning.finished.connect(_on_lightning_finished)
	_lightning.play()


func _create_impact_feedback() -> void:
	var feedback_root := Node3D.new()
	feedback_root.name = "ImpactFeedback"
	add_child(feedback_root)

	_impact_ring_material = _make_emissive_material(
		Color(1.0, 0.12, 0.02, 0.95), 5.0
	)
	_impact_ring = _make_ring(
		"ImpactRing", 0.58, 0.7, _impact_ring_material
	)
	_impact_ring.position.y = IMPACT_Y
	_impact_ring.scale = Vector3(0.16, 0.8, 0.16)
	feedback_root.add_child(_impact_ring)

	_inner_ring_material = _make_emissive_material(
		Color(1.0, 0.58, 0.12, 0.9), 6.0
	)
	_inner_ring = _make_ring(
		"InnerRing", 0.25, 0.34, _inner_ring_material
	)
	_inner_ring.position.y = IMPACT_Y + 0.01
	_inner_ring.scale = Vector3(0.12, 0.8, 0.12)
	feedback_root.add_child(_inner_ring)

	_hit_flash_material = _make_emissive_material(
		Color(1.0, 0.3, 0.04, 0.9), 5.0
	)
	_hit_flash = MeshInstance3D.new()
	_hit_flash.name = "HitFlash"
	var flash_mesh := SphereMesh.new()
	flash_mesh.radius = 0.42
	flash_mesh.height = 0.84
	flash_mesh.radial_segments = 16
	flash_mesh.rings = 8
	_hit_flash.mesh = flash_mesh
	_hit_flash.material_override = _hit_flash_material
	_hit_flash.position.y = HIT_FLASH_Y
	_hit_flash.scale = Vector3(0.22, 0.35, 0.22)
	feedback_root.add_child(_hit_flash)

	_flash_light = OmniLight3D.new()
	_flash_light.name = "ImpactFlashLight"
	_flash_light.light_color = Color(1.0, 0.08, 0.015, 1.0)
	_flash_light.light_energy = 3.5
	_flash_light.omni_range = 4.0
	_flash_light.position.y = 0.7
	feedback_root.add_child(_flash_light)

	_add_impact_sparks(feedback_root)
	_add_sky_pillar(feedback_root)
	_animate_impact_feedback()


func _add_sky_pillar(feedback_root: Node3D) -> void:
	_pillar_root = Node3D.new()
	_pillar_root.name = "SkyPillar"
	_pillar_root.position.y = PILLAR_START_Y
	feedback_root.add_child(_pillar_root)

	_pillar_outer_material = _make_beam_material(
		Color(1.0, 0.01, 0.005, 0.34), 5.0
	)
	var outer_beam := _make_beam(
		"OuterBeam", 0.14, PILLAR_HEIGHT, _pillar_outer_material
	)
	_pillar_root.add_child(outer_beam)

	_pillar_core_material = _make_beam_material(
		Color(1.0, 0.22, 0.04, 1.0), 10.0
	)
	var core_beam := _make_beam(
		"CoreBeam", 0.045, PILLAR_HEIGHT, _pillar_core_material
	)
	_pillar_root.add_child(core_beam)

	var pillar_light := OmniLight3D.new()
	pillar_light.name = "PillarLight"
	pillar_light.light_color = Color(1.0, 0.03, 0.01, 1.0)
	pillar_light.light_energy = 2.2
	pillar_light.omni_range = 3.5
	pillar_light.position.y = -PILLAR_HEIGHT * 0.5 + 0.4
	_pillar_root.add_child(pillar_light)

	_animate_sky_pillar(pillar_light)


func _make_beam(
	beam_name: String,
	radius: float,
	height: float,
	material: StandardMaterial3D
) -> MeshInstance3D:
	var beam := MeshInstance3D.new()
	beam.name = beam_name
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.radial_segments = 16
	mesh.cap_top = false
	mesh.cap_bottom = false
	beam.mesh = mesh
	beam.material_override = material
	return beam


func _make_beam_material(
	color: Color, energy: float
) -> StandardMaterial3D:
	var material := _make_emissive_material(color, energy)
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material


func _animate_sky_pillar(pillar_light: OmniLight3D) -> void:
	var drop_tween := create_tween()
	drop_tween.tween_property(
		_pillar_root,
		"position:y",
		PILLAR_IMPACT_Y,
		PILLAR_DROP_DURATION
	).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)

	var pulse_tween := create_tween()
	pulse_tween.tween_interval(PILLAR_DROP_DURATION)
	pulse_tween.tween_property(
		_pillar_root,
		"scale",
		Vector3(1.2, 1.0, 1.2),
		0.06
	).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	pulse_tween.tween_property(
		_pillar_root,
		"scale",
		Vector3.ONE,
		0.2
	).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)

	var fade_tween := create_tween()
	fade_tween.tween_interval(PILLAR_DROP_DURATION + PILLAR_HOLD_DURATION)
	fade_tween.set_parallel(true)
	fade_tween.tween_property(
		_pillar_outer_material,
		"albedo_color",
		Color(1.0, 0.01, 0.005, 0.0),
		PILLAR_FADE_DURATION
	)
	fade_tween.tween_property(
		_pillar_core_material,
		"albedo_color",
		Color(1.0, 0.22, 0.04, 0.0),
		PILLAR_FADE_DURATION
	)
	fade_tween.tween_property(
		_pillar_outer_material,
		"emission_energy_multiplier",
		0.0,
		PILLAR_FADE_DURATION
	)
	fade_tween.tween_property(
		_pillar_core_material,
		"emission_energy_multiplier",
		0.0,
		PILLAR_FADE_DURATION
	)
	fade_tween.tween_property(
		pillar_light,
		"light_energy",
		0.0,
		PILLAR_FADE_DURATION
	)


func _make_ring(
	ring_name: String,
	inner_radius: float,
	outer_radius: float,
	material: StandardMaterial3D
) -> MeshInstance3D:
	var ring := MeshInstance3D.new()
	ring.name = ring_name
	var mesh := TorusMesh.new()
	mesh.inner_radius = inner_radius
	mesh.outer_radius = outer_radius
	mesh.rings = 32
	mesh.ring_segments = 8
	ring.mesh = mesh
	ring.material_override = material
	return ring


func _make_emissive_material(
	color: Color, energy: float
) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = color
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
	return material


func _add_impact_sparks(feedback_root: Node3D) -> void:
	var sparks := GPUParticles3D.new()
	sparks.name = "ImpactSparks"
	sparks.amount = 18
	sparks.lifetime = 0.48
	sparks.one_shot = true
	sparks.explosiveness = 1.0
	sparks.randomness = 0.35
	sparks.local_coords = true
	sparks.visibility_aabb = AABB(
		Vector3(-3.0, -1.0, -3.0), Vector3(6.0, 5.0, 6.0)
	)
	sparks.position.y = IMPACT_Y

	var process_material := ParticleProcessMaterial.new()
	process_material.direction = Vector3(0.0, 1.0, 0.0)
	process_material.spread = 78.0
	process_material.initial_velocity_min = 2.0
	process_material.initial_velocity_max = 4.2
	process_material.gravity = Vector3(0.0, -8.0, 0.0)
	process_material.scale_min = 0.06
	process_material.scale_max = 0.14
	sparks.process_material = process_material

	var spark_mesh := SphereMesh.new()
	spark_mesh.radius = 0.06
	spark_mesh.height = 0.12
	spark_mesh.radial_segments = 4
	spark_mesh.rings = 2
	spark_mesh.material = _make_emissive_material(
		Color(1.0, 0.48, 0.08, 1.0), 7.0
	)
	sparks.draw_pass_1 = spark_mesh
	feedback_root.add_child(sparks)
	sparks.emitting = true


func _animate_impact_feedback() -> void:
	var ring_tween := create_tween().set_parallel(true)
	ring_tween.tween_property(
		_impact_ring,
		"scale",
		Vector3(1.9, 0.8, 1.9),
		0.55
	).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	ring_tween.tween_property(
		_impact_ring_material,
		"albedo_color",
		Color(1.0, 0.12, 0.02, 0.0),
		0.55
	)

	var inner_tween := create_tween().set_parallel(true)
	inner_tween.tween_property(
		_inner_ring,
		"scale",
		Vector3(2.5, 0.8, 2.5),
		0.36
	).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	inner_tween.tween_property(
		_inner_ring_material,
		"albedo_color",
		Color(1.0, 0.58, 0.12, 0.0),
		0.36
	)

	var flash_tween := create_tween()
	flash_tween.tween_property(
		_hit_flash,
		"scale",
		Vector3(1.35, 1.5, 1.35),
		0.08
	).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	flash_tween.tween_property(
		_hit_flash,
		"scale",
		Vector3(0.65, 0.8, 0.65),
		0.4
	).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
	var flash_fade_tween := create_tween()
	flash_fade_tween.tween_interval(0.08)
	flash_fade_tween.tween_property(
		_hit_flash_material,
		"albedo_color",
		Color(1.0, 0.3, 0.04, 0.0),
		0.4
	)

	var light_tween := create_tween()
	light_tween.tween_property(
		_flash_light, "light_energy", 0.0, 0.48
	).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)


func _on_lightning_finished() -> void:
	queue_free()
