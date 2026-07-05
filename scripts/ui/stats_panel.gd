extends Control

@onready var label := $Label

func _ready() -> void:
	label.text = ""

func update(p: SimPlayerSnap) -> void:
	label.text = "Lv %d | HP %d/%d | ATK %.0f | ASP %.2f | Kills %d | XP %d/%d" % [
		p.level, p.hp, p.max_hp, p.atk, p.asp, p.kills, p.xp, p.xp_needed
	]
