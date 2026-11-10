extends Node

const MENU_FPS := 30
const DESIGN_SIZE := Vector2i(800, 520)
const HUD_MAX_ASPECT := 21.0 / 9.0
const COMMON_RESOLUTIONS: Array[Vector2i] = [
	Vector2i(640, 480), Vector2i(800, 600), Vector2i(960, 540), Vector2i(1024, 768),
	Vector2i(1152, 720), Vector2i(1280, 720), Vector2i(1280, 800), Vector2i(1280, 960),
	Vector2i(1280, 1024), Vector2i(1366, 768), Vector2i(1440, 900), Vector2i(1600, 900),
	Vector2i(1600, 1000), Vector2i(1600, 1200), Vector2i(1680, 1050), Vector2i(1920, 1080),
	Vector2i(1920, 1200), Vector2i(1920, 1440), Vector2i(2560, 720), Vector2i(2560, 1080),
	Vector2i(2560, 1440),
	Vector2i(2560, 1600), Vector2i(3440, 1440), Vector2i(3840, 1080), Vector2i(3840, 1600),
	Vector2i(3840, 2160),
	Vector2i(3840, 2400), Vector2i(5120, 1440), Vector2i(5120, 2160), Vector2i(7680, 2160),
]
const MIN_GAME_FPS := 30
const MAX_GAME_FPS := 240
const BUSES := ["Master", "Music", "SFX"]
const PATH := "user://preferences.cfg"

var preferences := ConfigFile.new()
var fullscreen := false
var preferred_screen := -1
var fullscreen_resolution := Vector2i.ZERO
var vsync := true
var show_fps := false
var fps_limit := -1
var in_menu := true
var _refresh_timer := 0.0
var _counter: Label


func _ready() -> void:
	preferences.load(PATH)
	var language: String = preferences.get_value("interface", "language", "fr")
	TranslationServer.set_locale("en" if language == "en" else "fr")
	fullscreen = preferences.get_value("video", "fullscreen", false) == true
	var saved_screen = preferences.get_value("video", "screen", -1)
	if saved_screen is int:
		preferred_screen = saved_screen
	var saved_resolution = preferences.get_value("video", "fullscreen_resolution", Vector2i.ZERO)
	if saved_resolution is Vector2i:
		fullscreen_resolution = saved_resolution
	vsync = preferences.get_value("video", "vsync", true) == true
	show_fps = preferences.get_value("video", "show_fps", false) == true
	var saved_limit = preferences.get_value("video", "fps_limit", -1)
	if saved_limit is int and (saved_limit in [-1, 0] or (saved_limit >= MIN_GAME_FPS and saved_limit <= MAX_GAME_FPS)):
		fps_limit = saved_limit
	for bus in BUSES:
		_apply_volume(bus, volume(bus))
	_create_counter()
	apply_display()
	set_menu(true)


func volume(bus: String) -> float:
	return clampf(float(preferences.get_value("audio", bus, 1.0)), 0.0, 1.0)


func set_volume(bus: String, value: float) -> void:
	preferences.set_value("audio", bus, value)
	_apply_volume(bus, value)


func _apply_volume(bus: String, value: float) -> void:
	var index := AudioServer.get_bus_index(bus)
	AudioServer.set_bus_mute(index, is_zero_approx(value))
	AudioServer.set_bus_volume_linear(index, maxf(value, 0.0001))


func set_language(language: String) -> void:
	TranslationServer.set_locale(language)
	preferences.set_value("interface", "language", language)
	preferences.save(PATH)


func save() -> Error:
	preferences.set_value("video", "fullscreen", fullscreen)
	preferences.set_value("video", "screen", preferred_screen)
	preferences.set_value("video", "fullscreen_resolution", fullscreen_resolution)
	preferences.set_value("video", "vsync", vsync)
	preferences.set_value("video", "show_fps", show_fps)
	preferences.set_value("video", "fps_limit", fps_limit)
	return preferences.save(PATH)


