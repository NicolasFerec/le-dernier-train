extends Node2D
class_name TrainLobby

const FLOOR := Rect2(0, 0, 1560, 360)
const BOUNDS := Rect2(30, 73, 1500, 251)
const SPAWNS := [Vector2(480, 149), Vector2(435, 178), Vector2(525, 178), Vector2(480, 220)]
const DEPARTURE_STATION := 3
const DEPARTURE_RADIUS := 90.0
const INVITATION_STATION := 4
const STATIONS := [
	[Vector2(148, 100), "CHARACTER_STATION", Color("#63c0bc")],
	[Vector2(480, 105), "ARSENAL_STATION", Color("#dfad6b")],
	[Vector2(647, 103), "PASSIVES_STATION", Color("#b4abeb")],
	[Vector2(1370, 161), "DEPARTURE_STATION", Color("#8ccfa3")],
	[Vector2(310, 105), "STEAM_INVITATION_STATION", Color("#8dc2f4")],
]
const DUMMIES := [Vector2(889, 159), Vector2(972, 257), Vector2(1055, 153)]
const PRACTICE_AREA := Rect2(814, 108, 318, 200)
const PRACTICE_LABEL := Vector2(976, 91)
const WALL_BAY_WIDTH := 390
const WALL_HEIGHT := 116.0
const SKIRT_HEIGHT := 32.0
const FLOOR_TEXTURE := preload("res://arena/train/floor.webp")
const WALL := preload("res://arena/train/wall.webp")
const END_WALL := preload("res://arena/train/end_wall.webp")
const PRACTICE_FLOOR := preload("res://arena/train/practice_floor.webp")
const RAILWAY := preload("res://arena/train/railway.webp")
const SKIRT := preload("res://arena/train/skirt.webp")
const LIGHT_POOL := preload("res://arena/train/light_pool.tres")
const SHADOW := preload("res://arena/isometric_shadow.tres")
const INK := Color("#1d2228")
const BRASS := Color("#d1a065")

# Furniture is drawn in screen space. Its feet sit on the projected grid.
const STATION_PROPS := [
	[preload("res://arena/train/wardrobe.webp"), Vector2(148, 60), 88.0],
	[preload("res://arena/train/workbench.webp"), Vector2(480, 61), 77.0],
	[preload("res://arena/train/lectern.webp"), Vector2(647, 61), 75.0],
	[preload("res://arena/train/timetable.webp"), Vector2(1370, 98), 100.0],
	[preload("res://arena/train/telegraph.webp"), Vector2(310, 63), 79.0],
]
const CRATE := preload("res://arena/train/crate.webp")
const LANTERN := preload("res://arena/train/lantern.webp")
const TARGET := preload("res://arena/train/dummy.webp")
const TARGET_HEIGHT := 74.0
const IMPACT_DURATION := 0.24
const CRATES := [Vector2(66, 51), Vector2(754, 55), Vector2(1168, 57), Vector2(1478, 62),
	Vector2(222, 334), Vector2(703, 334), Vector2(1225, 333)]
const LANTERNS := [Vector2(78, 330), Vector2(415, 336), Vector2(807, 63), Vector2(1168, 330),
	Vector2(1487, 328)]

var _interactable_station := -1
var impacts := PackedFloat32Array()
var _dummy_visuals: Array[Sprite2D] = []


func _ready() -> void:
	ControlHints.changed.connect(queue_redraw)
	impacts.resize(DUMMIES.size())
	for prop in STATION_PROPS:
		_add_prop(prop[0], prop[1], prop[2])
	for point in CRATES:
		_add_prop(CRATE, point, 34.0)
	for point in LANTERNS:
		_add_prop(LANTERN, point, 49.0)
	for index in DUMMIES.size():
		var dummy := _add_prop(TARGET, DUMMIES[index], TARGET_HEIGHT)
		dummy.name = "PracticeTarget%d" % index
		dummy.draw.connect(_draw_dummy_impact.bind(dummy, index))
		_dummy_visuals.append(dummy)


func _add_prop(texture: Texture2D, ground_position: Vector2, height: float) -> Sprite2D:
	var sprite := Sprite2D.new()
	sprite.texture = texture
	sprite.position = Isometric.GROUND * ground_position
	sprite.z_index = Isometric.depth(sprite.position.y)
	var factor := height / texture.get_height()
	sprite.scale = Vector2.ONE * factor
	# The front feet can extend just past the centre of the footprint.
	sprite.offset.y = -texture.get_height() * 0.45
	add_child(sprite)
	return sprite


