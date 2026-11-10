extends Node

signal ended(message: String)
signal steam_hosted
signal steam_connecting
signal notice(message: String)

const PORT := 7000
const MAX_PLAYERS := 4
const STEAM_PROTOCOL := "godot-roguelite-1"

var version: String = ProjectSettings.get_setting("application/config/version")
@onready var scene_network: SceneMultiplayer = multiplayer

var steam: Object
var steam_lobby := 0
var steam_host_id := 0
var creating_lobby := false
var joining_lobby := 0
var steam_status := "STEAM_REQUIRED"



func _ready() -> void:
	scene_network.auth_timeout = 5.0
	scene_network.auth_callback = _check_peer_version
	scene_network.peer_authenticating.connect(_send_version)
	scene_network.peer_authentication_failed.connect(_version_check_failed)
	multiplayer.connection_failed.connect(func(): leave(tr("CONNECTION_FAILED")))
	multiplayer.server_disconnected.connect(func(): leave(tr("HOST_LEFT")))
	_initialize_steam()


func _send_version(peer_id: int) -> void:
	scene_network.send_auth(peer_id, version.to_utf8_buffer())


func _check_peer_version(peer_id: int, data: PackedByteArray) -> void:
	var remote_version := data.get_string_from_utf8() if data.size() <= 32 else ""
	if remote_version == version:
		scene_network.complete_auth(peer_id)
	elif not multiplayer.is_server():
		leave.call_deferred(_version_message(remote_version))


func _version_check_failed(peer_id: int) -> void:
	if peer_id == 1 and is_online():
		leave.call_deferred(_version_message(""))


func _version_message(host_version: String) -> String:
	var parts := host_version.split(".")
	if parts.size() != 3 or not parts[0].is_valid_int() or not parts[1].is_valid_int() or not parts[2].is_valid_int():
		return tr("VERSION_UNKNOWN")
	return tr("VERSION_MISMATCH") % [version, host_version]


func host() -> Error:
	if _busy():
		return ERR_BUSY
	var peer := ENetMultiplayerPeer.new()
	var error := peer.create_server(PORT, MAX_PLAYERS - 1)
	if error == OK:
		multiplayer.multiplayer_peer = peer
	return error


func join(address: String) -> Error:
	if _busy():
		return ERR_BUSY
	var peer := ENetMultiplayerPeer.new()
	var error := peer.create_client(address, PORT)
	if error == OK:
		multiplayer.multiplayer_peer = peer
	return error


func leave(message := "") -> void:
	if is_online():
		multiplayer.multiplayer_peer.close()
		multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()
	if steam_lobby != 0:
		steam.leaveLobby(steam_lobby)
		steam_lobby = 0
		steam_host_id = 0
	ended.emit(message)


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
		join_steam.call_deferred(args[invite + 1])


func _process(_delta: float) -> void:
	if steam != null:
		steam.run_callbacks()


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
	join_steam(str(lobby_id))


func _steam_lobby_created(result: int, lobby_id: int) -> void:
	if not creating_lobby:
		return
	creating_lobby = false
	if result != 1:
		notice.emit(tr("STEAM_CREATE_FAILED") % result)
		return
	var peer: MultiplayerPeer = ClassDB.instantiate("SteamMultiplayerPeer")
	var error: Error = peer.create_host()
	if error != OK:
		steam.leaveLobby(lobby_id)
		notice.emit(tr("STEAM_HOST_FAILED") % error_string(error))
		return
	if not steam.setLobbyData(lobby_id, "host_id", str(steam.getSteamID())) or not steam.setLobbyData(lobby_id, "protocol", STEAM_PROTOCOL) or not steam.setLobbyData(lobby_id, "version", version):
		peer.close()
		steam.leaveLobby(lobby_id)
		notice.emit(tr("STEAM_SETUP_FAILED"))
		return
	steam_lobby = lobby_id
	multiplayer.multiplayer_peer = peer
	steam_hosted.emit()


func _steam_lobby_joined(lobby_id: int, _permissions: int, _locked: bool, response: int) -> void:
	if joining_lobby != lobby_id:
		return
	joining_lobby = 0
	if response != 1:
		notice.emit(tr("STEAM_JOIN_FAILED") % response)
		return
	var host_version: String = steam.getLobbyData(lobby_id, "version")
	if steam.getLobbyData(lobby_id, "protocol") != STEAM_PROTOCOL or host_version != version:
		steam.leaveLobby(lobby_id)
		notice.emit(_version_message(host_version))
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
