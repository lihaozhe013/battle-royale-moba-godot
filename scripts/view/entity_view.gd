extends Node3D

class_name EntityView

var entity_id: int
var entity_type: int  # 0=Player, 1=Bot, 2=Arrow, 3=Pickup
var pickup_type: int  # 0=XP, 1=Heal, 2=SmallHeal

var _hp_bar: Node3D

func init(id: int, type: int, ptype: int = 0) -> void:
	entity_id = id
	entity_type = type
	pickup_type = ptype
	_create_mesh()

func _create_mesh() -> void:
	match entity_type:
		0:  # Player
			var m := MeshInstance3D.new()
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.8, 0.4, 0.8)
			var mat := StandardMaterial3D.new()
			mat.albedo_color = Color(0.2, 0.6, 1.0)
			m.mesh.surface_set_material(0, mat)
			add_child(m)
		1:  # Bot
			var m := MeshInstance3D.new()
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.8, 0.4, 0.8)
			var mat := StandardMaterial3D.new()
			mat.albedo_color = Color(1.0, 0.3, 0.3)
			m.mesh.surface_set_material(0, mat)
			add_child(m)
		2:  # Arrow
			var m := MeshInstance3D.new()
			var cyl := CylinderMesh.new()
			cyl.top_radius = 0.05
			cyl.bottom_radius = 0.05
			cyl.height = 0.5
			m.mesh = cyl
			var mat := StandardMaterial3D.new()
			mat.albedo_color = Color(1.0, 0.8, 0.0)
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.5
			add_child(m)
		3:  # Pickup
			var m := MeshInstance3D.new()
			m.mesh = BoxMesh.new()
			m.mesh.size = Vector3(0.4, 0.3, 0.4)
			var mat := StandardMaterial3D.new()
			match pickup_type:
				0: mat.albedo_color = Color(0.6, 0.2, 0.8)  # XP purple
				1: mat.albedo_color = Color(0.2, 1.0, 0.2)  # Heal green
				2: mat.albedo_color = Color(0.2, 0.8, 0.6)  # SmallHeal teal
			m.mesh.surface_set_material(0, mat)
			m.position.y = 0.3
			add_child(m)

func apply_snapshot(x: float, z: float, ang: float, hp: int, max_hp: int, dead: bool) -> void:
	visible = not dead
	if dead:
		return
	position = Vector3(x, 0, z)
	rotation = Vector3(0, ang, 0)
