class_name SkillVfxAttachment
extends Node3D


func show_c_slash() -> void:
	var root := Node3D.new()
	root.position = Vector3(0, 0.05, 0)
	add_child(root)

	var gcyl := CylinderMesh.new()
	gcyl.top_radius = 0.4
	gcyl.bottom_radius = 0.4
	gcyl.height = 80.0
	var gmat := StandardMaterial3D.new()
	gmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	gmat.albedo_color = Color(0.2, 0.4, 0.9, 0.0)
	gmat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	var glow := MeshInstance3D.new()
	glow.mesh = gcyl
	glow.material_override = gmat
	glow.position = Vector3(0, 40.0, 0)
	root.add_child(glow)

	var cyl := CylinderMesh.new()
	cyl.top_radius = 0.15
	cyl.bottom_radius = 0.15
	cyl.height = 80.0
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = Color(0.1, 0.4, 0.9, 0.0)
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	var beam := MeshInstance3D.new()
	beam.mesh = cyl
	beam.material_override = mat
	beam.position = Vector3(0, 40.0, 0)
	root.add_child(beam)

	var tw := create_tween()
	tw.tween_property(mat, "albedo_color:a", 0.85, 0.1)
	tw.parallel().tween_property(gmat, "albedo_color:a", 0.25, 0.1)
	tw.tween_property(mat, "albedo_color:a", 0.0, 0.2).set_delay(1.0)
	tw.parallel().tween_property(gmat, "albedo_color:a", 0.0, 0.2)
	tw.tween_callback(root.queue_free)
