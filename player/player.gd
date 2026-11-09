extends Node2D

const COLORS := [Color.SKY_BLUE, Color.GOLD, Color.LIGHT_GREEN, Color.VIOLET]

const DASH_COOLDOWN := 0.65
const SPRITES := [
	preload("res://assets/tiny_dungeon/fighter.png"),
	preload("res://assets/tiny_dungeon/scout.png"),
]

var character := 0
var slot := 0
var hp := 5
var max_hp := 5
var speed := 220.0
var reach := 78.0
var damage := 1
var attack_delay := 0.36
var facing := Vector2.RIGHT
var movement := Vector2.ZERO
var attack_time := 0.0
var dash_time := 0.0
var dash_cooldown := 0.0
var dash_direction := Vector2.ZERO


func step(delta: float, bounds: Rect2) -> void:
	attack_time = maxf(0, attack_time - delta)
	dash_time = maxf(0, dash_time - delta)
	dash_cooldown = maxf(0, dash_cooldown - delta)
	if hp > 0:
		if movement != Vector2.ZERO:
			facing = movement.normalized()
		var velocity := dash_direction * 550.0 if dash_time > 0 else movement * speed
		position = (position + velocity * delta).clamp(bounds.position, bounds.end)
	queue_redraw()


func dodge() -> void:
	if hp > 0 and dash_cooldown <= 0:
		dash_direction = facing if movement == Vector2.ZERO else movement.normalized()
		dash_time = 0.16
		dash_cooldown = DASH_COOLDOWN


func _draw() -> void:
	var color: Color = COLORS[slot] if hp > 0 else Color.DIM_GRAY
	draw_set_transform(Vector2(0, 10), 0, Vector2(1, 0.43))
	draw_circle(Vector2.ZERO, 18, Color(0.04, 0.06, 0.09, 0.75))
	draw_set_transform(Vector2.ZERO)
	draw_texture_rect(SPRITES[character], Rect2(-20, -26, 40, 40), false,
		Color.WHITE if hp > 0 else Color(0.5, 0.5, 0.55, 0.55))
	draw_arc(Vector2(0, 8), 17, 0.25, PI - 0.25, 20, color, 2, true)
	if hp > 0:
		draw_circle(facing * 21 + Vector2(0, 5), 2.5, color)
		if dash_time > 0:
			draw_arc(Vector2.ZERO, 21, 0, TAU, 24, Color.WHITE, 2)
		if attack_time > attack_delay - 0.16:
			draw_arc(Vector2.ZERO, reach, facing.angle() - 1.15, facing.angle() + 1.15, 24, color, 4)
	draw_rect(Rect2(-15, -25, 30, 4), Color.DIM_GRAY)
	draw_rect(Rect2(-15, -25, 30.0 * hp / max_hp, 4), color)
