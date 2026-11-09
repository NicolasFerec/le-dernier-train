extends Node2D

const PLAYER := preload("res://player/player.tscn")
const ENEMY := preload("res://enemy/enemy.tscn")
const FLOOR := Rect2(24, 66, 752, 394)
const BOUNDS := Rect2(46, 88, 708, 350)
const SPAWNS := [Vector2(220, 215), Vector2(220, 315), Vector2(140, 215), Vector2(140, 315)]
const STATE_INTERVAL := 1.0 / 30.0

const ROOMS := [
	[Vector2(610, 265)],
	[Vector2(610, 165), Vector2(610, 365)],
]
const REWARD := 3

const FLOOR_TILE := preload("res://assets/tiny_dungeon/floor.png")
const WALL_TILE := preload("res://assets/tiny_dungeon/wall.png")
const STATIONS := [
	[Vector2(145, 160), "CHARACTER_STATION", preload("res://assets/tiny_dungeon/mirror.png")],
	[Vector2(315, 160), "ARSENAL_STATION", preload("res://assets/tiny_dungeon/weapon.png")],
	[Vector2(485, 160), "PASSIVES_STATION", preload("res://assets/tiny_dungeon/passive.png")],
	[Vector2(655, 160), "DEPARTURE_STATION", preload("res://assets/tiny_dungeon/gate.png")],
]

var open_station := -1
var slots := {1: 0}
var loadouts := {}
var players := {}
var enemies: Array[Node2D] = []
var room := 0
var room_token := 0
var state_time := 0.0
var connected := true
var connecting := false
var lobby_notice := ""

@onready var ui: CanvasLayer = $Interface


func _ready() -> void:
	ui.host_requested.connect(_host)
	ui.language_changed.connect(func():
		lobby_notice = ""
		queue_redraw()
	)
	ui.join_requested.connect(_join)
	ui.leave_requested.connect(func(): Session.leave())
	ui.start_requested.connect(_start_run)
	ui.advance_requested.connect(_advance)
	Profile.changed.connect(_send_loadout)
	Session.ended.connect(_reset)
	Session.notice.connect(func(message): lobby_notice = message)
	Session.steam_hosted.connect(_steam_hosted)
	Session.steam_connecting.connect(_begin_join)
	ui.steam_host_requested.connect(Session.host_steam)
	ui.steam_join_requested.connect(Session.join_steam)
	ui.steam_invite_requested.connect(Session.invite_steam)
	multiplayer.connected_to_server.connect(_joined)
	multiplayer.peer_connected.connect(_peer_joined)
	multiplayer.peer_disconnected.connect(_peer_left)
	_add_player(1, 0)


func _host() -> void:
	var error := Session.host()
	if error != OK:
		lobby_notice = tr("HOST_FAILED") % error_string(error)
		return
	lobby_notice = tr("UDP_LOBBY") % Session.PORT
	ui.set_session(true)


func _join(address: String) -> void:
	if address.is_empty():
		lobby_notice = tr("ENTER_ADDRESS")
		return
	var error := Session.join(address)
	if error != OK:
		lobby_notice = tr("CONNECT_ERROR") % error_string(error)
		return
	_begin_join()


func _steam_hosted() -> void:
	lobby_notice = tr("STEAM_LOBBY")
	ui.set_session(true)


func _begin_join() -> void:
	connecting = true
	connected = false
	_clear_room()
	ui.set_session(true)
	ui.show_phase(0, false, true)
	ui.show_status(tr("CONNECTING"))


func _joined() -> void:
	connected = true
	connecting = false
	_send_loadout()


func _reset(message: String) -> void:
	connected = true
	connecting = false
	state_time = 0.0
	slots = {1: 0}
	loadouts.clear()
	room = 0
	room_token = 0
	lobby_notice = message
	_clear_room()
	_add_player(1, 0)
	ui.set_session(false)
	queue_redraw()
	_show_state()


func _peer_joined(id: int) -> void:
	if not multiplayer.is_server():
		return
	if room > 0:
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
	if connected and room == 0 and not multiplayer.is_server():
		var kit: Array[int] = Profile.loadout()
		_submit_loadout.rpc_id(1, kit[0], kit[1], kit[2], kit[3])


@rpc("any_peer", "call_remote", "reliable")
func _submit_loadout(character: int, weapon: int, health: int, speed: int) -> void:
	var sender := multiplayer.get_remote_sender_id()
	if not multiplayer.is_server() or room != 0 or not slots.has(sender):
		return
	loadouts[sender] = [clampi(character, 0, 1), clampi(weapon, 0, 1), clampi(health, 0, 1), clampi(speed, 0, 1)]