func apply_display() -> void:
	if DisplayServer.get_name() == "headless":
		return
	var window := get_tree().root
	if can_choose_screen() and preferred_screen >= 0 and preferred_screen < DisplayServer.get_screen_count():
		window.current_screen = preferred_screen
	window.mode = Window.MODE_FULLSCREEN if fullscreen else Window.MODE_WINDOWED
	apply_render_resolution()
	apply_vsync()


func can_choose_screen() -> bool:
	return DisplayServer.get_name().to_lower() in ["windows", "x11", "macos"]


func screen_size() -> Vector2i:
	var size := DisplayServer.screen_get_size(get_tree().root.current_screen)
	return size if size.x > 0 and size.y > 0 else DESIGN_SIZE


func fullscreen_resolutions() -> Array[Vector2i]:
	var native := screen_size()
	var aspect := float(native.x) / native.y
	var options: Array[Vector2i] = [Vector2i.ZERO]
	for size in COMMON_RESOLUTIONS:
		if size != native and size.x <= native.x and size.y <= native.y:
			if absf(float(size.x) / size.y / aspect - 1.0) <= 0.02:
				options.append(size)
	return options


func apply_render_resolution() -> void:
	if DisplayServer.get_name() == "headless":
		return
	var window := get_tree().root
	if fullscreen:
		var target := fullscreen_resolution
		if target == Vector2i.ZERO or not fullscreen_resolutions().has(target):
			target = screen_size()
		window.content_scale_size = target
		window.content_scale_mode = Window.CONTENT_SCALE_MODE_VIEWPORT
	else:
		window.content_scale_mode = Window.CONTENT_SCALE_MODE_CANVAS_ITEMS
		window.content_scale_size = DESIGN_SIZE


# Keep the prototype's authored coordinates while changing the actual render target.
func design_transform() -> Transform2D:
	var viewport_size := get_tree().root.get_visible_rect().size
	var fit := minf(viewport_size.x / DESIGN_SIZE.x, viewport_size.y / DESIGN_SIZE.y)
	var origin := (viewport_size - Vector2(DESIGN_SIZE) * fit) * 0.5
	return Transform2D(0.0, Vector2(fit, fit), 0.0, origin)


func hud_safe_rect() -> Rect2:
	var visible := get_tree().root.get_visible_rect()
	var width := minf(visible.size.x, visible.size.y * HUD_MAX_ASPECT)
	var inset := (visible.size.x - width) * 0.5
	return Rect2(visible.position + Vector2(inset, 0), Vector2(width, visible.size.y))


func apply_vsync() -> void:
	if DisplayServer.get_name() != "headless":
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if vsync else DisplayServer.VSYNC_DISABLED)


func set_menu(active: bool) -> void:
	in_menu = active
	Engine.max_fps = MENU_FPS if in_menu else _game_limit()


func refresh_rate() -> int:
	return maxi(0, roundi(DisplayServer.screen_get_refresh_rate(get_tree().root.current_screen)))


func _game_limit() -> int:
	if fps_limit >= 0:
		return fps_limit
	var hz := refresh_rate()
	return hz if hz > 0 else 60


func _create_counter() -> void:
	var overlay := CanvasLayer.new()
	overlay.layer = 10
	add_child(overlay)
	_counter = Label.new()
	_counter.mouse_filter = Control.MOUSE_FILTER_IGNORE
	overlay.add_child(_counter)
	get_tree().root.size_changed.connect(_layout_counter)
	_layout_counter()
	_counter.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_counter.add_theme_color_override("font_outline_color", Color.BLACK)
	_counter.add_theme_constant_override("outline_size", 5)
	_counter.visible = show_fps


func _layout_counter() -> void:
	var fit := design_transform().get_scale()
	_counter.scale = fit
	_counter.size.x = 86
	_counter.position = Vector2(hud_safe_rect().end.x - 110 * fit.x, 66 * fit.y)


func _process(delta: float) -> void:
	_counter.visible = show_fps
	if show_fps:
		_counter.text = tr("FPS_COUNTER") % Engine.get_frames_per_second()
	if not in_menu and fps_limit == -1:
		_refresh_timer += delta
		if _refresh_timer >= 1.0:
			_refresh_timer = 0.0
			Engine.max_fps = _game_limit()
