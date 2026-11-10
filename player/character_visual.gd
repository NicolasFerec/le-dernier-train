extends Node2D

const INK := Color("#202630")
const STEEL := Color("#c3d6d5")
const BRASS := Color("#e6b379")
const ATTACK_SHEET := preload("res://player/fighter_sword_attack.png")
const ATTACK_CELL := Vector2(128, 96)
const ATTACK_ANCHOR := Vector2(48, 72)

var actor: Node2D
var gait := 0.0
var breathing := 0.0


func _ready() -> void:
	_process(0.0)


func _process(delta: float) -> void:
	if not actor.paused:
		var moving: bool = actor.render_movement != Vector2.ZERO
		gait = fmod(gait + delta * 12.0, TAU) if moving else 0.0
		breathing = fmod(breathing + delta * 3.0, TAU)
	var projected := Isometric.GROUND * Vector2(actor.render_position)
	position = projected - actor.position
	z_index = Isometric.depth(projected.y)
	queue_redraw()


func _draw() -> void:
	if actor.hp <= 0:
		draw_line(Vector2(-13, 3), Vector2(10, -3), INK, 13, true)
		draw_line(Vector2(-10, 3), Vector2(6, -2), Color("#546069"), 8, true)
		draw_circle(Vector2(12, -4), 6, Color("#8b786e"), true, -1, true)
		draw_arc(Vector2(0, 2), 15, 0.25, PI - 0.25, 20, Color("#bd7782"), 2, true)
		return
	var direction := (Isometric.GROUND * Vector2(actor.render_facing)).angle()
	var facing := Vector2.from_angle(snappedf(direction, PI / 4))
	var attack := clampf((actor.attack_time - actor.attack_delay + actor.SWING_TIME) / actor.SWING_TIME, 0.0, 1.0)
	var attack_angle: float = actor.attack_direction.angle()
	var half_angle := acos(actor.SWING_MIN_DOT)
	var swing_edge := attack_angle + lerpf(half_angle, -half_angle, attack)
	var weapon_facing := (Isometric.GROUND * Vector2(actor.attack_direction)).normalized() if attack > 0 else facing
	var dashing: bool = actor.render_dash_time > 0
	var scout: bool = actor.character == 1
	var coat := Color("#376965") if scout else Color("#78433b")
	var cloth := Color("#69af9a") if scout else Color("#bb7757")
	var team: Color = actor.COLORS[actor.slot]
	var stride := sin(gait) * 5.0
	var lift := sin(breathing) * 0.8 + absf(sin(gait)) * 2.0
	var lean := facing.x * (7.0 if dashing else 3.5 * attack)
	var hip := Vector2(lean, -15 - lift)
	var shoulder := hip + Vector2(facing.x * 2, -14 if dashing else -17)
	var head := shoulder + Vector2(facing.x * 3, -9)
	var width := 0.68 if absf(facing.x) > 0.9 else 1.0
	var front_hand := shoulder + Vector2(10 * width, 11) + weapon_facing * (4 + 12 * attack)
	var rear_hand := shoulder + Vector2(-11 * width, 10) - facing * stride * 0.45
	if weapon_facing.x < -0.1:
		front_hand.x -= 17 * width
		rear_hand.x += 22 * width

	draw_set_transform(Vector2.ZERO, 0, Vector2(1, 0.45))
	draw_circle(Vector2.ZERO, 17, Color("#16191d", 0.55))
	draw_arc(Vector2.ZERO, 15, 0.25, PI - 0.25, 20, team, 2, true)
	draw_set_transform(Vector2.ZERO)
	if dashing:
		for offset in [0, 6, 12]:
			var start := Vector2(0, -8 - offset) - facing * 12
			draw_line(start, start - facing * (30 - offset), Color(team, 0.54), 3, true)
	if attack > 0:
		_draw_swing(attack, team, attack_angle - half_angle, swing_edge)

	if not scout and actor.weapon == 0:
		var frame := 3
		if attack > 0 and not dashing:
			frame = mini(3, floori(attack * 4.0))
		var cell := Vector2(frame % 2, frame >> 1) * ATTACK_CELL
		var flip := weapon_facing.x < -0.1
		if flip:
			draw_set_transform(Vector2.ZERO, 0, Vector2(-1, 1))
		draw_texture_rect_region(ATTACK_SHEET, Rect2(-ATTACK_ANCHOR, ATTACK_CELL), Rect2(cell, ATTACK_CELL))
		draw_set_transform(Vector2.ZERO)
		return

	for side in [-1, 1]:
		var foot: Vector2 = Vector2(side * 6 * width, -1) + facing * side * stride
		_limb(hip + Vector2(side * 4, 0), foot, 6, coat.darkened(0.35))
		_limb(foot, foot + Vector2(facing.x * 4, 0), 4, INK)
	_limb(shoulder + Vector2(8 * signf(rear_hand.x - shoulder.x), 0), rear_hand, 5, coat)

	var hem := hip + Vector2(0, 7)
	draw_colored_polygon(PackedVector2Array([
		shoulder + Vector2(-10 * width, -5), shoulder + Vector2(10 * width, -5),
		hem + Vector2(10 * width, 0), hem + Vector2(-10 * width, 0)
	]), INK)
	draw_colored_polygon(PackedVector2Array([
		shoulder + Vector2(-8 * width, -4), shoulder + Vector2(8 * width, -4),
		hem + Vector2(8 * width, -2), hem + Vector2(-8 * width, -2)
	]), coat)
	draw_line(hip - Vector2(8 * width, 0), hip + Vector2(8 * width, 0), BRASS.darkened(0.25), 2, true)
	if facing.y >= -0.1:
		draw_line(shoulder + Vector2(-6 * width, -2), hip + Vector2(5 * width, 2), cloth, 3, true)
	else:
		draw_rect(Rect2(shoulder + Vector2(-5 * width, 1), Vector2(10 * width, 11)), cloth.darkened(0.28))
		draw_line(shoulder + Vector2(0, 1), hip, BRASS.darkened(0.3), 2, true)
	_limb(shoulder + Vector2(6 * signf(front_hand.x - shoulder.x), 0), front_hand, 6, cloth)
	draw_circle(front_hand, 3, BRASS.darkened(0.22))

	draw_circle(head, 9 if scout else 10, INK, true, -1, true)
	draw_circle(head, 7.5 if scout else 8.3, cloth, true, -1, true)
	if facing.y >= -0.1:
		draw_circle(head + Vector2(3 * facing.x, 2), 5.0, Color("#d6a97d"), true, -1, true)
		var goggles := head + Vector2(4 * facing.x, 0)
		draw_line(goggles - Vector2(3 * width, 0), goggles + Vector2(3 * width, 0), INK, 3, true)
		draw_circle(goggles + Vector2(2 * width, -0.5), 1.0, STEEL)
	else:
		draw_line(head + Vector2(-5 * width, 0), head + Vector2(5 * width, 0), coat, 3, true)
	if not scout:
		draw_line(head + Vector2(-9, -2), head + Vector2(9, -2), INK, 3, true)
		draw_line(head + Vector2(-6, -5), head + Vector2(5, -6), BRASS, 2, true)

	var weapon_angle := facing.angle() - 1.25
	if attack > 0:
		weapon_angle = (Isometric.GROUND * Vector2.from_angle(swing_edge)).angle()
	if scout and actor.weapon == 1:
		_draw_weapon(rear_hand, weapon_angle - 0.7, true)
	_draw_weapon(front_hand, weapon_angle, scout)


