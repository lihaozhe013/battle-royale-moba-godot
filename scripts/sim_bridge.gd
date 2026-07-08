extends Node

var sim: SimServer
var last_snapshot: SimSnapshot
var elapsed: float = 0.0
var _last_snap_seq := -1
var _prev_player_cast_state := 0
var _prev_player_cast_slot := -1

@onready var input_collector = $InputCollector
@onready var camera_controller = $CameraController
@onready var entity_manager = $EntityManager
@onready var health_bar_manager = $HealthBarManager
@onready var stats_panel = $CanvasLayer/StatsPanel
@onready var bottom_hud = $BottomHUD
var _skill_vfx: Node3D


func _ready() -> void:
	var file = FileAccess.open("res://data/maps/default.json", FileAccess.READ)
	if not file:
		push_error("Failed to load map JSON")
		return
	var map_json = file.get_as_text()
	file.close()

	_spawn_wall_visuals(map_json)

	health_bar_manager.entity_manager = entity_manager
	health_bar_manager.health_bar_scene = preload("res://scenes/ui/health_bar_ui.tscn")

	sim = SimServer.new()
	sim.initialize(map_json)
	print("SimServer initialized")

	# Auto-create SkillVFX if not in scene tree
	_skill_vfx = $SkillVFX if has_node("SkillVFX") else Node3D.new()
	_skill_vfx.name = "SkillVFX"
	if not _skill_vfx.get_parent():
		add_child(_skill_vfx)
	var vfx_script = preload("res://scripts/view/skill_vfx.gd")
	if not _skill_vfx.get_script():
		_skill_vfx.set_script(vfx_script)


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

func _physics_process(delta: float) -> void:
	var tick_rate = 1.0 / 30.0
	_frame_tick_index = 0
	elapsed += delta
	while elapsed >= tick_rate:
		_frame_tick_index += 1
		sim.set_local_input(
			input_collector.move_input,
			input_collector.aim_world,
			input_collector.fire,
			input_collector.input_seq
		)
		sim.set_cast_input(
			input_collector.cast_slot,
			input_collector.cast_confirm,
			input_collector.cast_cancel,
			input_collector.cast_interrupt,
			input_collector.cast_aim.x,
			input_collector.cast_aim.y
		)
		# 边沿脉冲只在帧首 tick 发送，catch-up 不发
		var first_tick := _frame_tick_index == 1
		sim.set_move_command(
			input_collector.move_cmd_target.x,
			input_collector.move_cmd_target.y,
			input_collector.move_cmd_issue and first_tick
		)
		sim.set_stop(
			input_collector.stop and first_tick
		)
		sim.tick(tick_rate)
		elapsed -= tick_rate
		if sim.is_game_over():
			print("=== GAME OVER ===")
			get_tree().paused = true
			return
		var snap = sim.pop_snapshot()
		if snap is SimSnapshot:
			last_snapshot = snap

	# 消费后清边沿脉冲（防 _physics 隔帧丢信号）
	input_collector.move_cmd_issue = false
	input_collector.stop = false


func _process(_delta: float) -> void:
	if not last_snapshot:
		return
	if last_snapshot.seq != _last_snap_seq:
		_last_snap_seq = last_snapshot.seq
		entity_manager.sync_entities(last_snapshot)
		health_bar_manager.sync_bars(last_snapshot)
		if last_snapshot.players.size() > 0:
			var p := last_snapshot.players[0] as SimPlayerSnap
			if p:
				if _prev_player_cast_state == 2 and p.cast_state == 0 and _prev_player_cast_slot == 0:
					_trigger_c_slash(p)
				_prev_player_cast_state = p.cast_state
				_prev_player_cast_slot = p.cast_slot
	if last_snapshot.players.size() > 0:
		var p = last_snapshot.players[0] as SimPlayerSnap
		if p:
			stats_panel.update(p)
			camera_controller.follow_target(p.x, p.y)
			bottom_hud.sync_player(p)
			bottom_hud.sync_skills(p.skills)
			var ev = entity_manager.get_entity(p.id)
			_skill_vfx.sync(last_snapshot, ev)


func _trigger_c_slash(p: SimPlayerSnap) -> void:
	var aim_pos := Vector2(p.cast_aim_x, p.cast_aim_y)
	var range_sq := 9.0
	var target_id := -1

	for b in last_snapshot.bots:
		var d_sq := Vector2(b.x, b.y).distance_squared_to(aim_pos)
		if d_sq < range_sq:
			range_sq = d_sq
			target_id = b.id

	if target_id >= 0:
		var view = entity_manager.get_entity(target_id)
		if view and is_instance_valid(view) and view.skill_vfx_attachment:
			view.skill_vfx_attachment.show_c_slash()
