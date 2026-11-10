extends Node

signal changed

const STICK_DEADZONE := 0.25
const ACTION_LABELS := {
	"move_up": "CONTROLS_UP", "move_down": "CONTROLS_DOWN",
	"move_left": "CONTROLS_LEFT", "move_right": "CONTROLS_RIGHT",
	"attack": "CONTROLS_LIGHT", "heavy_attack": "CONTROLS_HEAVY",
	"dodge": "DODGE", "interact": "CONTROLS_INTERACT",
}
const BUTTON_LABELS := {
	JOY_BUTTON_A: "A", JOY_BUTTON_B: "B", JOY_BUTTON_X: "X", JOY_BUTTON_Y: "Y",
	JOY_BUTTON_LEFT_SHOULDER: "LB", JOY_BUTTON_RIGHT_SHOULDER: "RB",
	JOY_BUTTON_LEFT_STICK: "LS", JOY_BUTTON_RIGHT_STICK: "RS",
	JOY_BUTTON_DPAD_UP: "↑", JOY_BUTTON_DPAD_DOWN: "↓",
	JOY_BUTTON_DPAD_LEFT: "←", JOY_BUTTON_DPAD_RIGHT: "→",
	JOY_BUTTON_START: "Menu", JOY_BUTTON_BACK: "View",
}
const KEY_LABELS := {
	KEY_UP: "↑", KEY_DOWN: "↓", KEY_LEFT: "←", KEY_RIGHT: "→",
}

var using_gamepad := false
var _last_gamepad := -1
var _default_bindings := {}


func _ready() -> void:
	# Preserve Godot's keyboard mappings and add the Xbox confirm/back buttons.
	var confirm := InputEventJoypadButton.new()
	confirm.button_index = JOY_BUTTON_A
	InputMap.action_add_event("ui_accept", confirm)
	var back := InputEventJoypadButton.new()
	back.button_index = JOY_BUTTON_B
	InputMap.action_add_event("ui_cancel", back)
	Input.joy_connection_changed.connect(_on_joy_connection_changed)
	for action in ACTION_LABELS:
		_default_bindings[action] = InputMap.action_get_events(action)
		for gamepad in [false, true]:
			var saved = GameSettings.preferences.get_value(_binding_section(gamepad), action, -1)
			if saved is int and saved >= 0:
				_apply_binding(action, gamepad, saved)


func _input(event: InputEvent) -> void:
	if event is InputEventJoypadButton and event.pressed:
		_use_gamepad(event.device)
	elif event is InputEventJoypadMotion and absf(event.axis_value) >= STICK_DEADZONE:
		_use_gamepad(event.device)
	elif (event is InputEventKey and event.pressed and not event.echo) \
			or (event is InputEventMouseButton and event.pressed) \
			or (event is InputEventMouseMotion and not event.screen_relative.is_zero_approx()):
		_set_gamepad(false)


func vibrate_on_hit() -> void:
	if vibration_enabled() and using_gamepad and _last_gamepad in Input.get_connected_joypads():
		Input.start_joy_vibration(_last_gamepad, 0.30, 0.65, 0.16)


func _use_gamepad(device: int) -> void:
	_last_gamepad = device
	_set_gamepad(true)


func _on_joy_connection_changed(device: int, connected: bool) -> void:
	if not connected and device == _last_gamepad:
		_last_gamepad = -1
		_set_gamepad(false)


func _set_gamepad(active: bool) -> void:
	if using_gamepad != active:
		using_gamepad = active
		changed.emit()


func vibration_enabled() -> bool:
	return GameSettings.preferences.get_value("controls", "vibration", true) == true


func set_vibration(active: bool) -> void:
	GameSettings.preferences.set_value("controls", "vibration", active)
	if not active and _last_gamepad in Input.get_connected_joypads():
		Input.stop_joy_vibration(_last_gamepad)


func binding_label(action: String, gamepad: bool, primary_only := false) -> String:
	var labels := PackedStringArray()
	for event in InputMap.action_get_events(action):
		if gamepad and event is InputEventJoypadButton:
			labels.append(BUTTON_LABELS.get(event.button_index, tr("CONTROLS_BUTTON_NUMBER") % (event.button_index + 1)))
		elif not gamepad and event is InputEventKey:
			var code := _keycode(event)
			if code == KEY_ESCAPE:
				labels.append(tr("CONTROLS_ESCAPE"))
			elif code == KEY_SPACE:
				labels.append(tr("CONTROLS_SPACE"))
			else:
				labels.append(KEY_LABELS.get(code, OS.get_keycode_string(code)))
		if primary_only and not labels.is_empty():
			break
	return " / ".join(labels)


func rebind(action: String, gamepad: bool, event: InputEvent) -> String:
	for other in ACTION_LABELS.keys() + ["session_menu"]:
		if other == action:
			continue
		for binding in InputMap.action_get_events(other):
			if gamepad and binding is InputEventJoypadButton and binding.button_index == event.button_index:
				return other
			if not gamepad and binding is InputEventKey and _keycode(binding) == _keycode(event):
				return other
	var code: int = event.button_index if gamepad else event.physical_keycode
	_apply_binding(action, gamepad, code)
	GameSettings.preferences.set_value(_binding_section(gamepad), action, code)
	changed.emit()
	return ""


func reset_bindings() -> void:
	for action in _default_bindings:
		InputMap.action_erase_events(action)
		for event in _default_bindings[action]:
			InputMap.action_add_event(action, event)
	for gamepad in [false, true]:
		var section := _binding_section(gamepad)
		if GameSettings.preferences.has_section(section):
			GameSettings.preferences.erase_section(section)
	changed.emit()


func _apply_binding(action: String, gamepad: bool, code: int) -> void:
	for binding in InputMap.action_get_events(action):
		if (gamepad and binding is InputEventJoypadButton) or (not gamepad and binding is InputEventKey):
			InputMap.action_erase_event(action, binding)
	if gamepad:
		var button := InputEventJoypadButton.new()
		button.button_index = code
		InputMap.action_add_event(action, button)
	else:
		var key := InputEventKey.new()
		key.physical_keycode = code
		InputMap.action_add_event(action, key)


func _binding_section(gamepad: bool) -> String:
	return "gamepad_bindings" if gamepad else "keyboard_bindings"


func _keycode(event: InputEventKey) -> int:
	if event.physical_keycode == 0:
		return event.keycode
	if DisplayServer.get_name() != "headless":
		var local := DisplayServer.keyboard_get_keycode_from_physical(event.physical_keycode)
		if local != 0:
			return local
	return event.physical_keycode
