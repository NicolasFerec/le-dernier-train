extends Node

signal ended(message: String)
signal steam_hosted
signal steam_connecting
signal notice(message: String)
signal invitation_received(code: String)
signal roster_changed

const PORT := 7000
const MAX_PLAYERS := 4
const WIRE_PROTOCOL := "godot-roguelite-9"

var version: String = ProjectSettings.get_setting("application/config/version")
@onready var scene_network: SceneMultiplayer = multiplayer

var steam: Object
var steam_lobby := 0
var steam_host_id := 0
var creating_lobby := false
var joining_lobby := 0
var steam_status := "STEAM_REQUIRED"
var pending_invitation := ""
var roster: Array = []
var _pings := {}
var _ping_requests := {}
var _roster_timer := 0.0
var _discard_steam_request := false
var local_dev_enabled := OS.has_feature("editor") and "--local-multiplayer" in OS.get_cmdline_user_args()



func _ready() -> void:
	scene_network.auth_timeout = 5.0
	scene_network.auth_callback = _check_peer_version
	scene_network.peer_authenticating.connect(_send_version)
	scene_network.peer_authentication_failed.connect(_version_check_failed)
	multiplayer.connected_to_server.connect(NetworkTime.start)
	multiplayer.connection_failed.connect(func(): leave(tr("CONNECTION_FAILED")))
	multiplayer.server_disconnected.connect(func(): leave(tr("HOST_LEFT")))
	_initialize_steam()


func _send_version(peer_id: int) -> void:
	scene_network.send_auth(peer_id, ("%s|%s" % [version, WIRE_PROTOCOL]).to_utf8_buffer())


func _check_peer_version(peer_id: int, data: PackedByteArray) -> void:
	var hello := data.get_string_from_utf8() if data.size() <= 64 else ""
	var fields := hello.split("|", true, 1)
	var remote_version := fields[0]
	if remote_version == version and fields.size() == 2 and fields[1] == WIRE_PROTOCOL:
		scene_network.complete_auth(peer_id)
	elif not multiplayer.is_server():
		var message := tr("VERSION_INCOMPATIBLE") if remote_version == version else _version_message(remote_version)
		leave.call_deferred(message)


func _version_check_failed(peer_id: int) -> void:
	if peer_id == 1 and is_online():
		leave.call_deferred(_version_message(""))


func _version_message(host_version: String) -> String:
	var parts := host_version.split(".")
	if parts.size() != 3 or not parts[0].is_valid_int() or not parts[1].is_valid_int() or not parts[2].is_valid_int():
		return tr("VERSION_UNKNOWN")
	return tr("VERSION_MISMATCH") % [version, host_version]


func host() -> Error:
	if not local_dev_enabled:
		return ERR_UNAVAILABLE
	if _busy():
		return ERR_BUSY
	var peer := ENetMultiplayerPeer.new()
	peer.set_bind_ip("127.0.0.1")
	var error := peer.create_server(PORT, MAX_PLAYERS - 1)
	if error == OK:
		multiplayer.multiplayer_peer = peer
		NetworkTime.start()
	return error


func join() -> Error:
	if not local_dev_enabled:
		return ERR_UNAVAILABLE
	if _busy():
		return ERR_BUSY
	var peer := ENetMultiplayerPeer.new()
	var error := peer.create_client("127.0.0.1", PORT)
	if error == OK:
		multiplayer.multiplayer_peer = peer
	return error


func leave(message := "") -> void:
	_discard_steam_request = creating_lobby or joining_lobby != 0
	roster.clear()
	_pings.clear()
	_ping_requests.clear()
	_roster_timer = 0.0
	if is_online():
		NetworkTime.stop()
		multiplayer.multiplayer_peer.close()
		multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()
	if steam_lobby != 0:
		steam.leaveLobby(steam_lobby)
		steam_lobby = 0
		steam_host_id = 0
	ended.emit(message)
	roster_changed.emit()


func is_online() -> bool:
	return multiplayer.has_multiplayer_peer() and not multiplayer.multiplayer_peer is OfflineMultiplayerPeer


