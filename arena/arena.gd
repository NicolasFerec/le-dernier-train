extends Node2D

const PLAYER := preload("res://player/player.tscn")
const ENEMY := preload("res://enemy/enemy.tscn")
const STATE_INTERVAL := 1.0 / 30.0
const DEPARTURE_DELAY := 10.0
const REWARD := 3

var open_station := -1
var slots := {1: 0}
var loadouts := {}
var players := {}
var enemies: Array[Node2D] = []
var room := 0
var room_token := 0
var run_finished := false
var rooms_cleared := 0
var run_stats := {}
var departure_time := 0.0
var game_paused := false
var state_time := 0.0
var _motion_epoch := Time.get_ticks_msec()
var _player_motion: Array[NetworkPlayerMotion] = []
var _enemy_motion: Array[NetworkEnemyMotion] = []
var connected := true
var connecting := false
var lobby_notice := ""
var visible_area := Rect2()
var design_layout := Transform2D.IDENTITY

@onready var mission: MissionRoom = $MissionRoom
@onready var train: TrainLobby = $TrainLobby
@onready var ui: CanvasLayer = $Interface


func _ready() -> void:
	_setup_network_motion()
	GameSettings.set_menu(false)
	get_viewport().size_changed.connect(_fit_viewport)
	_fit_viewport()
	ui.session_menu.local_host_requested.connect(_host)
	ui.session_menu.local_join_requested.connect(_join)
	ui.session_menu.resume_requested.connect(_toggle_session_menu)
	ui.invitation_menu.close_requested.connect(_close_station)
	ui.close_station_requested.connect(_close_station)
	ui.start_requested.connect(_start_run)
	ui.advance_requested.connect(_advance)
	ui.run_summary.return_requested.connect(_return_from_summary)
	Profile.changed.connect(_send_loadout)
	Session.ended.connect(_reset)
	Session.notice.connect(func(message: String):
		lobby_notice = message
		ui.session_menu.refresh_session(room, connecting, lobby_notice)
		ui.invitation_menu.refresh_session(connecting, lobby_notice)
	)
	Session.steam_hosted.connect(_steam_hosted)
	Session.steam_connecting.connect(_begin_join)
	multiplayer.connected_to_server.connect(_joined)
	multiplayer.peer_connected.connect(_peer_joined)
	multiplayer.peer_disconnected.connect(_peer_left)
	_add_player(1, 0)
	Session.invitation_received.connect(func(_code: String): Session.accept_pending_invitation())
	Session.accept_pending_invitation.call_deferred()


func _fit_viewport() -> void:
	design_layout = GameSettings.design_transform()
	ui.layout_hud(design_layout)
	visible_area = design_layout.affine_inverse() * get_viewport().get_visible_rect()
	_update_camera()


func _update_camera(delta := 0.0) -> void:
	train.visible = room == 0
	GameAudio.set_scene(GameAudio.Scene.WAGON if train.visible else GameAudio.Scene.SILENT)
	var local_player: Node2D = players.get(multiplayer.get_unique_id())
	var fallback := Vector2(TrainLobby.SPAWNS[0]) if room == 0 else mission.spawn_point(0)
	var focus := Vector2(local_player.render_position) if local_player != null else fallback
	transform = design_layout.translated_local(visible_area.get_center() - Isometric.GROUND * focus)
	if train.visible:
		train.update_furniture_visibility(focus, delta)


func _setup_network_motion() -> void:
	# Stable RPC paths let netfox receive state during scene changes and late joins.
	for slot in Session.MAX_PLAYERS:
		var lane := NetworkPlayerMotion.new()
		lane.name = "PlayerMotion%d" % slot
		add_child(lane)
		_player_motion.append(lane)
	var max_enemies := 0
	for layout in MissionRoom.LAYOUTS:
		max_enemies = maxi(max_enemies, layout.spawns.size())
	for index in max_enemies:
		var lane := NetworkEnemyMotion.new()
		lane.name = "EnemyMotion%d" % index
		add_child(lane)
		_enemy_motion.append(lane)
	NetworkTime.before_tick_loop.connect(_prepare_network_motion)
	NetworkTime.after_tick_loop.connect(_finish_network_motion)


