extends Node

var sim: SimServer
var last_snapshot: SimSnapshot
var elapsed: float = 0.0
var _last_snap_seq := -1

@onready var input_collector = $InputCollector
@onready var camera_controller = $CameraController
@onready var entity_manager = $EntityManager
@onready var health_bar_manager = $HealthBarManager
@onready var stats_panel = $CanvasLayer/StatsPanel
@onready var bottom_hud = $BottomHUD


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


func _physics_process(delta: float) -> void:
	var tick_rate = 1.0 / 30.0
	elapsed += delta
	while elapsed >= tick_rate:
		sim.set_local_input(
			input_collector.move_input,
			input_collector.aim_world,
			input_collector.fire,
			input_collector.input_seq
		)
		sim.set_skill_input(
			input_collector.skill_q,
			input_collector.skill_w,
			input_collector.skill_e,
			input_collector.skill_r
		)
		sim.tick(tick_rate)
		elapsed -= tick_rate
		var snap = sim.pop_snapshot()
		if snap is SimSnapshot:
			last_snapshot = snap


func _process(_delta: float) -> void:
	if not last_snapshot:
		return
	if last_snapshot.seq != _last_snap_seq:
		_last_snap_seq = last_snapshot.seq
		entity_manager.sync_entities(last_snapshot)
		health_bar_manager.sync_bars(last_snapshot)
	if last_snapshot.players.size() > 0:
		var p = last_snapshot.players[0] as SimPlayerSnap
		if p:
			stats_panel.update(p)
			camera_controller.follow_target(p.x, p.y)
			bottom_hud.sync_player(p)
			bottom_hud.sync_skills(p.skills)