func show_interaction(station: int) -> void:
	if _interactable_station != station:
		_interactable_station = station
		queue_redraw()


func flash(index: int) -> void:
	if index >= 0 and index < DUMMIES.size():
		impacts[index] = IMPACT_DURATION
		_dummy_visuals[index].queue_redraw()


func _process(delta: float) -> void:
	for index in impacts.size():
		if impacts[index] > 0:
			impacts[index] = maxf(0, impacts[index] - delta)
			var impact := impacts[index] / IMPACT_DURATION
			var dummy := _dummy_visuals[index]
			dummy.rotation = sin(impact * TAU) * impact * 0.16
			dummy.self_modulate = Color.WHITE.lerp(Color(1.35, 1.25, 1.12), impact)
			dummy.queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


func _draw() -> void:
	draw_set_transform_matrix(Isometric.GROUND.translated(Vector2(0, SKIRT_HEIGHT)))
	# The two tracks run parallel to the carriage on the very same grid as its floor.
	_draw_band(RAILWAY, Rect2(-1100, -20, 3760, 348), 320.0, Color("#8995a7"))
	_draw_band(RAILWAY, Rect2(-1100, 328, 3760, 360), 320.0, Color("#8290a3"))
	draw_set_transform_matrix(Isometric.GROUND)
	draw_texture_rect(FLOOR_TEXTURE, FLOOR, false)
	_draw_band(SKIRT, Rect2(0, 0, FLOOR.size.x, 10), 246.0)
	_draw_band(SKIRT, Rect2(0, FLOOR.end.y - 11, FLOOR.size.x, 11), 246.0)
	draw_texture_rect(PRACTICE_FLOOR, PRACTICE_AREA, false)
	_draw_floor_lights()
	draw_set_transform_matrix(Transform2D.IDENTITY)

	# Leave the near end open so that the camera can see the walkable floor.
	_draw_side(END_WALL, Vector2.ZERO, Isometric.GROUND.y, FLOOR.size.y, WALL_HEIGHT, 0.0, FLOOR.size.y)
	_draw_side(WALL, Vector2.ZERO, Isometric.GROUND.x, FLOOR.size.x, WALL_HEIGHT, 0.0, WALL_BAY_WIDTH)
	_draw_side(SKIRT, Isometric.GROUND * Vector2(0, FLOOR.size.y), Isometric.GROUND.x,
		FLOOR.size.x, SKIRT_HEIGHT, SKIRT_HEIGHT, 246.0)
	_draw_side(SKIRT, Isometric.GROUND * Vector2(FLOOR.size.x, 0), Isometric.GROUND.y,
		FLOOR.size.y, SKIRT_HEIGHT, SKIRT_HEIGHT, 260.0)

	for prop in STATION_PROPS:
		var texture: Texture2D = prop[0]
		_draw_prop_shadow(prop[1], prop[2] * texture.get_width() / texture.get_height())
	for point in CRATES:
		_draw_prop_shadow(point, 37.0)
	for point in LANTERNS:
		_draw_prop_shadow(point, 33.0)
	for point in DUMMIES:
		_draw_prop_shadow(point, 43.0)
	for index in STATIONS.size():
		_draw_station(index)
	_label(Isometric.GROUND * PRACTICE_LABEL + Vector2(0, -24), tr("TRAIN_PRACTICE"), BRASS)
	var practice_buttons := []
	for action in ["attack", "heavy_attack", "dodge"]:
		practice_buttons.append(ControlHints.binding_label(action, ControlHints.using_gamepad, true))
	_label(Isometric.GROUND * PRACTICE_LABEL + Vector2(0, -5), tr("TRAIN_PRACTICE_CONTROLS") % practice_buttons, Color("#f4e0be"), 11)