func _prepare_network_motion() -> void:
	var active_slots := {}
	var local_id := multiplayer.get_unique_id()
	for id in players:
		var player: PlayerLocomotion = players[id]
		var lane := _player_motion[player.slot]
		active_slots[player.slot] = true
		lane.bind(id, _motion_epoch, player, _movement_bounds(), _movement_obstacles())
		lane.paused = game_paused or run_finished
		lane.body.hp = player.hp
		lane.body.speed = player.speed
		if id == local_id:
			lane.gather(_read_movement())
	for slot in _player_motion.size():
		if not active_slots.has(slot):
			_player_motion[slot].bind(0, _motion_epoch, null, Rect2(), [])
	for index in _enemy_motion.size():
		var enemy: Node2D = enemies[index] if index < enemies.size() else null
		_enemy_motion[index].prepare(enemy, _motion_epoch)


func _finish_network_motion() -> void:
	var authority := multiplayer.is_server()
	for id in players:
		var player: Node2D = players[id]
		var lane := _player_motion[player.slot]
		if not lane.has_pose():
			continue
		lane.record_pose()
		if authority and not game_paused and not run_finished:
			player.position = lane.body.position
			player.facing = lane.body.facing
			player.movement = lane.body.movement
			player.dash_time = lane.body.dash_time
			player.dash_cooldown = lane.body.dash_cooldown
			player.dash_direction = lane.body.dash_direction
			if lane.dodge_started > lane.played_dodge:
				lane.played_dodge = lane.dodge_started
				_emit_world_sound(GameAudio.Cue.DASH, player.position)
	for lane in _enemy_motion:
		lane.record_pose()


func _process(delta: float) -> void:
	var fraction := Engine.get_physics_interpolation_fraction()
	var online := Session.is_online()
	var authority := multiplayer.is_server()
	for id in players:
		var player: Node2D = players[id]
		var lane := _player_motion[player.slot]
		player.render_position = player.position
		player.render_movement = player.movement
		player.render_facing = player.facing
		player.render_dash_time = player.dash_time
		if not game_paused and not run_finished and online and lane.has_pose() and lane.stamp == Vector2i(id, _motion_epoch):
			player.render_position = lane.render_position
			player.render_movement = lane.body.movement
			player.render_facing = lane.body.facing
			player.render_dash_time = lane.body.dash_time
		elif not game_paused and not run_finished and not online:
			player.render_position = player.previous_position.lerp(player.position, fraction)
	for index in enemies.size():
		var enemy := enemies[index]
		var lane := _enemy_motion[index]
		if not game_paused and not run_finished and online and not authority and lane.has_pose(_motion_epoch):
			enemy.render_position = lane.render_position
		elif not game_paused and not run_finished and authority:
			enemy.render_position = enemy.previous_position.lerp(enemy.position, fraction)
		else:
			enemy.render_position = enemy.position
	_update_camera(delta)


func _host() -> void:
	var error := Session.host()
	lobby_notice = "" if error == OK else tr("HOST_FAILED") % error_string(error)
	ui.session_menu.refresh_session(room, connecting, lobby_notice)


func _join() -> void:
	var error := Session.join()
	if error != OK:
		lobby_notice = tr("CONNECT_ERROR") % error_string(error)
		ui.session_menu.refresh_session(room, connecting, lobby_notice)
		return
	lobby_notice = ""
	_begin_join()


func _steam_hosted() -> void:
	lobby_notice = ""
	ui.session_menu.refresh_session(room, connecting, lobby_notice)


func _begin_join() -> void:
	connecting = true
	connected = false
	_clear_room()
	_set_game_paused(false)
	ui.session_menu.refresh_session(room, connecting, lobby_notice)
	ui.show_phase(0, false, true)
	train.show_interaction(-1)
	_update_camera()


