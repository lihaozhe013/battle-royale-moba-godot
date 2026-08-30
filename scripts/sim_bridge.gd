extends Node

var sim: SimServer
var last_snapshot: SimSnapshot
var elapsed: float = 0.0
var _last_snap_seq := -1
var _prev_player_cast_error := 0

@onready var camera_controller = $CameraController
@onready var entity_manager = $EntityManager
@onready var camera: Camera3D = $CameraController/Camera3D
@onready var ui_root: UIRoot = $UIRoot
var input_event_queue: InputEventQueue
var input_state_machine: InputStateMachine
var command_buffer: CommandBuffer
var command_builder: CommandBuilder
var cast_settings: CastSettings
var _skill_vfx: Node3D

const HOVER_RADIUS := 2.0
const TICK_RATE := 1.0 / 30.0
const START_MENU_SCENE := "res://scenes/start_menu.tscn"
const MATCH_RESULTS_SCENE := "res://scenes/match_results.tscn"
const NORMAL_CURSOR_TEXTURE: Texture2D = preload(
	"res://resources/ui/cursors/normal_cursor.png"
)
const CAST_CURSOR_TEXTURE: Texture2D = preload(
	"res://resources/ui/cursors/cast_cursor.png"
)
const CURSOR_HOTSPOT := Vector2(2.0, 2.0)
var _cursor_cast_mode := false
var _cursor_initialized := false
var _results_shown := false

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
	_set_cursor_mode(false)
	MatchResultStore.clear()
	var file = FileAccess.open("res://data/maps/default.json", FileAccess.READ)
	if not file:
		DebugLogger.log("[ERROR] Failed to load map JSON")
		return
	var map_json = file.get_as_text()
	file.close()
	var stats_file = FileAccess.open("res://data/stats.yaml", FileAccess.READ)
	if not stats_file:
		DebugLogger.log("[ERROR] [stats_config] Failed to load stats YAML")
		return
	var stats_yaml = stats_file.get_as_text()
	stats_file.close()

	# Auto-create input layer nodes
	input_event_queue = _ensure_node(
		"InputEventQueue", &"res://scripts/input/input_event_queue.gd"
	)
	input_state_machine = _ensure_node(
		"InputStateMachine", &"res://scripts/input/input_state_machine.gd"
	)
	command_buffer = _ensure_node(
		"CommandBuffer", &"res://scripts/input/command_buffer.gd"
	)
	command_builder = _ensure_node(
		"CommandBuilder", &"res://scripts/input/command_builder.gd"
	)
	cast_settings = _ensure_node(
		"CastSettings", &"res://scripts/input/cast_settings.gd"
	)
	command_builder.setup(
		input_event_queue, input_state_machine, command_buffer, cast_settings
	)

	_spawn_wall_visuals(map_json)
	ui_root.bind_runtime(entity_manager, camera, cast_settings)
	ui_root.settings_panel.main_menu_requested.connect(_on_main_menu_requested)

	sim = SimServer.new()
	if not sim.initialize(map_json, stats_yaml, MatchSetup.selected_hero_id):
		DebugLogger.log("[ERROR] [stats_config] SimServer initialization failed")
		return
	DebugLogger.log("SimServer initialized")
	ui_root.prewarm_health_bars(sim.get_hero_capacity())

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


