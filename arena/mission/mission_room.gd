class_name MissionRoom
extends Node2D

const LAYOUTS := [
	{
		"size": Vector2(1760, 570),
		"spawns": [Vector2(890, 285)],
		"fixtures": [270, 545, 1030, 1430],
	},
	{
		"size": Vector2(2130, 700),
		"spawns": [Vector2(775, 205), Vector2(1510, 490)],
		"fixtures": [270, 575, 920, 1325, 1650, 1910],
	},
]
const GATE_RADIUS := 126.0
const INK := Color("#1b242c")
const BRASS := Color("#c09563")
const RED := Color("#ee856a")
const GREEN := Color("#a2d68f")
const TILE := 76

var number := 0
var bounds := Rect2()
var gate := Vector2.ZERO
var enemy_spawns: Array = []
var _floor := Rect2()
var _open := false
var _prompt := ""


func _ready() -> void:
	ControlHints.changed.connect(queue_redraw)


func configure(next: int) -> void:
	number = next
	visible = number > 0
	_open = false
	_prompt = ""
	if visible:
		var layout: Dictionary = LAYOUTS[number - 1]
		_floor = Rect2(Vector2.ZERO, layout.size)
		bounds = Rect2(28, 72, _floor.size.x - 56, _floor.size.y - 144)
		gate = Vector2(_floor.end.x - 130, _floor.get_center().y)
		enemy_spawns = layout.spawns
	queue_redraw()


func spawn_point(slot: int) -> Vector2:
	return Vector2(135 + 44 * int(slot >= 2), _floor.get_center().y + 48 * (slot % 2) - 24)


func update_gate(unlocked: bool, prompt: String) -> void:
	if _open != unlocked or _prompt != prompt:
		_open = unlocked
		_prompt = prompt
		queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


func _draw() -> void:
	if number == 0:
		return
	var pumping := number == 2
	var end := _floor.end
	var center_y := _floor.get_center().y
	var steel := Color("#465563") if pumping else Color("#505765")
	var accent := Color("#74b7ae") if pumping else Color("#dcaf75")
	draw_set_transform_matrix(Isometric.GROUND)
	draw_rect(_floor.grow(110), Color("#25323a") if pumping else Color("#262e39"))
	if pumping:
		for y in [-102.0, end.y + 34.0]:
			draw_rect(Rect2(-45, y, end.x + 90, 68), Color("#305861"))
			for x in range(0, int(end.x), 49):
				draw_line(Vector2(x, y + 14 + (x % 5) * 8), Vector2(x + 26, y + 14 + (x % 5) * 8), Color("#4e7d7e"), 2)
	draw_rect(_floor.grow(11), INK)
	draw_rect(_floor, steel)
	for y in range(0, int(end.y), TILE):
		for x in range(0, int(end.x), TILE):
			var cell := Vector2(x, y)
			var size := (end - cell).min(Vector2(TILE, TILE))
			var shade := float((x * 13 + y * 19) % 53) / 650.0
			draw_rect(Rect2(cell + Vector2.ONE, size - Vector2(2, 2)), steel.lightened(shade))
			draw_line(cell + Vector2(3, 3), cell + Vector2(size.x - 3, 3), steel.lightened(shade + 0.05), 1)
			draw_circle(cell + Vector2(7, 7), 1.4, INK, true, -1, true)
	for x in ([350, 955, 1585] if pumping else [330, 700, 1210]):
		for y in [105.0, end.y - 177.0]:
			_draw_grate(Rect2(x, y, 116, 70), steel, accent)
	var stripe := Color("#34454d", 0.7)
	draw_rect(Rect2(0, center_y - 55, end.x, 110), stripe)
	for y in [center_y - 55, center_y + 55]:
		draw_line(Vector2(0, y), Vector2(end.x, y), accent.darkened(0.32), 2, true)
	for x in range(315, int(end.x) - 220, 245):
		var arrow := Vector2(x, center_y)
		draw_polyline(PackedVector2Array([arrow + Vector2(-9, -8), arrow + Vector2(4, 0), arrow + Vector2(-9, 8)]), accent.darkened(0.15), 3, true)
	for y in [60.0, end.y - 60.0]:
		draw_line(Vector2(0, y), Vector2(end.x, y), BRASS.darkened(0.35), 3)
		for x in range(9, int(end.x) - 14, 24):
			draw_line(Vector2(x, y - 4), Vector2(x + 9, y + 4), BRASS.darkened(0.25), 2)
	draw_rect(Rect2(17, center_y - 75, 54, 150), steel.darkened(0.29))
	for x in range(22, 66, 8):
		draw_line(Vector2(x, center_y - 70), Vector2(x, center_y + 70), steel.lightened(0.21), 2)
	var ring := GREEN if _open else RED
	draw_circle(gate, GATE_RADIUS, Color(ring, 0.12), true, -1, true)
	draw_arc(gate, GATE_RADIUS, 0, TAU, 72, Color(ring, 0.73), 3, true)
	draw_set_transform_matrix(Transform2D.IDENTITY)

	_block(Rect2(0, 0, end.x, 25), 31, steel.darkened(0.22))
	_block(Rect2(0, end.y - 20, end.x, 20), 12, steel.darkened(0.25))
	for x: int in LAYOUTS[number - 1].fixtures:
		_fixture(Vector2(x, 29), accent, pumping)
		if pumping:
			_fixture(Vector2(x + 60, end.y - 28), accent.darkened(0.22), false)
	_portal(Vector2(24, center_y), BRASS, false, true)
	_portal(Vector2(end.x - 15, center_y), ring, true, _open)
	_label(Isometric.GROUND * Vector2(24, center_y) + Vector2(0, -83), tr("ROOM_ENTRANCE"), BRASS)
	var label_position := Isometric.GROUND * Vector2(end.x - 15, center_y) + Vector2(0, -83)
	_label(label_position, tr("ROOM_GATE_OPEN" if _open else "ROOM_GATE_LOCKED"), ring)
	if not _prompt.is_empty():
		var message := tr(_prompt)
		if _prompt != "ROOM_GATE_WAIT_HOST":
			message = "[%s]  %s" % [ControlHints.binding_label("interact", ControlHints.using_gamepad, true), message]
		_label(label_position + Vector2(0, -25), message, Color("#f6e8d1"))


