extends Node2D

const SPRITE := preload("res://assets/tiny_dungeon/enemy.png")

var hp := 6
var hit_cooldown := 0.0


func step(delta: float, players: Dictionary) -> void:
	if hp <= 0:
		return
	hit_cooldown = maxf(0, hit_cooldown - delta)
	var target: Node2D
	var nearest := INF
	for player in players.values():
		var distance := position.distance_squared_to(player.position)
		if player.hp > 0 and distance < nearest:
			nearest = distance
			target = player
	if target == null:
		return
	if nearest > 32.0 * 32.0:
		position = position.move_toward(target.position, 100.0 * delta)
	elif hit_cooldown <= 0 and target.dash_time <= 0:
		target.hp -= 1
		hit_cooldown = 0.85


func _draw() -> void:
	if hp <= 0:
		draw_circle(Vector2.ZERO, 16, Color(0.3, 0.17, 0.2))
		draw_line(Vector2(-8, -8), Vector2(8, 8), Color.SALMON, 2)
		draw_line(Vector2(8, -8), Vector2(-8, 8), Color.SALMON, 2)
		return
	draw_set_transform(Vector2(0, 12), 0, Vector2(1, 0.43))
	draw_circle(Vector2.ZERO, 19, Color(0.04, 0.06, 0.09, 0.75))
	draw_set_transform(Vector2.ZERO)
	draw_texture_rect(SPRITE, Rect2(-23, -28, 46, 46), false)
	draw_rect(Rect2(-18, -29, 36, 5), Color.DIM_GRAY)
	draw_rect(Rect2(-18, -29, hp * 6, 5), Color.SALMON)
