extends Node2D
class_name TrainLobby

const FLOOR := Rect2(0, 0, 2460, 520)
const PRACTICE := Rect2(1440, 147, 350, 251)
const BOUNDS := Rect2(30, 76, 2400, 408)
const SPAWNS := [Vector2(285, 245), Vector2(225, 295), Vector2(345, 295), Vector2(285, 345)]
const DEPARTURE_STATION := 3
const DEPARTURE_RADIUS := 90.0
const INVITATION_STATION := 4
const STATIONS := [
	[Vector2(310, 119), "CHARACTER_STATION", Color("#63c0bc")],
	[Vector2(830, 119), "ARSENAL_STATION", Color("#dfad6b")],
	[Vector2(1125, 119), "PASSIVES_STATION", Color("#b4abeb")],
	[Vector2(2195, 167), "DEPARTURE_STATION", Color("#8ccfa3")],
	[Vector2(520, 141), "STEAM_INVITATION_STATION", Color("#8dc2f4")],
]
const DUMMIES := [Vector2(1515, 226), Vector2(1615, 333), Vector2(1720, 225)]
const JOINTS := [655.0, 1355.0, 1915.0]
const WOOD := preload("res://arena/train/walnut_floor.webp")
const STEEL_TREAD := preload("res://arena/train/steel_tread.webp")
const INK := Color("#1d2228")
const BRASS := Color("#d1a065")

var _interactable_station := -1
var impacts := PackedFloat32Array([0, 0, 0])


func _ready() -> void:
	ControlHints.changed.connect(queue_redraw)


func show_interaction(station: int) -> void:
	if _interactable_station != station:
		_interactable_station = station
		queue_redraw()


func flash(index: int) -> void:
	if index >= 0 and index < DUMMIES.size():
		impacts[index] = 0.24
		queue_redraw()


func _process(delta: float) -> void:
	for index in impacts.size():
		if impacts[index] > 0:
			impacts[index] = maxf(0, impacts[index] - delta)
			queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


func _draw() -> void:
	draw_set_transform_matrix(Isometric.GROUND)
	draw_rect(FLOOR.grow(100), Color("#252e37"))
	for y in [-74, -49, 569, 594]:
		draw_line(Vector2(-118, y), Vector2(2578, y), Color("#3e4952"), 5, true)
	for x in range(-100, 2561, 44):
		for y in [-61, 581]:
			draw_rect(Rect2(x, y - 19, 11, 38), Color("#4c565d"))
	draw_rect(FLOOR.grow(10), INK)
	_draw_surface(FLOOR, WOOD, 260, Color("#d3c2af"))
	draw_rect(Rect2(8, 202, 2444, 116), Color("#32494d", 0.32))
	for y in [202, 318]:
		draw_line(Vector2(11, y), Vector2(2449, y), Color("#b4ab85", 0.58), 2, true)
	for x in JOINTS:
		draw_rect(Rect2(x - 12, 0, 24, 520), Color("#38434b"))
		for rib in [-7, 0, 7]:
			draw_line(Vector2(x + rib, 0), Vector2(x + rib, 520), Color("#6a777c"), 2)
	for y in [2, 508]:
		draw_rect(Rect2(0, y, 2460, 10), Color("#384750"))
		for x in range(15, 2460, 26):
			draw_circle(Vector2(x, y + 5), 2, BRASS)
	_draw_surface(PRACTICE, STEEL_TREAD, 125, Color("#93a4a8"))
	draw_rect(PRACTICE, BRASS, false, 3)
	for index in STATIONS.size():
		var station: Array = STATIONS[index]
		var radius := DEPARTURE_RADIUS if index == DEPARTURE_STATION else 49.0
		draw_circle(station[0], radius, Color("#29383c", 0.8))
		draw_arc(station[0], radius, 0, TAU, 48, station[2], 3, true)
	for point in DUMMIES:
		draw_circle(point, 30, Color("#242c34"))
		draw_arc(point, 28, 0, TAU, 32, Color("#b18457"), 2, true)
	draw_set_transform_matrix(Transform2D.IDENTITY)

	# Low rear bulkhead, windows and brass framing in projected space.
	_wall_segment(0, 2460, 31, Color("#426169"))
	for x in range(28, 2420, 112):
		var a := Isometric.GROUND * Vector2(x, 0)
		var b := Isometric.GROUND * Vector2(x + 75, 0)
		var rise := Vector2(0, -23)
		draw_colored_polygon(PackedVector2Array([a + rise, b + rise, b + Vector2(0, -8), a + Vector2(0, -8)]), Color("#6ea0aa"))
		draw_line(a + rise, b + rise, BRASS, 2, true)
		draw_line(a + Vector2(0, -7), b + Vector2(0, -7), INK, 2, true)
	_wall_segment(0, 2460, 4, BRASS)
	for x in [85, 520, 715, 1000, 1265, 1830, 1995, 2360]:
		_draw_lamp(Isometric.GROUND * Vector2(x, 43))
	for x in [75, 470, 573, 718, 1220, 1825, 1970, 2340]:
		_draw_crate(Vector2(x, 52), 34, Color("#665643"))
	for x in [212, 926, 1390, 2038, 2296]:
		_draw_crate(Vector2(x, 488), 26, Color("#485a5d"))
	for x in [180, 770, 1070, 2010, 2280]:
		_draw_bench(Vector2(x, 34))
	for index in STATIONS.size():
		_draw_station(index)
	for index in DUMMIES.size():
		_draw_dummy(index)
	_label(Isometric.GROUND * Vector2(1615, 153) + Vector2(0, -24), tr("TRAIN_PRACTICE"), BRASS)
	var practice_controls := "TRAIN_PRACTICE_GAMEPAD" if ControlHints.using_gamepad else "TRAIN_PRACTICE_KEYBOARD"
	_label(Isometric.GROUND * Vector2(1615, 153) + Vector2(0, -5), tr(practice_controls), Color("#f4e0be"), 11)