func _draw_grate(rect: Rect2, steel: Color, accent: Color) -> void:
	draw_rect(rect.grow(5), steel.darkened(0.36))
	draw_rect(rect, accent.darkened(0.40), false, 2)
	for x in range(7, int(rect.size.x) - 6, 7):
		var start := rect.position + Vector2(x, 4)
		draw_line(start, start + Vector2(0, rect.size.y - 8), steel.lightened(0.08), 2, true)
	draw_line(rect.get_center() - Vector2(rect.size.x * 0.5, 0), rect.get_center() + Vector2(rect.size.x * 0.5, 0), steel.darkened(0.26), 5)


func _block(rect: Rect2, height: float, color: Color) -> void:
	var a := Isometric.GROUND * rect.position
	var b := Isometric.GROUND * Vector2(rect.end.x, rect.position.y)
	var c := Isometric.GROUND * rect.end
	var d := Isometric.GROUND * Vector2(rect.position.x, rect.end.y)
	var rise := Vector2(0, -height)
	draw_colored_polygon(PackedVector2Array([b, c, c + rise, b + rise]), color.darkened(0.39))
	draw_colored_polygon(PackedVector2Array([c, d, d + rise, c + rise]), color.darkened(0.24))
	draw_colored_polygon(PackedVector2Array([a + rise, b + rise, c + rise, d + rise]), color)
	draw_polyline(PackedVector2Array([b + rise, c + rise, d + rise]), color.lightened(0.18), 2, true)


func _fixture(point: Vector2, accent: Color, pump: bool) -> void:
	var size := Vector2(83, 38) if pump else Vector2(62, 29)
	var height := 46.0 if pump else 28.0
	_block(Rect2(point - size * 0.5, size), height, Color("#51666a"))
	var p := Isometric.GROUND * point
	var light := p + Vector2(6, -height - 5)
	draw_circle(light, 20, Color(accent, 0.09), true, -1, true)
	draw_circle(light, 13, Color(accent, 0.14), true, -1, true)
	draw_circle(light, 6, INK, true, -1, true)
	draw_circle(light, 3.5, accent, true, -1, true)
	draw_line(light + Vector2(-18, 5), light + Vector2(-43, -10), BRASS.darkened(0.30), 5, true)


func _portal(point: Vector2, accent: Color, exit: bool, open: bool) -> void:
	var a := Isometric.GROUND * (point - Vector2(0, 52))
	var b := Isometric.GROUND * (point + Vector2(0, 52))
	var rise := Vector2(0, -54)
	if not open:
		draw_colored_polygon(PackedVector2Array([a, b, b + rise, a + rise]), Color("#333b4a"))
		for i in range(1, 5):
			draw_line(a.lerp(b, i / 5.0), (a + rise).lerp(b + rise, i / 5.0), Color("#65717d"), 3, true)
	else:
		draw_line(a, b, accent.darkened(0.35), 4, true)
	for edge in [a, b]:
		draw_line(edge, edge + rise, INK, 13, true)
		draw_line(edge, edge + rise, Color("#77838a"), 7, true)
	draw_line(a + rise, b + rise, INK, 16, true)
	draw_line(a + rise - Vector2(0, 2), b + rise - Vector2(0, 2), Color("#65747c"), 9, true)
	var lamp := (a + b) * 0.5 + rise
	draw_circle(lamp, 8, INK, true, -1, true)
	draw_circle(lamp, 4.5 if exit else 3.0, accent, true, -1, true)


func _label(point: Vector2, value: String, color: Color) -> void:
	var font := ThemeDB.fallback_font
	var size := font.get_string_size(value, HORIZONTAL_ALIGNMENT_LEFT, -1, 14)
	draw_rect(Rect2(point - Vector2(size.x / 2 + 7, 17), Vector2(size.x + 14, 22)), INK)
	draw_string(font, point - Vector2(size.x / 2, 0), value, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, color)
