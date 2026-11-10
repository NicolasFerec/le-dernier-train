class_name NetworkPlayerMotion
extends Node

class Controls extends Node:
	var stamp := Vector2i.ZERO
	var direction := Vector2.ZERO
	var dodge_request := 0

var body := PlayerLocomotion.new()
var controls := Controls.new()
var stamp := Vector2i.ZERO
var render_position := Vector2.ZERO
var paused := false
var dodge_consumed := 0
var dodge_started := 0
var played_dodge := 0

var _owner := Vector2i.ZERO
var _bounds := Rect2()
var _first_tick := 0
var _next_dodge := 0
var _rollback := RollbackSynchronizer.new()
var _interpolation := TickInterpolator.new()


func _ready() -> void:
	body.name = "Body"
	controls.name = "Controls"
	add_child(body)
	add_child(controls)
	_rollback.name = "Rollback"
	_rollback.root = self
	_rollback.enable_prediction = true
	_rollback.enable_input_broadcast = false
	_rollback.state_properties = [
		":stamp", ":dodge_consumed", ":dodge_started", "Body:position", "Body:facing",
		"Body:movement", "Body:dash_time", "Body:dash_cooldown", "Body:dash_direction"
	]
	_rollback.input_properties = ["Controls:stamp", "Controls:direction", "Controls:dodge_request"]
	_rollback.visibility_filter.add_visibility_filter(func(_peer: int) -> bool: return _owner.x != 0)
	add_child(_rollback)
	_interpolation.root = self
	_interpolation.properties = [":render_position"]
	_interpolation.process_priority = -1
	add_child(_interpolation)


func bind(peer: int, epoch: int, player: PlayerLocomotion, bounds: Rect2) -> void:
	var owner := Vector2i(peer, epoch) if player != null else Vector2i.ZERO
	if owner == _owner:
		if player == null or not multiplayer.is_server() or body.position == player.position:
			return
	_owner = owner
	_bounds = bounds
	_first_tick = NetworkTime.tick
	_next_dodge = 0
	dodge_consumed = 0
	dodge_started = 0
	played_dodge = 0
	controls.set_multiplayer_authority(peer if peer != 0 else 1)
	controls.stamp = owner
	controls.direction = Vector2.ZERO
	controls.dodge_request = 0
	stamp = owner if multiplayer.is_server() or peer == multiplayer.get_unique_id() else Vector2i.ZERO
	if player != null:
		body.position = player.position
		body.facing = player.facing
		body.movement = Vector2.ZERO
		body.dash_time = player.dash_time
		body.dash_cooldown = player.dash_cooldown
		body.dash_direction = player.dash_direction
		render_position = body.position
		_interpolation.teleport()
	_rollback.process_settings()
	_rollback.visibility_filter.update_visibility()


func gather(direction: Vector2) -> void:
	controls.stamp = _owner
	controls.direction = direction
	controls.dodge_request = _next_dodge


func request_dodge() -> void:
	_next_dodge += 1


func has_pose() -> bool:
	return _owner.x != 0 and stamp == _owner


func record_pose() -> void:
	if has_pose():
		render_position = body.position


func _rollback_tick(delta: float, tick: int, _is_fresh: bool) -> void:
	if not has_pose() or tick < _first_tick or paused:
		return
	var valid_input := controls.stamp == _owner and controls.direction.is_finite()
	body.movement = controls.direction.limit_length() if valid_input else Vector2.ZERO
	if valid_input and controls.dodge_request > dodge_consumed:
		dodge_consumed = controls.dodge_request
		if body.dodge():
			dodge_started = dodge_consumed
	body.step_motion(delta, _bounds)
