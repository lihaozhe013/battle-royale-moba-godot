extends Node3D
class_name MeleeStrikeVfx

@onready var _lightning: Node3D = $Lightning


func _ready() -> void:
	if not _lightning.has_method("play"):
		queue_free()
		return
	if _lightning.has_signal("finished"):
		_lightning.finished.connect(_on_lightning_finished)
	_lightning.play()


func _on_lightning_finished() -> void:
	queue_free()
