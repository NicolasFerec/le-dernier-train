extends Node

enum Scene { SILENT, DEPOT, WAGON }
enum Cue { BLADE, HAMMER, DAGGERS, DASH, DUMMY, ENEMY_HIT, ENEMY_STRIKE, ENEMY_FALL, STATION }

const WORLD_AUDIO := &"world_audio"
const TRACKS := {
	Scene.DEPOT: ["res://audio/ambience/depot_music.ogg", "res://audio/ambience/depot_air.ogg"],
	Scene.WAGON: ["res://audio/ambience/wagon_music.ogg", "res://audio/ambience/wagon_air.ogg"],
}
const EFFECTS := {
	Cue.BLADE: "res://audio/effects/blade_swing.wav",
	Cue.HAMMER: "res://audio/effects/heavy_swing.wav",
	Cue.DAGGERS: "res://audio/effects/dagger_swing.wav",
	Cue.DASH: "res://audio/effects/dash.wav",
	Cue.DUMMY: "res://audio/effects/dummy_hit.wav",
	Cue.ENEMY_HIT: "res://audio/effects/enemy_hit.wav",
	Cue.ENEMY_STRIKE: "res://audio/effects/enemy_strike.wav",
	Cue.ENEMY_FALL: "res://audio/effects/enemy_fall.wav",
	Cue.STATION: "res://audio/effects/station.wav",
}

var _scene := Scene.SILENT
var _layers := {}
var _effects := {}
var _fade: Tween
var _quitting := false


func _ready() -> void:
	get_tree().auto_accept_quit = false
	get_window().close_requested.connect(quit_game)
	for location in TRACKS:
		var layers: Array[AudioStreamPlayer] = []
		for index in 2:
			var layer := AudioStreamPlayer.new()
			layer.stream = load(TRACKS[location][index])
			layer.bus = &"Music" if index == 0 else &"SFX"
			layer.volume_linear = 0.0
			add_child(layer)
			layers.append(layer)
		_layers[location] = layers
	for cue in EFFECTS:
		_effects[cue] = load(EFFECTS[cue])


func set_scene(location: Scene) -> void:
	if _scene == location or _quitting:
		return
	_scene = location
	if _fade != null:
		_fade.kill()
	_fade = create_tween().set_parallel()
	for key in _layers:
		var active: bool = key == location
		for layer: AudioStreamPlayer in _layers[key]:
			if active and not layer.playing:
				layer.play()
			if layer.playing:
				_fade.tween_property(layer, "volume_linear", 1.0 if active else 0.0, 1.1)
	_fade.finished.connect(_stop_faded_layers)


func _stop_faded_layers() -> void:
	for location in _layers:
		if location != _scene:
			for layer: AudioStreamPlayer in _layers[location]:
				layer.stop()


func play_at(parent: Node2D, cue: int, point: Vector2) -> void:
	if _quitting or not _effects.has(cue):
		return
	var sound := AudioStreamPlayer2D.new()
	sound.stream = _effects[cue]
	sound.bus = &"SFX"
	sound.volume_db = -4.0
	sound.position = point
	sound.max_distance = 640.0 * parent.global_transform.x.length()
	sound.pitch_scale = randf_range(0.93, 1.07)
	parent.add_child(sound)
	sound.add_to_group(WORLD_AUDIO)
	sound.finished.connect(sound.queue_free)
	sound.play()



func quit_game() -> void:
	if _quitting:
		return
	_quitting = true
	if _fade != null:
		_fade.kill()
	for location in _layers:
		for layer: AudioStreamPlayer in _layers[location]:
			layer.stop()
	get_tree().call_group(WORLD_AUDIO, &"stop")
	# Let the audio mixer release its active stream references before shutdown.
	await get_tree().create_timer(0.12).timeout
	get_tree().quit()
