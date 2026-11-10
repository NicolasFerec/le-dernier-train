extends VBoxContainer

@onready var grid: GridContainer = $Bindings/Grid
@onready var status: Label = $Status

var _buttons := {}
var _action := ""
var _gamepad := false


func _ready() -> void:
	for action in ControlHints.ACTION_LABELS:
		_add_label(ControlHints.ACTION_LABELS[action])
		var pair: Array[Button] = []
		for gamepad in [false, true]:
			var button := Button.new()
			button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
			button.pressed.connect(_begin_capture.bind(action, gamepad))
			grid.add_child(button)
			pair.append(button)
		_buttons[action] = pair
	_add_label("CONTROLS_PAUSE")
	_add_label("CONTROLS_ESCAPE")
	_add_label("Menu")
	$Options/Vibration.set_pressed_no_signal(ControlHints.vibration_enabled())
	$Options/Vibration.toggled.connect(func(active: bool):
		ControlHints.set_vibration(active)
		_save()
	)
	$Options/Reset.pressed.connect(func():
		_cancel_capture()
		ControlHints.reset_bindings()
		refresh_bindings()
		_save()
	)
	visibility_changed.connect(func():
		if not is_visible_in_tree():
			_cancel_capture()
	)
	refresh_bindings()


func _add_label(key: String) -> void:
	var label := Label.new()
	label.text = key
	grid.add_child(label)


func refresh_bindings() -> void:
	for action in _buttons:
		for index in 2:
			var button: Button = _buttons[action][index]
			button.text = ControlHints.binding_label(action, index == 1)
			button.tooltip_text = button.text


func _begin_capture(action: String, gamepad: bool) -> void:
	if _action == action and _gamepad == gamepad:
		_cancel_capture()
		return
	_action = action
	_gamepad = gamepad
	status.text = "CONTROLS_CAPTURE_PAD" if gamepad else "CONTROLS_CAPTURE_KEY"


func _cancel_capture() -> void:
	_action = ""
	status.text = "CONTROLS_HINT"


func _input(event: InputEvent) -> void:
	if _action.is_empty() or not is_visible_in_tree():
		return
	if not (event is InputEventKey or event is InputEventJoypadButton):
		return
	get_viewport().set_input_as_handled()
	if not event.is_pressed() or event.is_echo():
		return
	if (event is InputEventKey and event.keycode == KEY_ESCAPE) or (event is InputEventJoypadButton and event.button_index == JOY_BUTTON_BACK):
		_cancel_capture()
		return
	if _gamepad != (event is InputEventJoypadButton):
		return
	if event is InputEventKey and event.physical_keycode == 0:
		return
	var conflict := ControlHints.rebind(_action, _gamepad, event)
	if not conflict.is_empty():
		var label: String = ControlHints.ACTION_LABELS.get(conflict, "CONTROLS_PAUSE")
		status.text = tr("CONTROLS_CONFLICT") % tr(label)
		return
	_cancel_capture()
	refresh_bindings()
	_save()


func _save() -> void:
	status.text = "CONTROLS_HINT" if GameSettings.save() == OK else "SETTINGS_SAVE_FAILED"