func _on_main_menu_requested() -> void:
	_set_cursor_mode(false)
	get_tree().paused = false
	MatchResultStore.clear()
	MatchSetup.reset()
	DebugLogger.log("[start_menu] returning_to_menu")
	var error := get_tree().change_scene_to_file(START_MENU_SCENE)
	if error != OK:
		DebugLogger.log(
			"[ERROR] [start_menu] menu_scene_failed code=%d" % error
		)


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

		# New Sim API (unconditional, C++ side must always receive cleared values)
		sim.set_skill_command(
			_tmp_cast_slot,
			_tmp_cast_confirm,
			_tmp_cast_aim.x,
			_tmp_cast_aim.y,
			_tmp_cast_target_id
		)
		sim.set_skill_upgrade_command(_tmp_upgrade_slot)
		sim.set_attack_command_full(
			_tmp_attack_target_id,
			_tmp_attack_ground,
			_tmp_attack_ground_pos.x,
			_tmp_attack_ground_pos.y,
			_tmp_attack_clear
		)
		sim.set_cancel_command(_tmp_cancel_skill, _tmp_cancel_attack)
		sim.set_move_command(
			_tmp_move_target.x,
			_tmp_move_target.y,
			_tmp_move_issue and first_tick
		)
		sim.set_stop_command(_tmp_stop and first_tick)
		# Per-tick command tracing is disabled during normal gameplay.
		# Uncomment when debugging simulation input forwarding.
		# DebugLogger.log(
		# 	(
		# 		"[TICK] send: skill(slot=%d,confirm=%s) cancel(skill=%s,atk=%s) move(issue=%s,to=(%.1f,%.1f)) atk(id=%d) stop=%s"
		# 		% [
		# 			_tmp_cast_slot,
		# 			_tmp_cast_confirm,
		# 			_tmp_cancel_skill,
		# 			_tmp_cancel_attack,
		# 			_tmp_move_issue and first_tick,
		# 			_tmp_move_target.x,
		# 			_tmp_move_target.y,
		# 			_tmp_attack_target_id,
		# 			_tmp_stop and first_tick
		# 		]
		# 	)
		# )
		sim.tick(TICK_RATE)

		# Clear pulse fields
		if first_tick:
			_tmp_move_issue = false
			_tmp_stop = false
		_tmp_cast_confirm = false
		_tmp_cast_slot = -1
		_tmp_upgrade_slot = -1
		_tmp_cancel_skill = false
		_tmp_cancel_attack = false
		_tmp_attack_target_id = -1
		_tmp_attack_ground = false
		_tmp_attack_clear = false

		var snap = sim.pop_snapshot()
		if snap is SimSnapshot:
			last_snapshot = snap

		elapsed -= TICK_RATE
		if sim.is_game_over():
			_show_match_results(last_snapshot)
			return

	if ran_tick and last_snapshot:
		var local_idx = -1
		if last_snapshot.has_method("get_local_hero_index"):
			local_idx = last_snapshot.get_local_hero_index()
		if local_idx >= 0 and last_snapshot.heroes.size() > 0:
			input_state_machine.sync_from_snapshot(
				last_snapshot.heroes[local_idx]
			)
		elif last_snapshot.players.size() > 0:
			input_state_machine.sync_from_snapshot(last_snapshot.players[0])


func _show_match_results(final_snapshot: SimSnapshot) -> void:
	if _results_shown:
		return
	_results_shown = true
	if final_snapshot:
		MatchResultStore.capture_snapshot(final_snapshot)
		DebugLogger.log(
			(
				"[match_results] match_finished result=%d time=%.2f participants=%d"
				% [
					final_snapshot.result,
					final_snapshot.match_time,
					final_snapshot.heroes.size(),
				]
			)
		)
	else:
		DebugLogger.log("[ERROR] [match_results] final_snapshot_missing")
		MatchResultStore.clear()
	_set_cursor_mode(false)
	get_tree().paused = false
	var error := get_tree().change_scene_to_file(MATCH_RESULTS_SCENE)
	if error != OK:
		_results_shown = false
		DebugLogger.log(
			"[ERROR] [match_results] results_scene_failed code=%d" % error
		)


func _apply_command(c: Command) -> void:
	DebugLogger.log("[APPLY] %s" % c.get_type_name())
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


func _update_cursor_mode() -> void:
	if input_state_machine:
		_set_cursor_mode(input_state_machine.is_targeting_mode())


func _set_cursor_mode(cast_mode: bool) -> void:
	if _cursor_initialized and _cursor_cast_mode == cast_mode:
		return
	_cursor_cast_mode = cast_mode
	_cursor_initialized = true
	Input.set_custom_mouse_cursor(
		CAST_CURSOR_TEXTURE if cast_mode else NORMAL_CURSOR_TEXTURE,
		Input.CURSOR_ARROW,
		CURSOR_HOTSPOT
	)


