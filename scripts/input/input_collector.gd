extends Node

var move_input := Vector2.ZERO
var aim_world := Vector2.ZERO
var fire := false
var input_seq := 0
var skill_q := false
var skill_w := false
var skill_e := false
var skill_r := false

func _process(_delta: float) -> void:
	input_seq += 1

	var h := 0.0
	var v := 0.0
	if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP):   v += 1
	if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN): v -= 1
	if Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT): h += 1
	if Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT):h -= 1

	var raw := Vector2(h, v)
	move_input = raw.normalized() if raw.length_squared() > 1.0 else raw

	fire = Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	skill_q = Input.is_key_pressed(KEY_C)
	skill_w = Input.is_key_pressed(KEY_E)
	skill_e = Input.is_key_pressed(KEY_R)
	skill_r = Input.is_key_pressed(KEY_F)

	var cam := get_viewport().get_camera_3d()
	if cam:
		var mouse_pos := get_viewport().get_mouse_position()
		var from := cam.project_ray_origin(mouse_pos)
		var dir := cam.project_ray_normal(mouse_pos)
		if abs(dir.y) > 0.001:
			var t := -from.y / dir.y
			var hit := from + dir * t
			aim_world = Vector2(hit.x, hit.z)