func _joined() -> void:
	connected = true
	connecting = false
	lobby_notice = ""
	ui.session_menu.hide()
	_show_state()
	_send_loadout()


func _reset(message: String) -> void:
	connected = true
	connecting = false
	state_time = 0.0
	slots = {1: 0}
	loadouts.clear()
	room_token = 0
	lobby_notice = message
	_change_room(0)
	_set_game_paused(ui.session_menu.visible)
	ui.clear_guest_pause_notice()
	_add_player(1, 0)
	_show_state()


func _peer_joined(id: int) -> void:
	if not multiplayer.is_server():
		return
	if room > 0 or departure_time > 0:
		multiplayer.multiplayer_peer.disconnect_peer(id)
		return
	for slot in range(Session.MAX_PLAYERS):
		if not slots.values().has(slot):
			slots[id] = slot
			_add_player(id, slot)
			return
	multiplayer.multiplayer_peer.disconnect_peer(id)


func _peer_left(id: int) -> void:
	if not multiplayer.is_server():
		return
	slots.erase(id)
	loadouts.erase(id)
	if players.has(id):
		players[id].queue_free()
		players.erase(id)


func _send_loadout() -> void:
	if not connected or room != 0:
		return
	var kit: Array[int] = Profile.loadout()
	if multiplayer.is_server():
		players[1].equip(kit)
	else:
		_submit_loadout.rpc_id(1, kit[0], kit[1], kit[2], kit[3])


@rpc("any_peer", "call_remote", "reliable")
func _submit_loadout(character: int, weapon: int, health: int, speed: int) -> void:
	var sender := multiplayer.get_remote_sender_id()
	if not multiplayer.is_server() or room != 0 or not slots.has(sender):
		return
	var kit := [clampi(character, 0, 1), clampi(weapon, 0, 1), clampi(health, 0, 1), clampi(speed, 0, 1)]
	loadouts[sender] = kit
	players[sender].equip(kit)


func _start_run() -> void:
	if not connected or not multiplayer.is_server() or room != 0 or game_paused or departure_time > 0:
		return
	if _nearby_station() != TrainLobby.DEPARTURE_STATION:
		return
	for id in slots:
		if id != 1 and not loadouts.has(id):
			lobby_notice = tr("LOADOUT_SYNC")
			return
	Session.set_admission(false)
	if _party_at_departure():
		_begin_run()
	else:
		departure_time = DEPARTURE_DELAY
		_show_state()


func _party_at_departure() -> bool:
	var center: Vector2 = TrainLobby.STATIONS[TrainLobby.DEPARTURE_STATION][0]
	for player in players.values():
		if player.position.distance_squared_to(center) > TrainLobby.DEPARTURE_RADIUS * TrainLobby.DEPARTURE_RADIUS:
			return false
	return true


func _begin_run() -> void:
	loadouts[1] = Profile.loadout()
	rooms_cleared = 0
	run_stats.clear()
	for id in slots:
		var slot: int = slots[id]
		var name_text := Session.player_name(id, slot + 1) if Session.steam_lobby != 0 else ""
		run_stats[id] = {"slot": slot, "name": name_text, "kills": 0, "dealt": 0, "taken": 0}
	_enter_room(1)


func _enter_room(number: int) -> void:
	room_token += 1
	_change_room(number)
	for id in slots:
		_add_player(id, slots[id])
	for spawn in mission.enemy_spawns:
		_add_enemy(spawn)
	_show_state()


func _advance() -> void:
	if not multiplayer.is_server() or room == 0 or departure_time > 0 or game_paused or run_finished:
		return
	if _remaining() > 0 or not _host_can_depart():
		return
	if _players_at_exit() == _living_players():
		_finish_room(true)
	else:
		departure_time = DEPARTURE_DELAY
		_show_state()