func _draw_surface(area: Rect2, texture: Texture2D, tile_size: int, tint: Color) -> void:
	for y in range(0, int(area.size.y), tile_size):
		for x in range(0, int(area.size.x), tile_size):
			var origin := area.position + Vector2(x, y)
			var size := Vector2(tile_size, tile_size).min(area.end - origin)
			var source := Rect2(Vector2.ZERO, texture.get_size() * size / tile_size)
			draw_texture_rect_region(texture, Rect2(origin, size), source, tint)


func _wall_segment(start: float, end: float, height: float, color: Color) -> void:
	var a := Isometric.GROUND * Vector2(start, 0)
	var b := Isometric.GROUND * Vector2(end, 0)
	var top := Vector2(0, -height)
	draw_colored_polygon(PackedVector2Array([a, b, b + top, a + top]), color)
	draw_line(a + top, b + top, INK, 2, true)


func _draw_lamp(point: Vector2) -> void:
	draw_set_transform(point)
	draw_line(Vector2.ZERO, Vector2(0, -25), INK, 5, true)
	for radius in [24, 17, 10]:
		draw_circle(Vector2(0, -25), radius, Color("#ffc170", 0.08))
	draw_rect(Rect2(-4, -31, 8, 9), Color("#ffc779"))
	draw_line(Vector2(-6, -32), Vector2(6, -32), INK, 3, true)
	draw_set_transform(Vector2.ZERO)


func _draw_crate(point: Vector2, width: float, color: Color) -> void:
	var half := width * 0.5
	var corners := PackedVector2Array([
		Isometric.GROUND * (point + Vector2(-half, -half)), Isometric.GROUND * (point + Vector2(half, -half)),
		Isometric.GROUND * (point + Vector2(half, half)), Isometric.GROUND * (point + Vector2(-half, half))
	])
	var rise := Vector2(0, -15)
	draw_colored_polygon(PackedVector2Array([corners[1], corners[2], corners[2] + rise, corners[1] + rise]), color.darkened(0.38))
	draw_colored_polygon(PackedVector2Array([corners[2], corners[3], corners[3] + rise, corners[2] + rise]), color.darkened(0.25))
	draw_colored_polygon(PackedVector2Array([corners[0] + rise, corners[1] + rise, corners[2] + rise, corners[3] + rise]), color)
	draw_polyline(PackedVector2Array([corners[1] + rise, corners[2] + rise, corners[3] + rise]), BRASS.darkened(0.25), 2, true)
	draw_line(corners[0] + rise, corners[2] + rise, INK, 2, true)


func _draw_bench(point: Vector2) -> void:
	var a := Isometric.GROUND * (point - Vector2(34, 0))
	var b := Isometric.GROUND * (point + Vector2(34, 0))
	var rise := Vector2(0, -12)
	draw_line(a, a + rise, INK, 6, true)
	draw_line(b, b + rise, INK, 6, true)
	draw_line(a + rise, b + rise, Color("#523e34"), 13, true)
	draw_line(a + rise - Vector2(0, 2), b + rise - Vector2(0, 2), Color("#8a6750"), 8, true)
	draw_line(a + rise - Vector2(0, 6), b + rise - Vector2(0, 6), Color("#b58b62"), 2, true)