func set_admission(allowed: bool) -> void:
	if is_online():
		multiplayer.multiplayer_peer.refuse_new_connections = not allowed

	if steam_lobby != 0 and multiplayer.is_server():
		steam.setLobbyJoinable(steam_lobby, allowed)


func _busy() -> bool:
	return is_online() or creating_lobby or joining_lobby != 0


func _initialize_steam() -> void:
	var app_id := int(ProjectSettings.get_setting("steam/app_id", 0))
	if app_id <= 0:
		return
	if not Engine.has_singleton("Steam") or not ClassDB.class_exists("SteamMultiplayerPeer"):
		steam_status = "STEAM_EXTENSION_MISSING"
		return
	var api := Engine.get_singleton("Steam")
	if not api.isSteamRunning():
		steam_status = "STEAM_UNAVAILABLE"
		return
	var result: Dictionary = api.steamInitEx(app_id, false)
	if int(result.get("status", -1)) != 0:
		steam_status = "STEAM_UNAVAILABLE"
		push_warning(str(result))
		return
	steam = api
	steam.initRelayNetworkAccess()
	steam.connect("lobby_created", _steam_lobby_created)
	steam.connect("lobby_joined", _steam_lobby_joined)
	steam.connect("join_requested", _steam_invited)
	steam.connect("lobby_chat_update", _steam_membership_changed)
	var args := OS.get_cmdline_args() + OS.get_cmdline_user_args()
	var invite := args.find("+connect_lobby")
	if invite >= 0 and invite + 1 < args.size():
		pending_invitation = args[invite + 1]


func _process(delta: float) -> void:
	if steam != null:
		steam.run_callbacks()
	if is_online() and multiplayer.is_server():
		_roster_timer += delta
		if _roster_timer >= 1.0:
			_roster_timer = 0.0
			_update_roster()


func player_name(peer_id: int, index: int) -> String:
	if steam != null and (peer_id == multiplayer.get_unique_id() or steam_lobby != 0):
		if peer_id == multiplayer.get_unique_id():
			var local_name: String = steam.getPersonaName()
			if not local_name.is_empty():
				return local_name
		elif multiplayer.multiplayer_peer.has_method("get_steam_id_for_peer_id"):
			var steam_id: int = multiplayer.multiplayer_peer.get_steam_id_for_peer_id(peer_id)
			if steam_id != 0:
				var name_on_steam: String = steam.getFriendPersonaName(steam_id)
				if not name_on_steam.is_empty() and name_on_steam != "[unknown]":
					return name_on_steam
	return tr("SESSION_PLAYER_FALLBACK") % index


func _update_roster() -> void:
	var peers := multiplayer.get_peers()
	var now := Time.get_ticks_msec()
	roster = [[1, player_name(1, 1), -1]]
	for id in _pings.keys():
		if id not in peers:
			_pings.erase(id)
			_ping_requests.erase(id)
	for id in peers:
		var ping: int = _pings.get(id, -1)
		roster.append([id, player_name(id, roster.size() + 1), ping])
		_ping_requests[id] = now
		_ping.rpc_id(id, now)
	if not peers.is_empty():
		_sync_roster.rpc(roster)
	roster_changed.emit()


@rpc("authority", "call_remote", "unreliable")
func _ping(sent: int) -> void:
	_pong.rpc_id(1, sent)


@rpc("any_peer", "call_remote", "unreliable")
func _pong(sent: int) -> void:
	var id := multiplayer.get_remote_sender_id()
	if multiplayer.is_server() and _ping_requests.get(id, -1) == sent:
		_pings[id] = Time.get_ticks_msec() - sent
		_ping_requests.erase(id)


@rpc("authority", "call_remote", "unreliable_ordered")
func _sync_roster(entries: Array) -> void:
	roster = entries
	roster_changed.emit()


func host_steam() -> void:
	if steam == null:
		notice.emit(tr(steam_status))
	elif not _busy():
		creating_lobby = true
		notice.emit(tr("STEAM_CREATING"))
		steam.createLobby(1, MAX_PLAYERS) # Friends-only lobby.


