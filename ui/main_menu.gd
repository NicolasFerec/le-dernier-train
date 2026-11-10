extends Control

const ARENA := "res://arena/arena.tscn"
const FPS_PRESETS: Array[int] = [30, 60, 75, 90, 120, 144, 165, 180, 240]

@onready var home: VBoxContainer = $Center/Home
@onready var settings: PanelContainer = $Center/Settings
@onready var status: Label = $Center/Settings/Rows/Status
@onready var tabs: TabContainer = $Center/Settings/Rows/Tabs
@onready var update_channel: OptionButton = $Center/Settings/Rows/Tabs/General/Content/UpdateRow/Channel
@onready var graphics: GridContainer = $Center/Settings/Rows/Tabs/Graphics/Content/Grid
@onready var screens: OptionButton = $Center/Settings/Rows/Tabs/Graphics/Content/Grid/Screen
@onready var resolutions: OptionButton = $Center/Settings/Rows/Tabs/Graphics/Content/Grid/Resolution
@onready var fps_value: Label = $Center/Settings/Rows/Tabs/Graphics/Content/Grid/FrameLimit/Value

var _resolution_options: Array[Vector2i] = []


func _ready() -> void:
	GameAudio.set_scene(GameAudio.Scene.DEPOT)
	GameSettings.set_menu(true)
	set_anchors_preset(Control.PRESET_TOP_LEFT)
	get_viewport().size_changed.connect(_layout)
	_layout()
	$Center/Home/Play.pressed.connect(_play)
	$Center/Home/Options.pressed.connect(_open_settings)
	$Center/Home/Quit.pressed.connect(GameAudio.quit_game)
	$Center/Settings/Rows/Back.pressed.connect(_close_settings)
	$Center/Home/Play.grab_focus()
	ControlHints.changed.connect(_refresh_tab_shortcuts)
	_refresh_tab_shortcuts()
	_translate_settings()
	var language: OptionButton = $Center/Settings/Rows/Tabs/General/Content/LanguageRow/Language
	language.add_item("Français")
	language.add_item("English")
	language.select(1 if TranslationServer.get_locale() == "en" else 0)
	language.item_selected.connect(func(index: int):
		GameSettings.set_language("en" if index == 1 else "fr")
		_translate_settings()
		_refresh_screens()
		_refresh_resolutions()
		_update_fps_label()
	)
	update_channel.item_selected.connect(func(index: int):
		GameSettings.preferences.set_value("updates", "channel", "dev" if index == 1 else "latest")
		status.text = "" if GameSettings.save() == OK else tr("SETTINGS_SAVE_FAILED")
		status.visible = not status.text.is_empty()
	)
	var audio := $Center/Settings/Rows/Tabs/Audio/Grid
	for bus in GameSettings.BUSES:
		var slider: HSlider = audio.get_node(bus)
		slider.set_value_no_signal(GameSettings.volume(bus) * 100)
		slider.value_changed.connect(func(value: float): GameSettings.set_volume(bus, value / 100.0))

	screens.item_selected.connect(func(index: int):
		GameSettings.preferred_screen = screens.get_item_id(index) - 1
		GameSettings.apply_display()
		_refresh_resolutions()
	)
	resolutions.item_selected.connect(func(index: int):
		GameSettings.fullscreen_resolution = _resolution_options[index]
		GameSettings.apply_render_resolution()
	)
	var fullscreen: CheckButton = graphics.get_node("Fullscreen")
	fullscreen.set_pressed_no_signal(GameSettings.fullscreen)
	fullscreen.toggled.connect(func(active: bool):
		GameSettings.fullscreen = active
		GameSettings.apply_display()
		_refresh_resolutions()
	)
	var vsync: CheckButton = graphics.get_node("VSync")
	vsync.set_pressed_no_signal(GameSettings.vsync)
	vsync.toggled.connect(func(active: bool):
		GameSettings.vsync = active
		GameSettings.apply_vsync()
	)
	var counter: CheckButton = graphics.get_node("Counter")
	counter.set_pressed_no_signal(GameSettings.show_fps)
	counter.toggled.connect(func(active: bool): GameSettings.show_fps = active)
	var fps: HSlider = $Center/Settings/Rows/Tabs/Graphics/Content/Grid/FrameLimit/FPS
	var options := FPS_PRESETS.duplicate()
	var limit := GameSettings.fps_limit
	if limit > 0 and not options.has(limit):
		options.append(limit)
		options.sort()
	options.push_front(-1)
	options.append(0)
	fps.max_value = options.size() - 1
	fps.step = 1
	fps.tick_count = options.size()
	fps.ticks_on_borders = true
	fps.set_value_no_signal(options.find(limit))
	_update_fps_label()
	fps.value_changed.connect(func(value: float):
		GameSettings.fps_limit = options[int(value)]
		_update_fps_label()
	)
	Session.invitation_received.connect(func(_code: String): _play())
	if not Session.pending_invitation.is_empty():
		_play.call_deferred()


