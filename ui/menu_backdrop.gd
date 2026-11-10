extends Node2D

const DEPOT := preload("res://assets/menu/depot.png")
# Coordinates in the painted depot, so the effects stay attached when it is cropped.
const LANTERNS := [Vector2(0.102, 0.543), Vector2(0.95, 0.527)]
const REFLECTIONS := [Vector2(0.108, 0.738), Vector2(0.915, 0.84)]

var time := 0.0
var haze: GradientTexture2D


func _ready() -> void:
	var fade := Gradient.new()
	fade.offsets = PackedFloat32Array([0.0, 0.4, 1.0])
	fade.colors = PackedColorArray([Color.WHITE, Color(1, 1, 1, 0.53), Color(1, 1, 1, 0)])
	haze = GradientTexture2D.new()
	haze.gradient = fade
	haze.fill = GradientTexture2D.FILL_RADIAL
	haze.fill_from = Vector2(0.5, 0.5)
	haze.fill_to = Vector2(1.0, 0.5)
	haze.width = 128
	haze.height = 128


func _process(delta: float) -> void:
	time += delta
	queue_redraw()


func _draw() -> void:
	var view := get_viewport_rect().size
	if view.x <= 0.0 or view.y <= 0.0:
		return
	var image_size := DEPOT.get_size()
	var cover := maxf(view.x / image_size.x, view.y / image_size.y)
	var backdrop_size := image_size * cover
	var excess := backdrop_size - view
	var depot := Rect2(-excess * Vector2(0.5, 0.8), backdrop_size)
	draw_texture_rect(DEPOT, depot, false)

	for index in LANTERNS.size():
		var pulse := time + index * 2.3
		var brightness := 0.18 + sin(pulse * 1.7) * 0.045 + sin(pulse * 5.8) * 0.022
		var lamp: Vector2 = depot.position + depot.size * LANTERNS[index]
		var reflection: Vector2 = depot.position + depot.size * REFLECTIONS[index]
		_draw_haze(lamp, depot.size * Vector2(0.14, 0.26), Color(1.0, 0.57, 0.22, brightness))
		_draw_haze(lamp, depot.size * Vector2(0.035, 0.062), Color(1.0, 0.80, 0.42, brightness * 0.9))
		_draw_haze(reflection, depot.size * Vector2(0.18, 0.07), Color(1.0, 0.55, 0.24, brightness * 0.55))

	# Darken the area behind the central menu without dimming the edges.
	var design_scale := minf(view.x / GameSettings.DESIGN_SIZE.x, view.y / GameSettings.DESIGN_SIZE.y)
	_draw_haze(view * 0.5, Vector2(770, 640) * design_scale, Color(0.025, 0.039, 0.065, 0.9))


func _draw_haze(center: Vector2, size: Vector2, tint: Color) -> void:
	draw_texture_rect(haze, Rect2(center - size * 0.5, size), false, tint)