func _start_run() -> void:
	if not connected or not multiplayer.is_server() or room != 0:
		return
	loadouts[1] = Profile.loadout()
	for id in slots:
		if not loadouts.has(id):
			lobby_notice = tr("LOADOUT_SYNC")
			return
	Session.set_admission(false)
	ui.set_network_available(false)
	_enter_room(1)


func _enter_room(number: int) -> void:
	_clear_room()
	room = number
	room_token += 1
	for id in slots:
		var kit: Array = loadouts[id]
		var player := _add_player(id, slots[id])
		player.character = kit[0]
		player.max_hp = (4 if kit[0] == 1 else 5) + kit[2]
		player.hp = player.max_hp
		player.speed = (255.0 if kit[0] == 1 else 220.0) * (1.0 + 0.15 * kit[3])
		if kit[1] == 1:
			if kit[0] == 0:
				player.damage = 2
				player.reach = 62.0
				player.attack_delay = 0.58
			else:
				player.reach = 56.0
				player.attack_delay = 0.23
	for spawn in ROOMS[number - 1]:
		_add_enemy(spawn)
	queue_redraw()


func _advance() -> void:
	if not multiplayer.is_server() or room == 0:
		return
	var alive := _party_alive()
	if alive and _remaining() > 0:
		return
	if alive and room < ROOMS.size():
		_enter_room(room + 1)
		return
	if alive:
		_grant_reward(REWARD)
		if Session.is_online() and not multiplayer.get_peers().is_empty():
			_grant_reward.rpc(REWARD)
	else:
		lobby_notice = tr("PARTY_DOWN_NOTICE")
	room = 0
	room_token += 1
	_clear_room()
	for id in slots:
		_add_player(id, slots[id])
	Session.set_admission(true)
	ui.set_network_available(not Session.is_online())
	queue_redraw()


@rpc("authority", "call_remote", "reliable")
func _grant_reward(amount: int) -> void:
	var error := Profile.reward(amount)
	lobby_notice = tr("RUN_REWARD") % amount if error == OK else tr("REWARD_SAVE_FAILED")


func _clear_room() -> void:
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
	player.position = SPAWNS[slot]
	add_child(player)
	players[id] = player
	return player


func _add_enemy(spawn: Vector2) -> void:
	var enemy := ENEMY.instantiate()
	enemy.position = spawn
	add_child(enemy)
	enemies.append(enemy)


func _physics_process(delta: float) -> void:
	if not connected:
		return
	var movement := Vector2.ZERO
	if not ui.address.has_focus():
		movement = Vector2(
			int(Input.is_physical_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT))
			- int(Input.is_physical_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT)),
			int(Input.is_physical_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN))
			- int(Input.is_physical_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP))
		).limit_length()
	if not multiplayer.is_server():
		_submit_movement.rpc_id(1, room_token, movement)
		return
	players[1].movement = movement
	for id in players:
		var player: Node2D = players[id]
		if room == 0:
			var kit: Array = Profile.loadout() if id == 1 else loadouts.get(id, [0])
			player.character = kit[0]
		player.step(delta, BOUNDS)
	for enemy in enemies:
		enemy.step(delta, players)
		enemy.queue_redraw()
	_show_state()
	state_time += delta
	if state_time >= STATE_INTERVAL:
		state_time -= STATE_INTERVAL
		if Session.is_online() and not multiplayer.get_peers().is_empty():
			_broadcast_state()


func _unhandled_key_input(event: InputEvent) -> void:
	if not connected or not event.is_pressed() or event.is_echo():
		return
	if room == 0:
		if event.physical_keycode == KEY_E:
			var station := _nearby_station()
			open_station = station if open_station != station else -1
		return
	if event.physical_keycode != KEY_J and event.keycode != KEY_SPACE:
		return
	var dodge: bool = event.keycode == KEY_SPACE
	if multiplayer.is_server():
		_act(1, dodge)
	else:
		_submit_action.rpc_id(1, room_token, dodge)


@rpc("any_peer", "call_remote", "unreliable_ordered")
func _submit_movement(token: int, direction: Vector2) -> void:
	var sender := multiplayer.get_remote_sender_id()
	if multiplayer.is_server() and token == room_token and players.has(sender) and direction.is_finite():
		players[sender].movement = direction.limit_length()


@rpc("any_peer", "call_remote", "reliable")
func _submit_action(token: int, dodge: bool) -> void:
	if multiplayer.is_server() and token == room_token:
		_act(multiplayer.get_remote_sender_id(), dodge)


func _act(id: int, dodge: bool) -> void:
	var player: Node2D = players.get(id)
	if player == null or player.hp <= 0:
		return
	if dodge:
		player.dodge()
	elif player.attack_time <= 0:
		player.attack_time = player.attack_delay
		for enemy in enemies:
			var offset := enemy.position - player.position
			if enemy.hp > 0 and offset.length() <= player.reach and player.facing.dot(offset.normalized()) >= 0.4:
				enemy.hp = maxi(0, enemy.hp - player.damage)