func _draw_floor_lights() -> void:
	for x in range(65, int(FLOOR.size.x), WALL_BAY_WIDTH / 3):
		draw_texture_rect(LIGHT_POOL, Rect2(Vector2(x - 64, -6), Vector2(128, 185)),
			false, Color(0.27, 0.54, 0.75, 0.25))
	for point in [STATIONS[1][0], PRACTICE_AREA.get_center(), STATIONS[DEPARTURE_STATION][0]]:
		draw_texture_rect(LIGHT_POOL, Rect2(point - Vector2(140, 110), Vector2(280, 250)),
			false, Color(0.92, 0.56, 0.24, 0.39))
	for point in LANTERNS:
		draw_texture_rect(LIGHT_POOL, Rect2(point - Vector2(80, 98), Vector2(160, 132)),
			false, Color(0.92, 0.56, 0.24, 0.3))


func _draw_band(texture: Texture2D, area: Rect2, tile_width: float, tint := Color.WHITE) -> void:
	var offset := 0.0
	while offset < area.size.x:
		var width := minf(tile_width, area.size.x - offset)
		var target := Rect2(area.position + Vector2(offset, 0), Vector2(width, area.size.y))
		var source := Rect2(Vector2.ZERO, texture.get_size() * Vector2(width / tile_width, 1))
		draw_texture_rect_region(texture, target, source, tint)
		offset += width


func _draw_side(texture: Texture2D, origin: Vector2, axis: Vector2,
		length: float, height: float, bottom: float, tile_width: float) -> void:
	draw_set_transform_matrix(Transform2D(axis, Vector2.DOWN, origin))
	_draw_band(texture, Rect2(0, bottom - height, length, height), tile_width)
	draw_set_transform_matrix(Transform2D.IDENTITY)


func _draw_prop_shadow(point: Vector2, width: float) -> void:
	var center := Isometric.GROUND * point
	draw_texture_rect(SHADOW, Rect2(center + Vector2(-width * 0.48, -12), Vector2(width * 1.15, 27)), false)


func _draw_station(index: int) -> void:
	var station: Array = STATIONS[index]
	var prop: Array = STATION_PROPS[index]
	var title_anchor := Isometric.GROUND * Vector2(prop[1]) - Vector2(0, prop[2] + 8.0)
	var accent: Color = station[2]
	var active := index == _interactable_station
	if active or index == DEPARTURE_STATION:
		var radius := DEPARTURE_RADIUS if index == DEPARTURE_STATION else 16.0
		draw_set_transform_matrix(Isometric.GROUND)
		draw_arc(station[0], radius, 0, TAU, 48, Color(accent, 0.64 if active else 0.25), 2, true)
		draw_set_transform_matrix(Transform2D.IDENTITY)
	_draw_station_label(index, title_anchor, accent)


func _draw_station_label(index: int, title_anchor: Vector2, accent: Color) -> void:
	var title := tr(STATIONS[index][1])
	_label(title_anchor, title, accent)
	if index == _interactable_station:
		var font := ThemeDB.fallback_font
		var title_width := font.get_string_size(title, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
		var badge := title_anchor + Vector2(-title_width / 2 - 20, -4)
		var key := ControlHints.binding_label("interact", ControlHints.using_gamepad, true)
		var key_width := font.get_string_size(key, HORIZONTAL_ALIGNMENT_LEFT, -1, 13).x
		var badge_width := maxf(20.0, key_width + 10.0)
		badge.x -= (badge_width - 20.0) * 0.5
		var outline := Rect2(badge - Vector2(badge_width * 0.5, 10), Vector2(badge_width, 20))
		draw_rect(outline, INK)
		draw_rect(outline.grow(-2), Color("#a2d68f") if ControlHints.using_gamepad else Color("#f4e0be"))
		draw_string(font, badge + Vector2(-key_width / 2, 4), key, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, INK)


func _draw_dummy_impact(dummy: Sprite2D, index: int) -> void:
	var impact := impacts[index] / IMPACT_DURATION
	if impact <= 0:
		return
	var center := Vector2(0, -TARGET.get_height() * 0.56)
	for angle in range(0, 360, 90):
		var ray := Vector2.from_angle(deg_to_rad(angle + 30))
		dummy.draw_line(center + ray * 48, center + ray * (68 + 36 * (1 - impact)), Color("#ffdf86", impact), 5, true)


func _label(point: Vector2, label: String, color: Color, size := 13) -> void:
	var font := ThemeDB.fallback_font
	var width := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x
	draw_rect(Rect2(point - Vector2(width / 2 + 6, size), Vector2(width + 12, size + 6)), INK)
	draw_string(font, point - Vector2(width / 2, 1), label, HORIZONTAL_ALIGNMENT_LEFT, -1, size, color)