func _finish_room(alive: bool) -> void:
	if alive and room < MissionRoom.LAYOUTS.size():
		_enter_room(room + 1)
		return
	if alive:
		_grant_reward(REWARD)
		if Session.is_online() and not multiplayer.get_peers().is_empty():
			_grant_reward.rpc(REWARD)
	if Session.is_online() and not multiplayer.get_peers().is_empty():
		_finish_run.rpc(room_token, alive, rooms_cleared, run_stats)
	else:
		_finish_run(room_token, alive, rooms_cleared, run_stats)


@rpc("authority", "call_local", "reliable")
func _finish_run(token: int, victory: bool, cleared: int, stats: Dictionary) -> void:
	if not connected or token < room_token or run_finished:
		return
	run_finished = true
	departure_time = 0.0
	ui.show_departure(0)
	ui.show_run_summary(victory, cleared, MissionRoom.LAYOUTS.size(), stats, multiplayer.is_server())


func _return_from_summary() -> void:
	if not multiplayer.is_server() or not run_finished:
		return
	room_token += 1
	_change_room(0)
	for id in slots:
		_add_player(id, slots[id])
	Session.set_admission(true)
	_show_state()


func _change_room(number: int) -> void:
	_clear_room()
	room = number
	if number == 0:
		run_finished = false
		ui.run_summary.hide()
	_motion_epoch += 1
	mission.configure(number)


@rpc("authority", "call_remote", "reliable")
func _grant_reward(amount: int) -> void:
	var error := Profile.reward(amount)
	lobby_notice = tr("RUN_REWARD") % amount if error == OK else tr("REWARD_SAVE_FAILED")


func _clear_room() -> void:
	departure_time = 0.0
	open_station = -1
	for player in players.values():
		player.queue_free()
	players.clear()
	for enemy in enemies:
		enemy.queue_free()
	enemies.clear()


func _add_player(id: int, slot: int) -> Node2D:
	var player := PLAYER.instantiate()
	player.slot = slot
	player.try_3d_visual = room == 0 and id == multiplayer.get_unique_id() and "--conductor-3d" in OS.get_cmdline_user_args()
	player.paused = game_paused
	player.position = TrainLobby.SPAWNS[slot] if room == 0 else mission.spawn_point(slot)
	player.previous_position = player.position
	player.render_position = player.position
	if multiplayer.is_server():
		var kit: Array = Profile.loadout() if id == 1 else loadouts.get(id, [0, 0, 0, 0])
		player.equip(kit)
	add_child(player)
	players[id] = player
	return player


func _add_enemy(spawn: Vector2) -> void:
	var enemy := ENEMY.instantiate()
	enemy.position = spawn
	enemy.previous_position = spawn
	enemy.render_position = spawn
	enemy.paused = game_paused
	enemy.struck.connect(func(target_id: int):
		run_stats[target_id].taken += 1
		if target_id == multiplayer.get_unique_id():
			ControlHints.vibrate_on_hit()
		elif Session.is_online():
			_receive_hit_feedback.rpc_id(target_id, room_token)
		_emit_world_sound(GameAudio.Cue.ENEMY_STRIKE, enemy.position)
	)
	add_child(enemy)
	enemies.append(enemy)


@rpc("authority", "call_remote", "reliable")
func _receive_hit_feedback(token: int) -> void:
	if connected and token >= room_token:
		ControlHints.vibrate_on_hit()


func _physics_process(delta: float) -> void:
	if not connected or not multiplayer.is_server():
		return
	var online := Session.is_online()
	if not game_paused and not run_finished:
		if not online:
			players[1].movement = _read_movement()
		for id in players:
			var player: Node2D = players[id]
			var previous: float = player.swing_progress()
			if online:
				player.step_attack(delta)
			else:
				player.step(delta, _movement_bounds(), _movement_obstacles())
			if previous < 1.0 and player.hp > 0:
				_resolve_attack(id, player, previous)
		for enemy in enemies:
			enemy.step(delta, players)
		if room > 0 and _living_players() == 0:
			_finish_room(false)
		elif departure_time > 0:
			departure_time = maxf(0.0, departure_time - delta)
			if room == 0:
				if departure_time <= 0 or _party_at_departure():
					_begin_run()
			elif departure_time <= 0 or _players_at_exit() == _living_players():
				_finish_room(true)
		_show_state()
	if online and not multiplayer.get_peers().is_empty():
		state_time += delta
		if state_time >= STATE_INTERVAL:
			state_time -= STATE_INTERVAL
			_broadcast_state()


