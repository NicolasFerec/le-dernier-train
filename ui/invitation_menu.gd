extends CenterContainer

signal close_requested

@onready var invite: Button = $Panel/Rows/Invite
@onready var code: LineEdit = $Panel/Rows/JoinRow/Code
@onready var join: Button = $Panel/Rows/JoinRow/Join
@onready var unavailable: Label = $Panel/Rows/Unavailable
@onready var notice_label: Label = $Panel/Rows/Notice
@onready var back: Button = $Panel/Rows/Back


func _ready() -> void:
	invite.pressed.connect(_invite_friends)
	join.pressed.connect(_join_by_code)
	code.text_submitted.connect(func(_text: String): _join_by_code())
	back.pressed.connect(func(): close_requested.emit())


func open(connecting: bool, notice: String) -> void:
	show()
	refresh_session(connecting, notice)
	if invite.disabled:
		back.grab_focus()
	else:
		invite.grab_focus()


func refresh_session(connecting: bool, notice: String) -> void:
	if not visible:
		return
	var busy := connecting or Session.creating_lobby or Session.joining_lobby != 0
	var online := Session.is_online()
	var steam_host := Session.steam_lobby != 0 and multiplayer.is_server()
	var invite_reason := ""
	var join_reason := ""
	if Session.steam == null:
		invite_reason = tr(Session.steam_status)
		join_reason = invite_reason
	elif busy:
		invite_reason = tr("CONNECTING")
		join_reason = invite_reason
	elif online:
		join_reason = tr("STEAM_LEAVE_FIRST")
		if not steam_host:
			invite_reason = tr("STEAM_INVITE_HOST_ONLY") if Session.steam_lobby != 0 else join_reason
	invite.disabled = not invite_reason.is_empty()
	invite.tooltip_text = invite_reason
	join.disabled = not join_reason.is_empty()
	join.tooltip_text = join_reason
	code.editable = not join.disabled
	code.focus_mode = Control.FOCUS_NONE if join.disabled else Control.FOCUS_ALL
	code.tooltip_text = join_reason

	var reason := invite_reason if invite.disabled and join.disabled and not busy else ""
	unavailable.text = reason
	unavailable.visible = not reason.is_empty()
	notice_label.text = notice
	notice_label.visible = not notice.is_empty() and notice != reason
	var focus := get_viewport().gui_get_focus_owner()
	if focus == null or (focus == invite and invite.disabled) or (join.disabled and (focus == join or focus == code)):
		back.grab_focus()


func _invite_friends() -> void:
	if invite.disabled:
		return
	if Session.steam_lobby == 0:
		Session.host_steam()
	else:
		Session.invite_steam()


func _join_by_code() -> void:
	if not join.disabled:
		Session.join_steam(code.text.strip_edges())
