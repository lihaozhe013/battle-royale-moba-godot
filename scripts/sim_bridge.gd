extends Node

var sim: SimServer
var last_snapshot: SimSnapshot
var elapsed: float = 0.0
var _last_snap_seq := -1
var _prev_player_cast_state := 0
var _prev_player_cast_slot := -1
var _prev_player_cast_error := 0

@onready var camera_controller = $CameraController
@onready var entity_manager = $EntityManager
@onready var health_bar_manager = $HealthBarManager
@onready var bottom_hud = $BottomHUD
@onready var cast_bar_layer = $CastBarLayer
@onready var cast_error_layer = $CastErrorLayer
var input_event_queue: InputEventQueue
var input_state_machine: InputStateMachine
var command_buffer: CommandBuffer
var command_builder: CommandBuilder
var cast_settings: CastSettings
var _skill_vfx: Node3D

const HOVER_RADIUS := 2.0
const TICK_RATE := 1.0 / 30.0

# Temp adapter state (translates Command → new Sim API)
var _tmp_move_target := Vector2.ZERO
var _tmp_move_issue := false
var _tmp_stop := false
var _tmp_cast_slot := -1
var _tmp_cast_confirm := false
var _tmp_cast_aim := Vector2.ZERO
var _tmp_cast_target_id := -1
var _tmp_upgrade_slot := -1
var _tmp_cancel_skill := false
var _tmp_cancel_attack := false
var _tmp_attack_target_id := -1
var _tmp_attack_ground := false
var _tmp_attack_ground_pos := Vector2.ZERO
var _tmp_attack_clear := false
var _tmp_seq := 0


func _ready() -> void:
	var file = FileAccess.open("res://data/maps/default.json", FileAccess.READ)
	if not file:
		push_error("Failed to load map JSON")
		return
	var map_json = file.get_as_text()
	file.close()

	# Auto-create input layer nodes
	input_event_queue = _ensure_node("InputEventQueue", &"res://scripts/input/input_event_queue.gd")
	input_state_machine = _ensure_node("InputStateMachine", &"res://scripts/input/input_state_machine.gd")
	command_buffer = _ensure_node("CommandBuffer", &"res://scripts/input/command_buffer.gd")
	command_builder = _ensure_node("CommandBuilder", &"res://scripts/input/command_builder.gd")
	cast_settings = _ensure_node("CastSettings", &"res://scripts/input/cast_settings.gd")
	command_builder.setup(input_event_queue, input_state_machine, command_buffer, cast_settings)

	_spawn_wall_visuals(map_json)

	health_bar_manager.entity_manager = entity_manager
	health_bar_manager.health_bar_scene = preload("res://scenes/ui/health_bar_ui.tscn")

	sim = SimServer.new()
	sim.initialize(map_json)
	print("SimServer initialized")

	_skill_vfx = $SkillVFX if has_node("SkillVFX") else Node3D.new()
	_skill_vfx.name = "SkillVFX"
	if not _skill_vfx.get_parent():
		add_child(_skill_vfx)
	var vfx_script = preload("res://scripts/view/skill_vfx.gd")
	if not _skill_vfx.get_script():
		_skill_vfx.set_script(vfx_script)


func _ensure_node(name: String, script_path: StringName) -> Node:
	var n = get_node_or_null(name)
	if not n:
		n = Node.new()
		n.name = name
		n.set_script(load(script_path))
		add_child(n)
	return n


func _spawn_wall_visuals(json_text: String) -> void:
	var data = JSON.parse_string(json_text) as Dictionary
	if not data or not data.has("walls"):
		return
	var wall_material = StandardMaterial3D.new()
	wall_material.albedo_color = Color(0.4, 0.4, 0.45)
	for w in data["walls"]:
		var min_x = minf(w["minX"], w["maxX"])
		var max_x = maxf(w["minX"], w["maxX"])
		var min_y = minf(w["minY"], w["maxY"])
		var max_y = maxf(w["minY"], w["maxY"])
		var center = Vector3((min_x + max_x) * 0.5, 0.5, (min_y + max_y) * 0.5)
		var size = Vector3(max_x - min_x, 1.0, max_y - min_y)

		var m = MeshInstance3D.new()
		m.mesh = BoxMesh.new()
		m.mesh.size = size
		m.mesh.surface_set_material(0, wall_material)
		m.position = center
		add_child(m)


var _frame_tick_index := 0
var _log_prev_cast_slot := -1
var _log_prev_cast_state := 0