func _read_movement() -> Vector2:
	if game_paused or run_finished or open_station >= 0:
		return Vector2.ZERO
	# The controls follow the two ground axes. The camera projects them once.
	return Input.get_vector("move_left", "move_right", "move_up", "move_down")


func _movement_bounds() -> Rect2:
	return TrainLobby.BOUNDS if room == 0 else mission.bounds


func _movement_obstacles() -> Array[Rect2]:
	return train.obstacles if room == 0 else []


func _set_game_paused(active: bool) -> void:
	if game_paused != active:
		game_paused = active
		if multiplayer.is_server():
			_motion_epoch += 1
		for player in players.values():
			player.paused = active
			if active:
				player.movement = Vector2.ZERO
				player.previous_position = player.position
		for enemy in enemies:
			enemy.paused = active
			if active:
				enemy.previous_position = enemy.position
		train.set_process(not active)
	ui.show_remote_pause(active and not multiplayer.is_server())


func _toggle_session_menu() -> void:
	if Session.is_online() and not multiplayer.is_server():
		ui.show_guest_pause_notice()
		return
	if ui.session_menu.visible:
		ui.session_menu.hide()
	else:
		open_station = -1
		ui.session_menu.refresh_session(room, connecting, lobby_notice)
		ui.session_menu.open()
	_set_game_paused(ui.session_menu.visible)
	_show_state()
	if Session.is_online() and not multiplayer.get_peers().is_empty():
		_broadcast_state()


func _close_station() -> void:
	open_station = -1
	_show_state()


func _unhandled_input(event: InputEvent) -> void:
	if run_finished or not event.is_pressed() or event.is_echo():
		return
	if open_station >= 0 and (event.is_action_pressed("ui_cancel") or event.is_action_pressed("session_menu")):
		if event.is_action_pressed("session_menu") and Session.is_online() and not multiplayer.is_server():
			ui.show_guest_pause_notice()
			get_viewport().set_input_as_handled()
			return
		_close_station()
		get_viewport().set_input_as_handled()
		return
	if event.is_action_pressed("session_menu") or (ui.session_menu.visible and event.is_action_pressed("ui_cancel")):
		_toggle_session_menu()
		get_viewport().set_input_as_handled()
		return
	if not connected or ui.session_menu.visible:
		return
	if open_station >= 0:
		if event is InputEventKey and event.is_action_pressed("interact"):
			_close_station()
			get_viewport().set_input_as_handled()
		return
	if game_paused:
		return
	if event.is_action_pressed("interact"):
		if room == 0:
			open_station = _nearby_station()
			if open_station >= 0:
				_play_world_sound(room_token, GameAudio.Cue.STATION, TrainLobby.STATIONS[open_station][0])
			_show_state()
		else:
			_advance()
		get_viewport().set_input_as_handled()
		return
	var dodge := event.is_action_pressed("dodge")
	var heavy := event.is_action_pressed("heavy_attack")
	if not dodge and not heavy and not event.is_action_pressed("attack"):
		return
	if dodge and Session.is_online():
		var local: Node2D = players.get(multiplayer.get_unique_id())
		if local != null:
			var lane := _player_motion[local.slot]
			lane.bind(multiplayer.get_unique_id(), _motion_epoch, local, _movement_bounds(), _movement_obstacles())
			lane.request_dodge()
	elif multiplayer.is_server():
		_act(1, dodge, heavy)
	else:
		_submit_attack.rpc_id(1, room_token, heavy)
	get_viewport().set_input_as_handled()


@rpc("any_peer", "call_remote", "reliable")
func _submit_attack(token: int, heavy: bool) -> void:
	if multiplayer.is_server() and token == room_token:
		_act(multiplayer.get_remote_sender_id(), false, heavy)