func _draw_station(index: int) -> void:
	var station: Array = STATIONS[index]
	var center := Isometric.GROUND * Vector2(station[0])
	var accent: Color = station[2]
	draw_set_transform(center)
	draw_colored_polygon(PackedVector2Array([Vector2(-28, -13), Vector2(0, -28), Vector2(28, -13), Vector2(28, 1),
		Vector2(0, 16), Vector2(-28, 1)]), INK)
	draw_colored_polygon(PackedVector2Array([Vector2(-26, -14), Vector2(0, -28), Vector2(26, -14), Vector2(0, 0)]), Color("#5a6869"))
	draw_line(Vector2(-25, -13), Vector2(0, 1), accent, 2, true)
	draw_line(Vector2(0, 1), Vector2(25, -13), accent, 2, true)
	draw_rect(Rect2(-14, -41, 28, 24), INK)
	draw_rect(Rect2(-11, -38, 22, 18), accent.darkened(0.45))
	match index:
		0:
			draw_circle(Vector2(0, -32), 4, accent)
			draw_arc(Vector2(0, -20), 8, PI, TAU, 12, accent, 2, true)
		1:
			draw_line(Vector2(-6, -24), Vector2(7, -35), accent, 3, true)
			draw_line(Vector2(-6, -30), Vector2(-1, -24), BRASS, 2, true)
		2:
			for offset in [Vector2(-6, -30), Vector2(0, -34), Vector2(6, -27)]:
				draw_line(Vector2(0, -24), offset, accent, 1, true)
				draw_circle(offset, 2, accent)
		DEPARTURE_STATION:
			draw_line(Vector2(-6, -30), Vector2(7, -30), accent, 2, true)
			draw_polyline(PackedVector2Array([Vector2(2, -35), Vector2(7, -30), Vector2(2, -25)]), accent, 2, true)
		INVITATION_STATION:
			var signal_center := Vector2(0, -25)
			draw_circle(signal_center, 2, accent)
			for radius in [5.0, 9.0]:
				draw_arc(signal_center, radius, -PI * 0.81, -PI * 0.19, 14, accent, 1.5, true)
	draw_set_transform(Vector2.ZERO)
	var title := tr(station[1])
	var title_anchor := center + Vector2(0, -50)
	_label(title_anchor, title, accent)
	if index == _interactable_station:
		var font := ThemeDB.fallback_font
		var title_width := font.get_string_size(title, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
		var badge := title_anchor + Vector2(-title_width / 2 - 20, -4)
		var key := "A" if ControlHints.using_gamepad else "E"
		if ControlHints.using_gamepad:
			draw_circle(badge, 11, INK, true, -1, true)
			draw_circle(badge, 9, Color("#a2d68f"), true, -1, true)
		else:
			draw_rect(Rect2(badge - Vector2(10, 10), Vector2(20, 20)), INK)
			draw_rect(Rect2(badge - Vector2(8, 8), Vector2(16, 16)), Color("#f4e0be"))
		var key_width := font.get_string_size(key, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
		draw_string(font, badge + Vector2(-key_width / 2, 4), key, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, INK)


func _draw_dummy(index: int) -> void:
	var point: Vector2 = Isometric.GROUND * DUMMIES[index]
	var impact := impacts[index] / 0.24
	draw_set_transform(point)
	draw_line(Vector2.ZERO, Vector2(0, -18), INK, 7, true)
	draw_line(Vector2(-11, -25), Vector2(11, -25), BRASS.darkened(0.35), 5, true)
	var center := Vector2(impact * 6, -29)
	draw_circle(center, 13, INK)
	draw_circle(center, 10, BRASS.lerp(Color("#fff7bb"), impact))
	draw_arc(center, 6, 0, TAU, 24, Color("#944947"), 3, true)
	draw_circle(center, 2, INK)
	if impact > 0:
		for angle in range(0, 360, 90):
			var ray := Vector2.from_angle(deg_to_rad(angle + 30))
			draw_line(center + ray * 15, center + ray * (19 + 10 * (1 - impact)), Color("#ffdf86", impact), 2, true)
	draw_set_transform(Vector2.ZERO)


func _label(point: Vector2, label: String, color: Color, size := 13) -> void:
	var font := ThemeDB.fallback_font
	var width := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x
	draw_rect(Rect2(point - Vector2(width / 2 + 6, size), Vector2(width + 12, size + 6)), INK)
	draw_string(font, point - Vector2(width / 2, 1), label, HORIZONTAL_ALIGNMENT_LEFT, -1, size, color)
