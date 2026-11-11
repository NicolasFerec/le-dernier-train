extends Control

const PORTRAITS := [
	preload("res://player/conductor_portrait.png"),
	preload("res://player/scout_portrait.svg"),
]
const NAMES := ["FIGHTER", "SCOUT"]

var character := 0
var hp := 0
var max_hp := 1
var dash := 1.0
var teammates: Array[Dictionary] = []


func update_party(player: Node2D, players: Dictionary) -> void:
	character = player.character
	hp = player.hp
	max_hp = player.max_hp
	dash = 1.0 - clampf(player.dash_cooldown / player.DASH_COOLDOWN, 0, 1)
	teammates.clear()
	for id in players:
		var teammate: Node2D = players[id]
		if teammate == player:
			continue
		teammates.append({
			"slot": teammate.slot,
			"character": teammate.character,
			"name": _teammate_name(id, teammate.slot),
			"color": teammate.COLORS[teammate.slot],
			"hp": teammate.hp,
			"max_hp": teammate.max_hp,
		})
	teammates.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a.slot < b.slot)
	queue_redraw()


func _teammate_name(id: int, slot: int) -> String:
	for entry in Session.roster:
		if entry[0] == id:
			return entry[1]
	return tr("SESSION_PLAYER_FALLBACK") % (slot + 1)


func _draw() -> void:
	var font := get_theme_default_font()
	for index in teammates.size():
		_draw_teammate(font, teammates[index], -44.0 * (index + 1))
	draw_rect(Rect2(0, 0, 272, 68), Color("#171b27"))
	draw_rect(Rect2(0, 0, 272, 68), Color("#777087"), false, 1)
	draw_rect(Rect2(7, 7, 59, 54), Color("#303447"))
	draw_texture_rect(PORTRAITS[character], Rect2(9, 9, 54, 49), false)
	draw_string(font, Vector2(76, 19), tr(NAMES[character]), HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color("#f5ddbb"))
	draw_string(font, Vector2(205, 19), tr("HP") % [hp, max_hp], HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color.WHITE)
	draw_rect(Rect2(76, 25, 183, 12), Color("#3d2b39"))
	draw_rect(Rect2(76, 25, 183.0 * hp / max_hp, 12), Color("#dd5265"))
	draw_rect(Rect2(76, 45, 114, 6), Color("#263c50"))
	draw_rect(Rect2(76, 45, 114 * dash, 6), Color("#68cce3"))
	draw_string(font, Vector2(198, 53), tr("DODGE"), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color("#b7ddeb"))


func _draw_teammate(font: Font, teammate: Dictionary, y: float) -> void:
	var tint := Color.WHITE if teammate.hp > 0 else Color("#737783")
	draw_rect(Rect2(0, y, 192, 38), Color("#171b27"))
	draw_rect(Rect2(0, y, 192, 38), Color("#777087"), false, 1)
	draw_rect(Rect2(0, y, 3, 38), teammate.color)
	draw_rect(Rect2(7, y + 4, 33, 30), Color("#303447"))
	draw_texture_rect(PORTRAITS[teammate.character], Rect2(8, y + 5, 31, 28), false, tint)

	var health := tr("HP") % [teammate.hp, teammate.max_hp]
	var health_width := font.get_string_size(health, HORIZONTAL_ALIGNMENT_LEFT, -1, 11).x
	draw_string(font, Vector2(47, y + 16), teammate.name, HORIZONTAL_ALIGNMENT_LEFT, 132 - health_width, 12, Color("#f5ddbb"))
	draw_string(font, Vector2(184 - health_width, y + 16), health, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, tint)
	draw_rect(Rect2(47, y + 23, 137, 7), Color("#3d2b39"))
	var fraction := clampf(float(teammate.hp) / teammate.max_hp, 0.0, 1.0)
	if fraction > 0:
		draw_rect(Rect2(47, y + 23, 137 * fraction, 7), Color("#dd5265"))