func _limb(start: Vector2, end: Vector2, width: float, color: Color) -> void:
	draw_line(start, end, INK, width + 2, true)
	draw_line(start, end, color, width, true)


func _draw_weapon(hand: Vector2, angle: float, scout: bool) -> void:
	var axis := Vector2.from_angle(angle)
	var cross := axis.orthogonal()
	if not scout and actor.weapon == 1:
		_limb(hand - axis * 4, hand + axis * 19, 3, BRASS.darkened(0.2))
		var tip := hand + axis * 17
		_limb(tip - cross * 8, tip + cross * 8, 7, STEEL.darkened(0.26))
		draw_line(tip - cross * 7 + axis * 2, tip + cross * 7 + axis * 2, BRASS, 2, true)
		return
	var length := 24.0 if actor.weapon == 0 else 12.0
	var blade_width := 1.0 if scout and actor.weapon == 0 else 2.5
	_limb(hand - cross * 5, hand + cross * 5, 2, BRASS)
	draw_colored_polygon(PackedVector2Array([
		hand + cross * blade_width, hand + axis * length, hand - cross * blade_width
	]), STEEL)


func _draw_swing(attack: float, color: Color, start_angle: float, edge: float) -> void:
	var points := PackedVector2Array()
	var origin := Isometric.GROUND * Vector2(actor.attack_origin - actor.render_position) + Vector2(0, -13)
	for index in range(16):
		var ray := Vector2.from_angle(lerpf(maxf(start_angle, edge - 1.6), edge, index / 15.0))
		points.append(origin + Isometric.GROUND * (ray * actor.reach))
	draw_polyline(points, Color(color, 0.55 * sin(PI * attack)), 4, true)