func _broadcast_state() -> void:
	var roster := {}
	for id in players:
		var player: Node2D = players[id]
		roster[id] = [player.slot, player.position, player.hp, player.facing, player.attack_time,
			player.dash_time, player.max_hp, player.reach, player.attack_delay,
			player.dash_cooldown, player.character]
	var enemy_states := []
	for enemy in enemies:
		enemy_states.append([enemy.position, enemy.hp])
	_receive_state.rpc(room, room_token, slots.size(), roster, enemy_states)


@rpc("authority", "call_remote", "unreliable_ordered")
func _receive_state(number: int, token: int, count: int, roster: Dictionary, enemy_states: Array) -> void:
	if not connected:
		return
	if room_token != token:
		_clear_room()
		room = number
		room_token = token
		queue_redraw()
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
		player.queue_redraw()
	for index in enemy_states.size():
		var state: Array = enemy_states[index]
		if index == enemies.size():
			_add_enemy(state[0])
		enemies[index].position = state[0]
		enemies[index].hp = state[1]
		enemies[index].queue_redraw()
	_show_state(count)


func _remaining() -> int:
	var remaining := 0
	for enemy in enemies:
		remaining += int(enemy.hp > 0)
	return remaining


func _party_alive() -> bool:
	for player in players.values():
		if player.hp > 0:
			return true
	return false


func _show_state(count := -1) -> void:
	var authority := multiplayer.is_server()
	ui.set_steam_invite(room == 0 and authority and Session.steam_lobby != 0)
	if room == 0:
		ui.show_phase(0, authority, connecting)
		var station := _nearby_station()
		if open_station != station:
			open_station = -1
		ui.show_station(open_station, authority)
		var prompt := tr("LOBBY_HELP")
		if station >= 0:
			prompt = tr("CLOSE_PROMPT") if open_station >= 0 else "[E] %s" % tr(STATIONS[station][1])
		ui.show_prompt(prompt)
		if Session.is_online():
			var size := slots.size() if authority else count
			ui.show_status(tr("PLAYER_COUNT") % [size, Session.MAX_PLAYERS] + _notice_suffix())
		else:
			ui.show_status(tr("SOLO") + _notice_suffix())
		return
	var local_player: Node2D = players.get(multiplayer.get_unique_id())
	if local_player == null:
		return
	var remaining := _remaining()
	var outcome := tr("ENEMY_COUNT") % remaining
	var advance := ""
	if not _party_alive():
		outcome = tr("PARTY_DOWN")
		advance = tr("RETURN_LOBBY")
	elif remaining == 0:
		outcome = tr("ROOM_CLEARED")
		advance = tr("NEXT_ROOM") if room < ROOMS.size() else tr("CLAIM_REWARD")
	ui.show_phase(room, authority, false, advance)
	ui.show_player(local_player)
	ui.show_status(tr("ROOM_STATUS") % [
		room, ROOMS.size(), local_player.hp, local_player.max_hp, outcome
	])


func _notice_suffix() -> String:
	return "" if lobby_notice.is_empty() else "   ·   " + lobby_notice


func _nearby_station() -> int:
	var player: Node2D = players.get(multiplayer.get_unique_id())
	if player == null:
		return -1
	for index in STATIONS.size():
		if player.position.distance_to(STATIONS[index][0]) <= 52:
			return index
	return -1


func _draw() -> void:
	var tint := Color("#72667b") if room == 2 else Color("#76849c")
	draw_texture_rect(FLOOR_TILE, FLOOR, true, tint)
	for x in range(24, 776, 32):
		var width := minf(32, 776 - x)
		draw_texture_rect(WALL_TILE, Rect2(x, 66, width, 24), false)
	draw_rect(FLOOR, Color("#555269"), false, 2)
	if room != 0:
		return
	var font := ThemeDB.fallback_font
	for station in STATIONS:
		var point: Vector2 = station[0]
		draw_circle(point, 35, Color("#272b3d"))
		draw_arc(point, 35, 0, TAU, 36, Color("#b29874"), 2, true)
		draw_texture_rect(station[2], Rect2(point - Vector2(19, 19), Vector2(38, 38)), false)
		var label := tr(station[1])
		var label_size := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, 16)
		var label_position := point + Vector2(-label_size.x / 2, -49)
		draw_rect(Rect2(label_position - Vector2(7, 17), Vector2(label_size.x + 14, 23)), Color("#1b2030"))
		draw_string(font, label_position, label, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color("#ffe0b7"))
