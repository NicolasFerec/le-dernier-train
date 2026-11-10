extends Node2D

signal struck(target_id: int)

const AGGRO_RANGE := 360.0
const INK := Color("#21262b")
const IRON := Color("#856b61")
const OXIDE := Color("#bf7661")

var hp := 6
var hit_cooldown := 0.0
var facing := Vector2.LEFT
var moving := false
var paused := false
var previous_position := Vector2.ZERO
var render_position := Vector2.ZERO
var _gait := 0.0


func step(delta: float, players: Dictionary) -> void:
	previous_position = position
	moving = false
	if hp <= 0:
		return
	hit_cooldown = maxf(0, hit_cooldown - delta)
	var target: Node2D
	var nearest := INF
	var target_id := 0
	for id in players:
		var player: Node2D = players[id]
		var distance := position.distance_squared_to(player.position)
		if player.hp > 0 and distance < nearest:
			nearest = distance
			target = player
			target_id = id
	if target == null or nearest > AGGRO_RANGE * AGGRO_RANGE:
		return
	facing = position.direction_to(target.position)
	if nearest > 32.0 * 32.0:
		moving = true
		position = position.move_toward(target.position, 100.0 * delta)
	elif hit_cooldown <= 0 and target.dash_time <= 0:
		target.hp -= 1
		hit_cooldown = 0.85
		struck.emit(target_id)


func _process(delta: float) -> void:
	var projected := Isometric.GROUND * render_position
	z_index = Isometric.depth(projected.y)
	if not paused:
		_gait = fmod(_gait + delta * 12.0, TAU) if moving and hp > 0 else 0.0
	queue_redraw()


func _draw() -> void:
	var offset := Isometric.GROUND * render_position - position
	draw_set_transform(offset, 0, Vector2(1, 0.45))
	draw_circle(Vector2.ZERO, 24, Color("#1a1f23", 0.73), true, -1, true)
	draw_set_transform(offset)
	if hp <= 0:
		draw_line(Vector2(-17, 3), Vector2(11, -6), INK, 16, true)
		draw_line(Vector2(-12, 1), Vector2(10, -6), IRON.darkened(0.3), 9, true)
		draw_circle(Vector2(15, -5), 7, Color("#604d4c"), true, -1, true)
		return
	var aim := (Isometric.GROUND * facing).normalized()
	var strike := clampf((hit_cooldown - 0.66) / 0.19, 0.0, 1.0)
	var body := Vector2(0, -20 - absf(sin(_gait)) * 4) + aim * 8 * strike
	for side in [-1, 1]:
		var foot: Vector2 = Vector2(side * 13, -2) + aim * side * sin(_gait) * 8
		draw_line(body + Vector2(side * 8, 2), foot, INK, 10, true)
		draw_line(body + Vector2(side * 8, 2), foot, IRON.darkened(0.28), 6, true)
		var fist := body + Vector2(side * 19, 5) + aim * strike * 14
		draw_line(body + Vector2(side * 8, -5), fist, INK, 12, true)
		draw_line(body + Vector2(side * 8, -5), fist, OXIDE.darkened(0.19), 7, true)
		draw_circle(fist, 5, IRON, true, -1, true)
	draw_circle(body, 17, INK, true, -1, true)
	draw_circle(body, 14, IRON, true, -1, true)
	draw_arc(body, 11, PI, TAU, 20, OXIDE, 3, true)
	var head := body + Vector2(0, -11) + aim * 5
	draw_circle(head, 9, INK, true, -1, true)
	draw_circle(head, 6, Color("#554951"), true, -1, true)
	if aim.y > -0.5:
		draw_line(head + Vector2(-3, 1), head + Vector2(4, 1), Color("#ffd277"), 2, true)
	draw_line(Vector2(-14, -54), Vector2(14, -54), INK, 5, true)
	draw_line(Vector2(-13, -54), Vector2(-13 + hp * (26.0 / 6), -54), Color("#ea917f"), 3, true)