func _refresh_tab_shortcuts() -> void:
	$Center/Settings/Rows/TabShortcuts.text = "SETTINGS_TAB_GAMEPAD" if ControlHints.using_gamepad else "SETTINGS_TAB_KEYS"


func _translate_settings() -> void:
	tabs.set_tab_title(0, tr("SETTINGS_AUDIO"))
	tabs.set_tab_title(1, tr("SETTINGS_GRAPHICS"))
	tabs.set_tab_title(2, tr("SETTINGS_GENERAL"))
	tabs.set_tab_title(3, tr("CONTROLS_TAB"))
	$Center/Settings/Rows/Tabs/Controls/Content.refresh_bindings()
	update_channel.clear()
	update_channel.add_item(tr("SETTINGS_UPDATE_LATEST"))
	update_channel.add_item(tr("SETTINGS_UPDATE_DEV"))
	update_channel.select(1 if GameSettings.preferences.get_value("updates", "channel", "latest") == "dev" else 0)


func _layout() -> void:
	var layout := GameSettings.design_transform()
	position = layout.origin
	scale = layout.get_scale()
	size = GameSettings.DESIGN_SIZE


func _refresh_screens() -> void:
	var count := DisplayServer.get_screen_count()
	var selectable := GameSettings.can_choose_screen()
	screens.clear()
	if selectable:
		screens.add_item(tr("DISPLAY_SCREEN_AUTO"), 0)
		for index in count:
			var dimensions := DisplayServer.screen_get_size(index)
			screens.add_item(tr("DISPLAY_SCREEN_ITEM") % [index + 1, dimensions.x, dimensions.y], index + 1)
		screens.select(maxi(0, screens.get_item_index(GameSettings.preferred_screen + 1)))
	else:
		var current := get_tree().root.current_screen
		var dimensions := GameSettings.screen_size()
		screens.add_item(tr("DISPLAY_SCREEN_ITEM") % [current + 1, dimensions.x, dimensions.y])
	screens.disabled = not selectable or count < 2
	screens.tooltip_text = tr("DISPLAY_SCREEN_WAYLAND") if DisplayServer.get_name().to_lower() == "wayland" else ""


func _refresh_resolutions() -> void:
	_resolution_options = GameSettings.fullscreen_resolutions()
	var native := GameSettings.screen_size()
	resolutions.clear()
	resolutions.add_item(tr("DISPLAY_NATIVE") % [native.x, native.y])
	for size_option in _resolution_options.slice(1):
		resolutions.add_item("%d × %d" % [size_option.x, size_option.y])
	resolutions.select(maxi(0, _resolution_options.find(GameSettings.fullscreen_resolution)))
	resolutions.disabled = not GameSettings.fullscreen


func _play() -> void:
	get_tree().change_scene_to_file(ARENA)


func _open_settings() -> void:
	_refresh_screens()
	_refresh_resolutions()
	status.hide()
	home.hide()
	settings.show()
	tabs.get_tab_bar().grab_focus()


func _close_settings() -> void:
	if GameSettings.save() != OK:
		status.text = tr("SETTINGS_SAVE_FAILED")
		status.show()
		return
	settings.hide()
	home.show()
	$Center/Home/Options.grab_focus()


func _update_fps_label() -> void:
	match GameSettings.fps_limit:
		-1: fps_value.text = tr("FPS_AUTO")
		0: fps_value.text = tr("FPS_UNLIMITED")
		_: fps_value.text = tr("FPS_COUNTER") % GameSettings.fps_limit


func _unhandled_input(event: InputEvent) -> void:
	if not settings.visible:
		return
	if event.is_action_pressed("ui_cancel"):
		_close_settings()
	else:
		var direction := int(event.is_action_pressed("next_menu_tab")) - int(event.is_action_pressed("previous_menu_tab"))
		if direction == 0:
			return
		tabs.current_tab = posmod(tabs.current_tab + direction, tabs.get_tab_count())
		tabs.get_tab_bar().grab_focus()
	get_viewport().set_input_as_handled()


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST and is_node_ready() and settings.visible:
		GameSettings.save()
