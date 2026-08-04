class_name WorldBootstrap
extends Node

var _initialized := false


func _ready() -> void:
	initialize()


func initialize() -> void:
	if _initialized:
		return
	_initialized = true
	_create_directional_light()
	_create_ground()
	_create_environment()


func _create_directional_light() -> void:
	var light := DirectionalLight3D.new()
	light.name = "DirectionalLight3D"
	light.shadow_enabled = true
	var basis := Basis(
		Vector3(0.70710677, 0.49999997, -0.49999997),
		Vector3(0.0, 0.70710677, 0.70710677),
		Vector3(0.70710677, -0.49999997, 0.49999997)
	)
	light.transform = Transform3D(basis, Vector3.ZERO)
	add_child(light)


func _create_ground() -> void:
	var ground := MeshInstance3D.new()
	ground.name = "Ground"
	var mesh := PlaneMesh.new()
	mesh.size = Vector2(100, 100)
	ground.mesh = mesh
	ground.material_override = StandardMaterial3D.new()
	add_child(ground)


func _create_environment() -> void:
	var environment_node := WorldEnvironment.new()
	environment_node.name = "WorldEnvironment"
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color(0.1, 0.1, 0.12, 1)
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color(0.6416358, 0.6417704, 0.68589425, 1)
	environment_node.environment = environment
	add_child(environment_node)