func _process(_delta: float) -> void:
	_update_cursor_mode()
	if not last_snapshot:
		return

	var aim: Vector2 = input_event_queue.mouse_world

	# Hover detection
	var hover_id := -1
	var hover_sq := HOVER_RADIUS * HOVER_RADIUS
	if last_snapshot.heroes.size() > 0:
		for h in last_snapshot.heroes:
			if not h.is_local and not h.dead:
				var d_sq := Vector2(h.x, h.y).distance_squared_to(aim)
				if d_sq < hover_sq:
					hover_sq = d_sq
					hover_id = h.id
	else:
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
		ui_root.health_bar_manager.sync_bars(last_snapshot)
		var local_idx = (
			last_snapshot.get_local_hero_index()
			if last_snapshot.has_method("get_local_hero_index")
			else -1
		)
		if local_idx == -1 and last_snapshot.players.size() > 0:
			local_idx = 0
		if local_idx >= 0 and last_snapshot.heroes.size() > 0:
			var p := last_snapshot.heroes[local_idx] as SimHeroSnap
			if p:
				entity_manager.set_attack_target_id(p.attack_target_id)
				if p.cast_error > 0 and p.cast_error != _prev_player_cast_error:
					ui_root.cast_error.show_error(p.cast_error)
			_prev_player_cast_error = p.cast_error

			input_state_machine.sync_from_snapshot(p)
		elif last_snapshot.players.size() > 0:
			var p := last_snapshot.players[0] as SimPlayerSnap
			if p:
				entity_manager.set_attack_target_id(p.attack_target_id)
				if p.cast_error > 0 and p.cast_error != _prev_player_cast_error:
					ui_root.cast_error.show_error(p.cast_error)
			_prev_player_cast_error = p.cast_error

			input_state_machine.sync_from_snapshot(p)

	var local_snapshot = _get_local_snapshot()
	if local_snapshot:
		camera_controller.follow_target(local_snapshot.x, local_snapshot.y)
		ui_root.bottom_hud.sync_player(local_snapshot)
		ui_root.bottom_hud.sync_skills(local_snapshot.skills)
		if local_snapshot.cast_state >= 3:
			ui_root.cast_bar.sync_cast(local_snapshot.cast_progress)
		else:
			ui_root.cast_bar.hide_cast()
		_skill_vfx.sync(last_snapshot, entity_manager, input_state_machine)

		if local_snapshot.cast_slot != _log_prev_cast_slot:
			DebugLogger.log(
				"[CAST] slot=%d state=%d err=%d" % [
					local_snapshot.cast_slot,
					local_snapshot.cast_state,
					local_snapshot.cast_error,
				]
			)
			_log_prev_cast_slot = local_snapshot.cast_slot
		if local_snapshot.cast_state != _log_prev_cast_state:
			DebugLogger.log(
				"[CAST] state %d->%d slot=%d err=%d prog=%.2f" % [
					_log_prev_cast_state,
					local_snapshot.cast_state,
					local_snapshot.cast_slot,
					local_snapshot.cast_error,
					local_snapshot.cast_progress,
				]
			)
			_log_prev_cast_state = local_snapshot.cast_state
		if local_snapshot.cast_error > 0:
			DebugLogger.log("[CAST] ERROR=%d" % local_snapshot.cast_error)
		if local_snapshot.hit_target_id >= 0:
			DebugLogger.log("[CAST] HIT target=%d" % local_snapshot.hit_target_id)


func _get_local_snapshot():
	if last_snapshot.heroes.size() > 0:
		var local_idx := (
			last_snapshot.get_local_hero_index()
			if last_snapshot.has_method("get_local_hero_index")
			else 0
		)
		if local_idx >= 0 and local_idx < last_snapshot.heroes.size():
			return last_snapshot.heroes[local_idx]
	if last_snapshot.players.size() > 0:
		return last_snapshot.players[0]
	return null
