extends CenterContainer

signal resume_requested
signal local_host_requested
signal local_join_requested

@onready var rows: VBoxContainer = $Panel/Rows

var in_lobby := true
var connecting := false
var message := ""


func _ready() -> void:
	Session.roster_changed.connect(_refresh_players)
	rows.get_node("Resume").pressed.connect(func(): resume_requested.emit())
	rows.get_node("Local/Host").pressed.connect(func(): local_host_requested.emit())
	rows.get_node("Local/Join").pressed.connect(func(): local_join_requested.emit())
	rows.get_node("Leave").pressed.connect(func(): Session.leave())
	rows.get_node("MainMenu").pressed.connect(func():
		Session.leave()
		get_tree().change_scene_to_file("res://ui/main_menu.tscn")
	)
	rows.get_node("Quit").pressed.connect(func():
		Session.leave()
		GameAudio.quit_game()
	)


func open() -> void:
	show()
	_refresh_players()
	rows.get_node("Resume").grab_focus()


func refresh_session(room: int, is_connecting: bool, notice: String) -> void:
	in_lobby = room == 0
	connecting = is_connecting
	message = notice
	if visible:
		_refresh_header()


func _refresh_header() -> void:
	var online := Session.is_online()
	var mode := tr("SOLO")
	if connecting or Session.creating_lobby or Session.joining_lobby != 0:
		mode = tr("CONNECTING")
	elif online:
		mode = tr("SESSION_ONLINE") if Session.steam_lobby != 0 else tr("SESSION_LOCAL")
	rows.get_node("Mode").text = mode
	rows.get_node("Notice").text = message
	rows.get_node("Notice").visible = not message.is_empty()
	var can_host := in_lobby and not online and not Session.creating_lobby and Session.joining_lobby == 0
	rows.get_node("Local").visible = Session.local_dev_enabled and can_host
	rows.get_node("Leave").visible = online


func _refresh_players() -> void:
	if not visible:
		return
	_refresh_header()
	var list: VBoxContainer = rows.get_node("Players")
	for row in list.get_children():
		list.remove_child(row)
		row.queue_free()
	var entries := Session.roster
	if entries.is_empty() and not connecting:
		var local_id := multiplayer.get_unique_id()
		entries = [[local_id, Session.player_name(local_id, 1), -1]]
	for entry in entries:
		var row := HBoxContainer.new()
		var name_label := Label.new()
		name_label.auto_translate_mode = Node.AUTO_TRANSLATE_MODE_DISABLED
		var player_name: String = entry[1]
		if entry[0] == multiplayer.get_unique_id():
			player_name = tr("SESSION_YOU") % player_name
		if Session.is_online() and entry[0] == 1:
			player_name = tr("SESSION_HOST_NAME") % player_name
		name_label.text = player_name
		name_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var ping := Label.new()
		ping.text = tr("SESSION_PING") % entry[2] if entry[2] >= 0 else "—"
		row.add_child(name_label)
		row.add_child(ping)
		list.add_child(row)