func join_steam(code: String) -> void:
	if steam == null:
		notice.emit(tr(steam_status))
		return
	if _busy():
		notice.emit(tr("STEAM_LEAVE_FIRST"))
		return
	var lobby_id := code.to_int() if code.is_valid_int() else 0
	if lobby_id <= 0:
		notice.emit(tr("STEAM_INVALID_CODE"))
		return
	joining_lobby = lobby_id
	notice.emit(tr("STEAM_JOINING"))
	steam.joinLobby(lobby_id)


func invite_steam() -> void:
	if steam_lobby == 0 or not multiplayer.is_server():
		return
	DisplayServer.clipboard_set(str(steam_lobby))
	notice.emit(tr("STEAM_CODE_COPIED"))
	if steam.isOverlayEnabled():
		steam.activateGameOverlayInviteDialog(steam_lobby)


func _steam_invited(lobby_id: int, _friend_id: int) -> void:
	pending_invitation = str(lobby_id)
	invitation_received.emit(pending_invitation)


func accept_pending_invitation() -> void:
	var code := pending_invitation
	pending_invitation = ""
	if not code.is_empty():
		join_steam(code)


func _steam_lobby_created(result: int, lobby_id: int) -> void:
	if not creating_lobby:
		return
	creating_lobby = false
	if _discard_steam_request:
		_discard_steam_request = false
		if result == 1:
			steam.leaveLobby(lobby_id)
		return
	if result != 1:
		notice.emit(tr("STEAM_CREATE_FAILED") % result)
		return
	var peer: MultiplayerPeer = ClassDB.instantiate("SteamMultiplayerPeer")
	var error: Error = peer.create_host()
	if error != OK:
		steam.leaveLobby(lobby_id)
		notice.emit(tr("STEAM_HOST_FAILED") % error_string(error))
		return
	if not steam.setLobbyData(lobby_id, "host_id", str(steam.getSteamID())) or not steam.setLobbyData(lobby_id, "protocol", WIRE_PROTOCOL) or not steam.setLobbyData(lobby_id, "version", version):
		peer.close()
		steam.leaveLobby(lobby_id)
		notice.emit(tr("STEAM_SETUP_FAILED"))
		return
	steam_lobby = lobby_id
	multiplayer.multiplayer_peer = peer
	NetworkTime.start()
	steam_hosted.emit()
	invite_steam()


func _steam_lobby_joined(lobby_id: int, _permissions: int, _locked: bool, response: int) -> void:
	if joining_lobby != lobby_id:
		return
	joining_lobby = 0
	if _discard_steam_request:
		_discard_steam_request = false
		if response == 1:
			steam.leaveLobby(lobby_id)
		return
	if response != 1:
		notice.emit(tr("STEAM_JOIN_FAILED") % response)
		return
	var host_version: String = steam.getLobbyData(lobby_id, "version")
	if steam.getLobbyData(lobby_id, "protocol") != WIRE_PROTOCOL or host_version != version:
		steam.leaveLobby(lobby_id)
		notice.emit(tr("VERSION_INCOMPATIBLE") if host_version == version else _version_message(host_version))
		return
	var peer: MultiplayerPeer = ClassDB.instantiate("SteamMultiplayerPeer")
	var owner: int = steam.getLobbyOwner(lobby_id)
	if owner == 0 or str(owner) != steam.getLobbyData(lobby_id, "host_id"):
		steam.leaveLobby(lobby_id)
		notice.emit(tr("STEAM_HOST_LEFT"))
		return
	var error: Error = peer.create_client(owner)
	if error != OK:
		steam.leaveLobby(lobby_id)
		notice.emit(tr("STEAM_CONNECT_FAILED") % error_string(error))
		return
	steam_lobby = lobby_id
	multiplayer.multiplayer_peer = peer
	steam_host_id = owner
	steam_connecting.emit()


func _steam_membership_changed(lobby_id: int, changed_id: int, _actor: int, state: int) -> void:
	# Steam can transfer lobby ownership, but the game server belongs to the original host.
	if lobby_id == steam_lobby and changed_id == steam_host_id and state & 30:
		leave(tr("HOST_LEFT"))
