extends Control

const PORTRAITS := [
	preload("res://assets/tiny_dungeon/fighter_face.png"),
	preload("res://assets/tiny_dungeon/scout_face.png"),
]
const NAMES := ["FIGHTER", "SCOUT"]

var character := 0
var hp := 0
var max_hp := 1
var dash := 1.0


func update_player(player: Node2D) -> void:
	character = player.character
	hp = player.hp
	max_hp = player.max_hp
	dash = 1.0 - clampf(player.dash_cooldown / player.DASH_COOLDOWN, 0, 1)
	queue_redraw()


func _draw() -> void:
	var font := get_theme_default_font()
	draw_rect(Rect2(0, 0, 272, 68), Color("#171b27"))
	draw_rect(Rect2(0, 0, 272, 68), Color("#777087"), false, 1)
	draw_rect(Rect2(7, 7, 59, 54), Color("#303447"))
	draw_texture_rect(PORTRAITS[character], Rect2(8, 14, 56, 38), false)
	draw_string(font, Vector2(76, 19), tr(NAMES[character]), HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color("#f5ddbb"))
	draw_string(font, Vector2(205, 19), tr("HP") % [hp, max_hp], HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color.WHITE)
	draw_rect(Rect2(76, 25, 183, 12), Color("#3d2b39"))
	draw_rect(Rect2(76, 25, 183.0 * hp / max_hp, 12), Color("#dd5265"))
	draw_rect(Rect2(76, 45, 114, 6), Color("#263c50"))
	draw_rect(Rect2(76, 45, 114 * dash, 6), Color("#68cce3"))
	draw_string(font, Vector2(198, 53), tr("DODGE"), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color("#b7ddeb"))
