extends PlayerLocomotion

const COLORS := [Color.SKY_BLUE, Color.GOLD, Color.LIGHT_GREEN, Color.VIOLET]

const SWING_TIME := 0.16
const HEAVY_SWING_TIME := 0.32
const HEAVY_SWING_HALF_ANGLE := acos(0.4)
const LIGHT_SWING_HALF_ANGLE := deg_to_rad(14.0)
const VISUAL := preload("res://player/character_visual.tscn")

var character := 0
var weapon := 0
var slot := 0
var paused := false
var max_hp := 5
var reach := 78.0
var damage := 1
var attack_delay := 0.36
var attack_time := 0.0
var heavy_attack := false
var reverse_light_swing := true
var attack_hits := {}
var attack_origin := Vector2.ZERO
var attack_direction := Vector2.RIGHT
var previous_position := Vector2.ZERO
var render_position := Vector2.ZERO
var render_facing := Vector2.RIGHT
var render_movement := Vector2.ZERO
var render_dash_time := 0.0


func _ready() -> void:
	var visual := VISUAL.instantiate()
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
	heavy_attack = heavy
	if not heavy:
		reverse_light_swing = not reverse_light_swing
	attack_hits.clear()
	attack_origin = position
	attack_direction = facing
	attack_time = attack_delay


func swing_progress() -> float:
	return clampf((attack_delay - attack_time) / (HEAVY_SWING_TIME if heavy_attack else SWING_TIME), 0.0, 1.0)


func swing_offset(progress: float) -> float:
	var half_angle := HEAVY_SWING_HALF_ANGLE if heavy_attack else LIGHT_SWING_HALF_ANGLE
	if not heavy_attack and reverse_light_swing:
		half_angle = -half_angle
	return lerpf(half_angle, -half_angle, progress)


func swept_hit(target: Variant, point: Vector2, previous: float, padding := 0.0) -> bool:
	if attack_hits.has(target):
		return false
	var offset := point - attack_origin
	if offset.length_squared() > (reach + (padding if heavy_attack else 16.0)) ** 2:
		return false
	var angle := attack_direction.angle_to(offset)
	var start := swing_offset(previous)
	var end := swing_offset(swing_progress())
	var closest_angle := clampf(angle, minf(start, end), maxf(start, end))
	if heavy_attack:
		if angle != closest_angle:
			return false
	else:
		var ray := attack_direction.rotated(closest_angle)
		var closest_point := ray * clampf(offset.dot(ray), 0.0, reach)
		if offset.distance_squared_to(closest_point) > 16.0 ** 2:
			return false
	attack_hits[target] = true
	return true


func step(delta: float, bounds: Rect2) -> void:
	previous_position = position
	step_attack(delta)
	step_motion(delta, bounds)


func step_attack(delta: float) -> void:
	attack_time = maxf(0, attack_time - delta)
