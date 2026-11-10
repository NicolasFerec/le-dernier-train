class_name PlayerLocomotion
extends Node2D

const DASH_COOLDOWN := 0.65
const DASH_SPEED := 550.0
const DASH_DURATION := 0.16

var hp := 5
var speed := 220.0
var facing := Vector2.RIGHT
var movement := Vector2.ZERO
var dash_time := 0.0
var dash_cooldown := 0.0
var dash_direction := Vector2.ZERO


func step_motion(delta: float, bounds: Rect2) -> void:
	dash_time = maxf(0.0, dash_time - delta)
	dash_cooldown = maxf(0.0, dash_cooldown - delta)
	if hp <= 0:
		return
	if movement != Vector2.ZERO:
		facing = movement.normalized()
	var velocity := dash_direction * DASH_SPEED if dash_time > 0 else movement * speed
	position = (position + velocity * delta).clamp(bounds.position, bounds.end)


func dodge() -> bool:
	if hp <= 0 or dash_cooldown > 0:
		return false
	dash_direction = facing if movement == Vector2.ZERO else movement.normalized()
	dash_time = DASH_DURATION
	dash_cooldown = DASH_COOLDOWN
	return true