func _physics_process(delta: float) -> void:
	_frame_tick_index = 0
	elapsed += delta
	var ran_tick := false

	# Consume and translate commands each tick
	while elapsed >= TICK_RATE:
		_frame_tick_index += 1
		ran_tick = true
		var first_tick := _frame_tick_index == 1

		if first_tick:
			command_builder.process_frame()

		var cmds := command_buffer.pop_all()
		var merged := command_buffer.merge_commands(cmds)

		for c in merged:
			_apply_command(c)

		# New Sim API
		sim.set_skill_command(_tmp_cast_slot, _tmp_cast_confirm, _tmp_cast_aim.x, _tmp_cast_aim.y, _tmp_cast_target_id)
		if _tmp_upgrade_slot >= 0:
			sim.set_skill_upgrade_command(_tmp_upgrade_slot)
		sim.set_attack_command_full(_tmp_attack_target_id, _tmp_attack_ground, _tmp_attack_ground_pos.x, _tmp_attack_ground_pos.y, _tmp_attack_clear)
		if _tmp_cancel_skill:
			sim.set_cancel_command(true, false)
		if _tmp_cancel_attack:
			sim.set_cancel_command(false, true)
		sim.set_move_command(_tmp_move_target.x, _tmp_move_target.y, _tmp_move_issue and first_tick)
		if _tmp_stop and first_tick:
			sim.set_stop_command()
		sim.tick(TICK_RATE)

		# Clear pulse fields
		if first_tick:
			_tmp_move_issue = false
			_tmp_stop = false
		_tmp_cast_confirm = false
		_tmp_upgrade_slot = -1
		_tmp_cancel_skill = false
		_tmp_cancel_attack = false
		_tmp_attack_target_id = -1
		_tmp_attack_ground = false
		_tmp_attack_clear = false

		elapsed -= TICK_RATE
		if sim.is_game_over():
			print("=== GAME OVER ===")
			get_tree().paused = true
			return
		var snap = sim.pop_snapshot()
		if snap is SimSnapshot:
			last_snapshot = snap

	if ran_tick and last_snapshot and last_snapshot.players.size() > 0:
		input_state_machine.sync_from_snapshot(last_snapshot.players[0])


func _apply_command(c: Command) -> void:
	match c.type:
		Command.CmdType.MOVE:
			_tmp_move_target = c.move_target
			_tmp_move_issue = true
		Command.CmdType.SKILL:
			_tmp_cast_slot = c.skill_slot
			_tmp_cast_aim = c.skill_aim
			_tmp_cast_target_id = c.skill_target_id
			if c.skill_confirm:
				_tmp_cast_confirm = true
		Command.CmdType.SKILL_UPGRADE:
			_tmp_upgrade_slot = c.skill_slot
		Command.CmdType.ATTACK:
			_tmp_attack_target_id = c.attack_target_id
			if c.attack_ground.length_squared() > 0.001:
				_tmp_attack_ground = true
				_tmp_attack_ground_pos = c.attack_ground
		Command.CmdType.CANCEL:
			if c.cancel_scope == 0:
				_tmp_cancel_skill = true
			elif c.cancel_scope == 1:
				_tmp_cancel_attack = true
			else:
				_tmp_cancel_skill = true
				_tmp_cancel_attack = true
		Command.CmdType.STOP:
			_tmp_stop = true


func _process(_delta: float) -> void:
	if not last_snapshot:
		return

	var aim: Vector2 = input_event_queue.mouse_world

	# Hover detection
	var hover_id := -1
	var hover_sq := HOVER_RADIUS * HOVER_RADIUS
	for b in last_snapshot.bots:
		if b.dead:
			continue
		var d_sq := Vector2(b.x, b.y).distance_squared_to(aim)
		if d_sq < hover_sq:
			hover_sq = d_sq
			hover_id = b.id
	entity_manager.set_hover_id(hover_id)

	if last_snapshot.seq != _last_snap_seq:
		_last_snap_seq = last_snapshot.seq
		entity_manager.sync_entities(last_snapshot)
		health_bar_manager.sync_bars(last_snapshot)
		if last_snapshot.players.size() > 0:
			var p := last_snapshot.players[0] as SimPlayerSnap
			if p:
				entity_manager.set_attack_target_id(p.attack_target_id)

				if _prev_player_cast_state == 2 and p.cast_state == 0 and _prev_player_cast_slot == 0:
					if p.hit_target_id >= 0:
						_trigger_c_slash(p.hit_target_id)
				var prev_state := _prev_player_cast_state
				_prev_player_cast_state = p.cast_state
				_prev_player_cast_slot = p.cast_slot
				if prev_state >= 1 and p.cast_state == 0:
					if p.cast_error > 0 and p.cast_error != _prev_player_cast_error:
						cast_error_layer.show_error(p.cast_error)
				_prev_player_cast_error = p.cast_error

				# Sync FSM from snapshot
				input_state_machine.sync_from_snapshot(p)

	if last_snapshot.players.size() > 0:
		var p = last_snapshot.players[0] as SimPlayerSnap
		if p:
			camera_controller.follow_target(p.x, p.y)
			bottom_hud.sync_player(p)
			bottom_hud.sync_skills(p.skills)
			if p.cast_state >= 2:
				cast_bar_layer.sync_cast(p.cast_progress)
			else:
				cast_bar_layer.hide_cast()
			var ev = entity_manager.get_entity(p.id)
			_skill_vfx.set_skill_aiming(input_state_machine.command_axis == InputStateMachine.CommandAxis.SKILL_AIMING)
			_skill_vfx.sync(last_snapshot, ev)

			if p.cast_slot != _log_prev_cast_slot:
				print("[CAST] slot=%d state=%d err=%d" % [p.cast_slot, p.cast_state, p.cast_error])
				_log_prev_cast_slot = p.cast_slot
			if p.cast_state != _log_prev_cast_state:
				print("[CAST] state %d->%d slot=%d err=%d prog=%.2f" % [_log_prev_cast_state, p.cast_state, p.cast_slot, p.cast_error, p.cast_progress])
				_log_prev_cast_state = p.cast_state
			if p.cast_error > 0:
				print("[CAST] ERROR=%d" % p.cast_error)
			if p.hit_target_id >= 0:
				print("[CAST] HIT target=%d" % p.hit_target_id)


func _trigger_c_slash(target_id: int) -> void:
	if target_id < 0:
		return
	var view = entity_manager.get_entity(target_id)
	if view and is_instance_valid(view) and view.skill_vfx_attachment:
		view.skill_vfx_attachment.show_c_slash()
