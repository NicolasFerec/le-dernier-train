extends Sprite2D

const MODEL_PATH := "res://assets/models/conductor/conductor.glb"
const WEAPON_MODELS := [
	preload("res://assets/models/conductor/sabre.glb"),
	preload("res://assets/models/conductor/hammer.glb"),
]
# The imported models point along +X. Grip offsets are in model space.
const WEAPON_GRIPS := [Vector3(-0.37, 0.0, 0.0), Vector3(-0.30, -0.03, 0.0)]
const WEAPON_SCALES := [0.96, 0.86]
const VIEW_SIZE := 256
const BODY_HEIGHT := 1.8
const CAMERA_SIZE := 3.05
const CAMERA_PITCH := PI / 4.0

var _viewport: SubViewport
var _turn: Node3D
var _skeleton: Skeleton3D
var _mount: Node3D
var _weapons: Array[Node3D] = []
var _bones := {}
var _cycle := 0.0
var _run_weight := 0.0


func setup() -> bool:
	if not ResourceLoader.exists(MODEL_PATH, "PackedScene"):
		push_warning("The experimental conductor model is unavailable.")
		return false
	var model: Node3D = load(MODEL_PATH).instantiate()
	_skeleton = _find_skeleton(model)
	if _skeleton == null:
		model.free()
		push_warning("The experimental conductor model has no skeleton.")
		return false
	var hand := _bone("righthand")
	if hand == -1:
		model.free()
		push_warning("The experimental conductor model has no right hand bone.")
		return false

	_viewport = SubViewport.new()
	_viewport.size = Vector2i(VIEW_SIZE, VIEW_SIZE)
	_viewport.own_world_3d = true
	_viewport.transparent_bg = true
	_viewport.msaa_3d = Viewport.MSAA_2X
	_viewport.render_target_update_mode = SubViewport.UPDATE_WHEN_VISIBLE
	add_child(_viewport)
	_turn = Node3D.new()
	_viewport.add_child(_turn)
	_turn.add_child(model)

	var bounds := _mesh_bounds(model)
	var fit := BODY_HEIGHT / maxf(bounds.size.y, 0.01)
	model.scale *= fit
	model.position -= Vector3(bounds.get_center().x, bounds.position.y, bounds.get_center().z) * fit
	var skeleton_basis := (_turn.global_basis.inverse() * _skeleton.global_basis).orthonormalized()
	var stride_axis := skeleton_basis.inverse() * Vector3.RIGHT
	for side in ["left", "right"]:
		for limb in ["upleg", "leg", "foot", "arm", "forearm"]:
			var index := _bone(side + limb)
			if index != -1:
				_bones[side + limb] = {
					"index": index,
					"rest": _skeleton.get_bone_pose_rotation(index),
					"axis": _skeleton.get_bone_global_rest(index).basis.inverse() * stride_axis,
				}

	var attachment := BoneAttachment3D.new()
	attachment.bone_idx = hand
	_skeleton.add_child(attachment)
	_mount = Node3D.new()
	attachment.add_child(_mount)
	var hand_rest := _skeleton.get_bone_global_rest(hand)
	var carry := Basis(Vector3.UP, PI / 2.0 + 0.18) * Basis(Vector3.BACK, -0.25)
	_mount.basis = hand_rest.basis.orthonormalized().inverse() * skeleton_basis.inverse() * carry
	_mount.scale = Vector3.ONE / _skeleton.global_basis.get_scale()
	var middle := _bone("righthandmiddle1")
	if middle != -1:
		var palm := hand_rest.affine_inverse() * _skeleton.get_bone_global_rest(middle).origin
		_mount.position = palm * 0.65
	for index in WEAPON_MODELS.size():
		var weapon := WEAPON_MODELS[index].instantiate() as Node3D
		var weapon_scale: float = WEAPON_SCALES[index]
		weapon.scale = Vector3.ONE * weapon_scale
		weapon.position = -WEAPON_GRIPS[index] * weapon_scale
		_mount.add_child(weapon)
		_weapons.append(weapon)

	var world := WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_color = Color("#d7e1fb")
	world.environment.ambient_light_energy = 0.85
	_viewport.add_child(world)
	_add_light(Vector3(-48, -24, 0), Color("#ffddba"), 1.15)
	_add_light(Vector3(-30, 145, 0), Color("#b8d2ff"), 0.55)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = CAMERA_SIZE
	_viewport.add_child(camera)
	var aim := Vector3(0, 0.85, 0)
	camera.position = aim + Vector3(0, sin(CAMERA_PITCH), cos(CAMERA_PITCH)) * 5.0
	camera.look_at(aim)
	camera.current = true
	texture = _viewport.get_texture()
	scale = Vector2.ONE * 0.57
	offset.y = -0.85 * cos(CAMERA_PITCH) * VIEW_SIZE / CAMERA_SIZE
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	return true


func update(actor: Node2D, delta: float) -> void:
	for index in _weapons.size():
		_weapons[index].visible = actor.weapon == index
	if actor.paused:
		return
	var projected := Isometric.GROUND * Vector2(actor.render_facing)
	_turn.rotation.y = atan2(projected.x, projected.y / sin(CAMERA_PITCH))
	var moving: bool = actor.render_movement.length_squared() > 0.01
	_run_weight = move_toward(_run_weight, 1.0 if moving else 0.0, delta * 14.0)
	_cycle = fposmod(_cycle + delta * TAU * 2.55, TAU)
	var stride := sin(_cycle) * _run_weight
	_turn.position.y = absf(cos(_cycle)) * 0.045 * _run_weight
	_bend("leftupleg", stride * 0.63)
	_bend("rightupleg", -stride * 0.63)
	_bend("leftleg", maxf(0.0, -stride) * 0.85)
	_bend("rightleg", maxf(0.0, stride) * 0.85)
	_bend("leftfoot", -stride * 0.24)
	_bend("rightfoot", stride * 0.24)
	_bend("leftarm", -stride * 0.34)
	_bend("rightarm", stride * 0.18)
	_bend("leftforearm", -0.28 * _run_weight)
	_bend("rightforearm", -0.34 * _run_weight)


func _bend(key: String, angle: float) -> void:
	if _bones.has(key):
		var bone: Dictionary = _bones[key]
		_skeleton.set_bone_pose_rotation(bone.index, bone.rest * Quaternion(Vector3(bone.axis).normalized(), angle))


func _bone(suffix: String) -> int:
	for index in _skeleton.get_bone_count():
		if _skeleton.get_bone_name(index).to_lower().replace("_", "").ends_with(suffix):
			return index
	return -1


func _find_skeleton(node: Node) -> Skeleton3D:
	if node is Skeleton3D:
		return node
	for child in node.get_children():
		var found := _find_skeleton(child)
		if found != null:
			return found
	return null


func _mesh_bounds(model: Node3D) -> AABB:
	var bounds := AABB()
	for node in model.find_children("*", "MeshInstance3D", true, false):
		var instance := node as MeshInstance3D
		var local_bounds := (model.global_transform.affine_inverse() * instance.global_transform) * instance.get_aabb()
		bounds = local_bounds if bounds.size == Vector3.ZERO else bounds.merge(local_bounds)
	return bounds


func _add_light(angles: Vector3, color: Color, energy: float) -> void:
	var light := DirectionalLight3D.new()
	light.rotation_degrees = angles
	light.light_color = color
	light.light_energy = energy
	_viewport.add_child(light)
