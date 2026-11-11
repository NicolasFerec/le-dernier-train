extends RefCounted

enum Pose { IDLE, RUN, DODGE, LIGHT, HEAVY, HIT, FALL }

const BODY := preload("res://player/conductor/poses.png")
const WEAPONS := [
	preload("res://player/conductor/sabre.png"),
	preload("res://player/conductor/hammer.png"),
]
const WORKSHOP_POSES := [
	preload("res://player/conductor/workshop_sabre.png"),
	preload("res://player/conductor/workshop_marteau.png"),
]
const WORKSHOP_ROWS := {Pose.RUN: 0, Pose.DODGE: 1, Pose.LIGHT: 2, Pose.HEAVY: 3, Pose.IDLE: 2}
const SOUTHWEST := 1
const NORTHEAST := 5
const GRIPS := preload("res://player/conductor/grips.gd")
const CELL := Vector2(128, 128)
const GROUND := Vector2(64, 95)
const WEAPON_GRIP := Vector2(32, 32)
const RUN_CYCLE_DISTANCE := 73.0
const HIT_TIME := 0.18
const FALL_TIME := 0.24

var pose := Pose.IDLE
var direction := 0
var frame := 0
var _run_phase := 0.0
var _hit_time := 0.0
var _fall_time := 0.0
var _previous_hp := -1


func update(actor: Node2D, delta: float, traveled: float) -> void:
	if actor.paused:
		return
	var facing: Vector2 = actor.render_facing
	var progress := 0.0
	var frame_count := 1
	if actor.hp > 0:
		_fall_time = 0.0
	if _previous_hp > actor.hp:
		_hit_time = HIT_TIME
	_previous_hp = actor.hp
	_hit_time = maxf(0.0, _hit_time - delta)

	if actor.hp <= 0:
		pose = Pose.FALL
		_fall_time += delta
		progress = _fall_time / FALL_TIME
		frame_count = 2
	elif actor.render_dash_time > 0.0:
		pose = Pose.DODGE
		facing = actor.dash_direction
		progress = 1.0 - actor.render_dash_time / PlayerLocomotion.DASH_DURATION
		frame_count = 4
	elif actor.swing_progress() < 1.0:
		pose = Pose.HEAVY if actor.heavy_attack else Pose.LIGHT
		facing = actor.attack_direction
		progress = actor.swing_progress()
		frame_count = 4
	elif _hit_time > 0.0:
		pose = Pose.HIT
		progress = 1.0 - _hit_time / HIT_TIME
		frame_count = 2
	elif actor.render_movement != Vector2.ZERO:
		pose = Pose.RUN
		_run_phase = fposmod(_run_phase + traveled / RUN_CYCLE_DISTANCE, 1.0)
		progress = _run_phase
		frame_count = 4
	else:
		pose = Pose.IDLE
		_run_phase = 0.0

	var angle := (Isometric.GROUND * facing).angle()
	direction = posmod(roundi(angle / (PI / 4)) - 2, 8)
	frame = clampi(floori(progress * frame_count), 0, frame_count - 1)
	if pose == Pose.LIGHT and actor.reverse_light_swing and frame in [1, 2] and not _has_workshop_pose():
		frame = 3 - frame


func draw(canvas: CanvasItem, actor: Node2D) -> void:
	if _has_workshop_pose():
		var row: int = WORKSHOP_ROWS[pose] + (4 if direction == NORTHEAST else 0)
		var region := Rect2(Vector2(frame, row) * CELL, CELL)
		canvas.draw_texture_rect_region(WORKSHOP_POSES[actor.weapon], Rect2(-GROUND, CELL), region)
		return
	var region := Rect2(Vector2(direction, pose * 4 + frame) * CELL, CELL)
	if not GRIPS.OFFSETS.has(pose):
		canvas.draw_texture_rect_region(BODY, Rect2(-GROUND, CELL), region)
		return

	var hand: Vector2 = GRIPS.OFFSETS[pose][direction][frame]
	var facing := Vector2.from_angle((direction + 2) * PI / 4)
	var angle := facing.angle() - 0.9
	if pose == Pose.LIGHT or pose == Pose.HEAVY:
		var edge: float = actor.attack_direction.angle() + actor.swing_offset(actor.swing_progress())
		var target: Vector2 = actor.projected_swing_origin() + Isometric.GROUND * (
			Vector2.from_angle(edge) * float(actor.swing_reach()) - Vector2(actor.render_position)
		)
		angle = hand.angle_to_point(target)
	elif pose == Pose.RUN:
		angle = deg_to_rad(GRIPS.RUN_ANGLES[direction][frame])

	var behind := direction in [3, 4, 5]
	if behind:
		_draw_weapon(canvas, actor.weapon, hand, angle)
	canvas.draw_texture_rect_region(BODY, Rect2(-GROUND, CELL), region)
	if not behind:
		_draw_weapon(canvas, actor.weapon, hand, angle)
		# Restore the glove over the handle where the two sprites overlap.
		var glove := Rect2(hand - Vector2(2, 2), Vector2(4, 4))
		canvas.draw_texture_rect_region(BODY, glove, Rect2(region.position + GROUND + glove.position, glove.size))


func _has_workshop_pose() -> bool:
	return direction in [SOUTHWEST, NORTHEAST] and WORKSHOP_ROWS.has(pose)


func _draw_weapon(canvas: CanvasItem, index: int, hand: Vector2, angle: float) -> void:
	canvas.draw_set_transform(hand, angle)
	canvas.draw_texture(WEAPONS[index], -WEAPON_GRIP)
	canvas.draw_set_transform(Vector2.ZERO)