func _act(id: int, dodge: bool, heavy := false) -> void:
	var player: Node2D = players.get(id)
	if game_paused or run_finished or player == null or player.hp <= 0:
		return
	if dodge:
		if player.dodge():
			_emit_world_sound(GameAudio.Cue.DASH, player.position)
	elif player.attack_time <= 0:
		player.start_attack(heavy)
		var cue := GameAudio.Cue.BLADE
		if player.weapon == 1:
			cue = GameAudio.Cue.HAMMER if player.character == 0 else GameAudio.Cue.DAGGERS
		_emit_world_sound(cue, player.position)


func _resolve_attack(id: int, player: Node2D, previous: float) -> void:
	if room == 0:
		for index in TrainLobby.DUMMIES.size():
			if player.swept_hit(index, TrainLobby.DUMMIES[index], previous, 12.0):
				_practice_hit(room_token, index)
				if Session.is_online() and not multiplayer.get_peers().is_empty():
					_practice_hit.rpc(room_token, index)
		return
	var hit_damage: int = player.damage * (2 if player.heavy_attack else 1)
	for enemy in enemies:
		if enemy.hp > 0 and player.swept_hit(enemy, enemy.position, previous):
			var damage := mini(enemy.hp, hit_damage)
			enemy.hp -= damage
			run_stats[id].dealt += damage
			_emit_world_sound(GameAudio.Cue.ENEMY_HIT, enemy.position)
			if enemy.hp == 0:
				run_stats[id].kills += 1
				_emit_world_sound(GameAudio.Cue.ENEMY_FALL, enemy.position)
	if _remaining() == 0:
		rooms_cleared = room


@rpc("authority", "call_remote", "unreliable")
func _practice_hit(token: int, index: int) -> void:
	if room == 0 and room_token == token and index >= 0 and index < TrainLobby.DUMMIES.size():
		train.flash(index)
		_play_world_sound(token, GameAudio.Cue.DUMMY, TrainLobby.DUMMIES[index])


func _emit_world_sound(cue: int, point: Vector2) -> void:
	_play_world_sound(room_token, cue, point)
	if Session.is_online() and not multiplayer.get_peers().is_empty():
		_play_world_sound.rpc(room_token, cue, point)


@rpc("authority", "call_remote", "unreliable")
func _play_world_sound(token: int, cue: int, point: Vector2) -> void:
	if connected and room_token == token:
		GameAudio.play_at(self, cue, Isometric.GROUND * point)


func _broadcast_state() -> void:
	var roster := {}
	for id in players:
		var player: Node2D = players[id]
		roster[id] = [player.slot, player.position, player.hp, player.facing, player.attack_time,
			player.dash_time, player.max_hp, player.reach, player.attack_delay,
			player.dash_cooldown, player.character, player.weapon, player.movement, player.dash_direction, player.speed,
			player.attack_origin, player.attack_direction, player.heavy_attack, player.reverse_light_swing]
	var enemy_states := []
	for enemy in enemies:
		enemy_states.append([enemy.position, enemy.hp, enemy.facing, enemy.moving, enemy.hit_cooldown])
	_receive_state.rpc(room, room_token, _motion_epoch, roster, enemy_states, departure_time, game_paused)


