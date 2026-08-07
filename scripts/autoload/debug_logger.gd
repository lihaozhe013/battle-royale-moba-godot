extends Node

const DEFAULT_LOG_PATH := "res://debug.log"
const FLUSH_INTERVAL := 0.25

var _file: FileAccess
var _pending: Array[String] = []
var _flush_elapsed := 0.0


func _ready() -> void:
	var log_path: String = ProjectSettings.get_setting(
		"debug/file_logging/log_path", DEFAULT_LOG_PATH
	)
	_file = FileAccess.open(log_path, FileAccess.WRITE)
	if not _file and log_path != "user://debug.log":
		log_path = "user://debug.log"
		_file = FileAccess.open(log_path, FileAccess.WRITE)
	if not _file:
		push_error("[debug_logging] Failed to open log file")


func log(message: String) -> void:
	if not _file:
		return
	_pending.append(message)


func _process(delta: float) -> void:
	if _pending.is_empty():
		return
	_flush_elapsed += delta
	if _flush_elapsed >= FLUSH_INTERVAL:
		_flush()


func _flush() -> void:
	if not _file or _pending.is_empty():
		return
	for message in _pending:
		_file.store_line(message)
	_pending.clear()
	_flush_elapsed = 0.0
	_file.flush()


func _exit_tree() -> void:
	_flush()
	if _file:
		_file.close()
