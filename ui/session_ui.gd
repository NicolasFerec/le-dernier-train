extends CanvasLayer

signal close_station_requested
signal start_requested
signal advance_requested

@onready var session_menu: CenterContainer = $SessionMenu
@onready var invitation_menu: CenterContainer = $InvitationMenu
@onready var station_menu: CenterContainer = $StationMenu
@onready var lobby: VBoxContainer = $StationMenu/Lobby
@onready var character_select: OptionButton = $StationMenu/Lobby/Character
@onready var weapon_select: OptionButton = $StationMenu/Lobby/Weapon

var shop_buttons := {}
var _remote_paused := false


func _ready() -> void:
	$StationMenu/Lobby/Start.pressed.connect(func(): start_requested.emit())
	$StationMenu/Lobby/Back.pressed.connect(func(): close_station_requested.emit())
	$Advance.pressed.connect(func(): advance_requested.emit())
	$PauseNoticeTimer.timeout.connect(_refresh_pause_notice)
	character_select.item_selected.connect(func(id): _select(id, weapon_select.selected))
	weapon_select.item_selected.connect(func(id): _select(character_select.selected, id))
	for item in Profile.ITEMS:
		var button := Button.new()
		$StationMenu/Lobby/Shop.add_child(button)
		shop_buttons[item] = button
		button.pressed.connect(func(): _buy(item))
	Profile.changed.connect(_refresh_profile)
	ControlHints.changed.connect(_refresh_controls)
	_refresh_controls()
	_refresh_profile()
	show_phase(0, true, false)
	show_station(-1, true)


func _buy(item: String) -> void:
	var error := Profile.buy(item)
	$StationMenu/Lobby/Notice.text = tr("PURCHASE_SAVED") if error == OK else tr("PURCHASE_FAILED") % error_string(error)


func _select(character: int, weapon: int) -> void:
	if Profile.select_loadout(character, weapon) != OK:
		$StationMenu/Lobby/Notice.text = tr("LOADOUT_SAVE_FAILED")


func _refresh_profile() -> void:
	$StationMenu/Lobby/Points.text = tr("POINTS") % Profile.points
	character_select.clear()
	character_select.add_item(tr("FIGHTER"))
	character_select.add_item(tr("SCOUT"))
	character_select.set_item_disabled(1, not Profile.unlocked.has("character"))
	character_select.select(Profile.character)
	weapon_select.clear()
	var weapons := ["RAPIER", "DAGGERS"] if Profile.character == 1 else ["SWORD", "HAMMER"]
	for weapon in weapons:
		weapon_select.add_item(tr(weapon))
	weapon_select.set_item_disabled(1, not Profile.has_secondary())
	weapon_select.select(Profile.weapon)
	for item in shop_buttons:
		var button: Button = shop_buttons[item]
		var entry: Array = Profile.ITEMS[item]
		var owned: bool = Profile.unlocked.has(item)
		var requires: String = entry[2]
		var locked: bool = requires != "" and not Profile.unlocked.has(requires)
		var name_text := tr(entry[0])
		if locked:
			name_text = tr("REQUIRES") % [name_text, tr(Profile.ITEMS[requires][0])]
		button.text = "%s · %s" % [name_text, tr("OWNED") if owned else tr("PRICE") % entry[1]]
		button.disabled = owned or locked or Profile.points < entry[1]
		button.focus_mode = Control.FOCUS_NONE if button.disabled else Control.FOCUS_ALL
	if station_menu.visible and get_viewport().gui_get_focus_owner() == null:
		_focus_station()


func layout_hud(layout: Transform2D) -> void:
	transform = layout
	var full := layout.affine_inverse() * get_viewport().get_visible_rect()
	for control in [$MenuBackdrop, station_menu, session_menu, invitation_menu]:
		control.position = full.position
		control.size = full.size
	var safe := layout.affine_inverse() * GameSettings.hud_safe_rect()
	var left := safe.position.x + 24
	var right := safe.end.x - 24
	$Status.offset_left = left
	$Status.offset_right = right
	$HUD.position = Vector2(left, safe.end.y - 79)
	$Help.offset_left = left + 284
	$Help.offset_right = right
	$Help.position.y = safe.end.y - 36