@rpc("authority", "call_remote", "unreliable_ordered")
func _receive_state(number: int, token: int, epoch: int, roster: Dictionary, enemy_states: Array, countdown: float, paused: bool) -> void:
	if not connected:
		return
	if room_token != token:
		_change_room(number)
		room_token = token
	_set_game_paused(paused)
	departure_time = countdown
	_motion_epoch = epoch
	for id in players.keys():
		if not roster.has(id):
			players[id].queue_free()
			players.erase(id)
	for id in roster:
		var state: Array = roster[id]
		var player: Node2D = players.get(id)
		if player == null:
			player = _add_player(id, state[0])
		player.position = state[1]
		player.hp = state[2]
		player.facing = state[3]
		player.attack_time = state[4]
		player.dash_time = state[5]
		player.max_hp = state[6]
		player.reach = state[7]
		player.attack_delay = state[8]
		player.dash_cooldown = state[9]
		player.character = state[10]
		player.weapon = state[11]
		player.movement = state[12]
		player.dash_direction = state[13]
		player.speed = state[14]
		player.attack_origin = state[15]
		player.attack_direction = state[16]
		player.heavy_attack = state.size() > 17 and state[17]
		player.reverse_light_swing = state.size() > 18 and state[18]
	for index in enemy_states.size():
		var state: Array = enemy_states[index]
		if index == enemies.size():
			_add_enemy(state[0])
		enemies[index].position = state[0]
		enemies[index].hp = state[1]
		enemies[index].facing = state[2]
		enemies[index].moving = state[3]
		enemies[index].hit_cooldown = state[4]
	_show_state()


func _remaining() -> int:
	var remaining := 0
	for enemy in enemies:
		remaining += int(enemy.hp > 0)
	return remaining


func _living_players() -> int:
	var living := 0
	for player in players.values():
		living += int(player.hp > 0)
	return living


func _near_exit(player: Node2D) -> bool:
	return player.position.distance_squared_to(mission.gate) <= MissionRoom.GATE_RADIUS * MissionRoom.GATE_RADIUS


func _players_at_exit() -> int:
	var present := 0
	for player in players.values():
		if player.hp > 0 and _near_exit(player):
			present += 1
	return present


func _host_can_depart() -> bool:
	var host: Node2D = players[1]
	return _near_exit(host) if host.hp > 0 else _players_at_exit() > 0


func _show_state() -> void:
	if run_finished:
		return
	var authority := multiplayer.is_server()
	ui.session_menu.refresh_session(room, connecting, lobby_notice)
	if room == 0:
		ui.show_phase(0, authority, connecting)
		ui.show_departure(ceili(departure_time))
		var station := _nearby_station() if not connecting else -1
		if open_station != station:
			open_station = -1
		ui.show_station(open_station, authority, lobby_notice)
		train.show_interaction(station if open_station < 0 and not ui.session_menu.visible else -1)
		return
	var local_player: Node2D = players.get(multiplayer.get_unique_id())
	if local_player == null:
		return
	var remaining := _remaining()
	var living := _living_players()
	var outcome := tr("ENEMY_COUNT") % remaining
	var button := ""
	var gate_prompt := ""
	var unlocked := remaining == 0 and living > 0
	if unlocked:
		outcome = tr("ROOM_GATE_FIND")
		var present := _players_at_exit()
		if living > 1:
			outcome = "%s   ·   %s" % [outcome, tr("ROOM_GATE_GROUP") % [present, living]]
		if departure_time <= 0 and not ui.session_menu.visible:
			var action := "NEXT_ROOM" if room < MissionRoom.LAYOUTS.size() else "ROOM_GATE_RETURN"
			if authority and _host_can_depart():
				if present < living:
					action = "ROOM_GATE_FORCE"
				if local_player.hp > 0:
					gate_prompt = action
				else:
					button = tr(action)
			elif not authority and _near_exit(local_player):
				gate_prompt = "ROOM_GATE_WAIT_HOST"
	mission.update_gate(unlocked, gate_prompt)
	ui.show_phase(room, authority, false, button)
	ui.show_departure(ceili(departure_time))
	ui.show_party(local_player, players)
	ui.show_status(tr("ROOM_STATUS") % [
		room, MissionRoom.LAYOUTS.size(), local_player.hp, local_player.max_hp, outcome
	])


func _nearby_station() -> int:
	var player: Node2D = players.get(multiplayer.get_unique_id())
	if player == null:
		return -1
	for index in TrainLobby.STATIONS.size():
		if index == TrainLobby.DEPARTURE_STATION and departure_time > 0:
			continue
		if player.position.distance_to(TrainLobby.STATIONS[index][0]) <= 66:
			return index
	return -1
