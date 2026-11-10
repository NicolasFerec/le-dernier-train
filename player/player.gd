extends PlayerLocomotion

const COLORS := [Color.SKY_BLUE, Color.GOLD, Color.LIGHT_GREEN, Color.VIOLET]

const SWING_TIME := 0.16
const SWING_MIN_DOT := 0.4
const VISUAL := preload("res://player/character_visual.gd")

var character := 0
var weapon := 0
var slot := 0
var paused := false
var max_hp := 5
var reach := 78.0
var damage := 1
var attack_delay := 0.36
var attack_time := 0.0
var attack_origin := Vector2.ZERO
var attack_direction := Vector2.RIGHT
var previous_position := Vector2.ZERO
var render_position := Vector2.ZERO
var render_facing := Vector2.RIGHT
var render_movement := Vector2.ZERO
var render_dash_time := 0.0


func _ready() -> void:
	var visual := VISUAL.new()
	visual.actor = self
	add_child(visual)


func equip(kit: Array) -> void:
	character = kit[0]
	weapon = kit[1]
	max_hp = (4 if character == 1 else 5) + kit[2]
	hp = max_hp
	speed = (255.0 if character == 1 else 220.0) * (1.0 + 0.15 * kit[3])
	damage = 2 if character == 0 and weapon == 1 else 1
	reach = 78.0 if weapon == 0 else (62.0 if character == 0 else 56.0)
	attack_time = 0.0


func start_attack(heavy: bool) -> void:
	var light_delay := 0.36 if weapon == 0 else (0.58 if character == 0 else 0.23)
	attack_delay = light_delay * (2.0 if heavy else 1.0)
	attack_origin = position
	attack_direction = facing
	attack_time = attack_delay


func can_hit(point: Vector2, padding := 0.0) -> bool:
	var offset := point - attack_origin
	return offset.length() <= reach + padding and attack_direction.dot(offset.normalized()) >= SWING_MIN_DOT


func step(delta: float, bounds: Rect2) -> void:
	previous_position = position
	step_attack(delta)
	step_motion(delta, bounds)


func step_attack(delta: float) -> void:
	attack_time = maxf(0, attack_time - delta)
