class_name NetworkEnemyMotion
extends Node

var point := Vector2.ZERO
var epoch := -1
var render_position := Vector2.ZERO

var _synchronizer := StateSynchronizer.new()
var _interpolation := TickInterpolator.new()
var _bound_epoch := -1
var _active := false


func _ready() -> void:
	_synchronizer.name = "State"
	_synchronizer.root = self
	_synchronizer.properties = [":point", ":epoch"]
	_synchronizer.visibility_filter.add_visibility_filter(func(_peer: int) -> bool: return _active)
	add_child(_synchronizer)
	_interpolation.root = self
	_interpolation.properties = [":render_position"]
	_interpolation.process_priority = -1
	add_child(_interpolation)


func prepare(enemy: Node2D, room_epoch: int) -> void:
	var active := enemy != null
	if _active != active or _bound_epoch != room_epoch:
		_active = active
		_bound_epoch = room_epoch
		_synchronizer.visibility_filter.update_visibility()
		if active:
			render_position = enemy.position
			_interpolation.teleport()
	if active and multiplayer.is_server():
		point = enemy.position
		epoch = room_epoch


func has_pose(room_epoch: int) -> bool:
	return _active and epoch == room_epoch


func record_pose() -> void:
	if has_pose(_bound_epoch):
		render_position = point
