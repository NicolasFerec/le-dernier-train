extends Node

signal changed

const STICK_DEADZONE := 0.25

var using_gamepad := false
var _last_gamepad := -1


func _ready() -> void:
	# Preserve Godot's keyboard mappings and add the Xbox confirm/back buttons.
	var confirm := InputEventJoypadButton.new()
	confirm.button_index = JOY_BUTTON_A
	InputMap.action_add_event("ui_accept", confirm)
	var back := InputEventJoypadButton.new()
	back.button_index = JOY_BUTTON_B
	InputMap.action_add_event("ui_cancel", back)
	Input.joy_connection_changed.connect(_on_joy_connection_changed)


func _input(event: InputEvent) -> void:
	if event is InputEventJoypadButton and event.pressed:
		_use_gamepad(event.device)
	elif event is InputEventJoypadMotion and absf(event.axis_value) >= STICK_DEADZONE:
		_use_gamepad(event.device)
	elif (event is InputEventKey and event.pressed and not event.echo) \
			or (event is InputEventMouseButton and event.pressed) \
			or (event is InputEventMouseMotion and not event.screen_relative.is_zero_approx()):
		_set_gamepad(false)


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