func show_phase(room: int, authority: bool, connecting: bool, advance := "") -> void:
	if room > 0 or connecting:
		show_station(-1, authority)
	$HUD.visible = room > 0
	$Status.visible = room > 0
	$Help.visible = room > 0 and not session_menu.visible
	var advance_was_visible: bool = $Advance.visible
	$Advance.visible = advance != "" and authority and not session_menu.visible
	$Advance.text = advance
	if $Advance.visible and not advance_was_visible:
		$Advance.grab_focus()


func show_departure(seconds: int) -> void:
	$Departure.visible = seconds > 0
	if seconds > 0:
		$Departure/Label.text = tr("ROOM_GATE_COUNTDOWN") % seconds


func show_remote_pause(active: bool) -> void:
	if _remote_paused != active:
		_remote_paused = active
		_refresh_pause_notice()


func show_guest_pause_notice() -> void:
	$PauseNoticeTimer.start()
	_refresh_pause_notice()


func clear_guest_pause_notice() -> void:
	$PauseNoticeTimer.stop()
	_refresh_pause_notice()


func _refresh_pause_notice() -> void:
	var guest_notice: bool = not $PauseNoticeTimer.is_stopped()
	$PauseNotice.visible = guest_notice or _remote_paused
	$PauseNotice/Label.text = "PAUSE_HOST_ONLY" if guest_notice else "PAUSE_BY_HOST"


func show_station(station: int, authority: bool, notice := "") -> void:
	var was_visible := station_menu.visible
	var show_invitations := station == TrainLobby.INVITATION_STATION and not session_menu.visible
	if show_invitations:
		if invitation_menu.visible:
			invitation_menu.refresh_session(false, notice)
		else:
			invitation_menu.open(false, notice)
	else:
		invitation_menu.hide()
	station_menu.visible = station >= 0 and not session_menu.visible and not show_invitations
	var menu_visible := station_menu.visible or session_menu.visible or invitation_menu.visible
	$MenuBackdrop.visible = menu_visible
	if GameSettings.in_menu != menu_visible:
		GameSettings.set_menu(menu_visible)
	if not station_menu.visible:
		return
	$StationMenu/Lobby/Title.text = tr(["CHARACTER_STATION", "ARSENAL_STATION", "PASSIVES_STATION", "DEPARTURE_STATION"][station])
	character_select.visible = station == 0
	weapon_select.visible = station == 1
	for item in shop_buttons:
		shop_buttons[item].visible = (
			(station == 0 and item == "character")
			or (station == 1 and item in ["hammer", "daggers"])
			or (station == 2 and item in ["health", "speed"])
		)
	$StationMenu/Lobby/Start.visible = station == TrainLobby.DEPARTURE_STATION and authority
	$StationMenu/Lobby/Waiting.visible = station == TrainLobby.DEPARTURE_STATION and not authority

	if not was_visible:
		_focus_station()


func _refresh_controls() -> void:
	$Help.text = "COMBAT_HELP_GAMEPAD" if ControlHints.using_gamepad else "COMBAT_HELP_KEYBOARD"
	if ControlHints.using_gamepad and get_viewport().gui_get_focus_owner() == null:
		if station_menu.visible:
			_focus_station()
		elif $Advance.visible:
			$Advance.grab_focus()


func _focus_station() -> void:
	for control in [character_select, weapon_select, $StationMenu/Lobby/Start] + shop_buttons.values() + [$StationMenu/Lobby/Back]:
		if control.is_visible_in_tree() and not control.disabled:
			control.grab_focus()
			return


func show_party(player: Node2D, players: Dictionary) -> void:
	$HUD.update_party(player, players)


func show_status(message: String) -> void:
	$Status.text = message
