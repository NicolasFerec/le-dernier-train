extends CanvasLayer

signal steam_host_requested
signal steam_join_requested(code: String)
signal steam_invite_requested
signal language_changed
signal host_requested
signal join_requested(address: String)
signal leave_requested
signal start_requested
signal advance_requested

@onready var address: LineEdit = $Bar/Address
@onready var lobby: VBoxContainer = $Lobby
@onready var character_select: OptionButton = $Lobby/Character
@onready var weapon_select: OptionButton = $Lobby/Weapon

var shop_buttons := {}


func _ready() -> void:
	var preferences := ConfigFile.new()
	preferences.load("user://preferences.cfg")
	var language: String = preferences.get_value("interface", "language", "fr")
	$Language.add_item("Français")
	$Language.add_item("English")
	$Language.select(1 if language == "en" else 0)
	_set_language($Language.selected)
	$Language.item_selected.connect(func(index: int):
		_set_language(index)
		preferences.set_value("interface", "language", TranslationServer.get_locale())
		preferences.save("user://preferences.cfg")
	)
	$Bar/SteamHost.pressed.connect(func(): steam_host_requested.emit())
	$Bar/SteamJoin.pressed.connect(func(): steam_join_requested.emit(address.text.strip_edges()))
	$Bar/SteamInvite.pressed.connect(func(): steam_invite_requested.emit())
	set_steam_invite(false)
	$Bar/Host.pressed.connect(func(): host_requested.emit())
	$Bar/Join.pressed.connect(func(): join_requested.emit(address.text.strip_edges()))
	address.text_submitted.connect(func(value): join_requested.emit(value.strip_edges()))
	$Bar/Leave.pressed.connect(func(): leave_requested.emit())
	$Lobby/Start.pressed.connect(func(): start_requested.emit())
	$Advance.pressed.connect(func(): advance_requested.emit())
	character_select.item_selected.connect(func(id): _select(id, weapon_select.selected))
	weapon_select.item_selected.connect(func(id): _select(character_select.selected, id))
	for item in Profile.ITEMS:
		var button := Button.new()
		button.focus_mode = Control.FOCUS_NONE
		$Lobby/Shop.add_child(button)
		shop_buttons[item] = button
		button.pressed.connect(func(): _buy(item))
	Profile.changed.connect(_refresh_profile)
	_refresh_profile()
	set_session(false)
	show_phase(0, true, false)
	show_station(-1, true)


func _set_language(index: int) -> void:
	TranslationServer.set_locale("en" if index == 1 else "fr")
	$Lobby/Notice.text = ""
	_refresh_profile()
	language_changed.emit()


func _buy(item: String) -> void:
	var error := Profile.buy(item)
	$Lobby/Notice.text = tr("PURCHASE_SAVED") if error == OK else tr("PURCHASE_FAILED") % error_string(error)


func _select(character: int, weapon: int) -> void:
	if Profile.select_loadout(character, weapon) != OK:
		$Lobby/Notice.text = tr("LOADOUT_SAVE_FAILED")


func _refresh_profile() -> void:
	$Lobby/Points.text = tr("POINTS") % Profile.points
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


func set_session(active: bool) -> void:
	set_network_available(not active)
	$Bar/Leave.visible = active


func set_network_available(available: bool) -> void:
	if not available:
		address.release_focus()
	address.visible = available
	$Bar/Host.visible = available
	$Bar/Join.visible = available
	$Bar/SteamHost.visible = available
	$Bar/SteamJoin.visible = available


func set_steam_invite(available: bool) -> void:
	$Bar/SteamInvite.visible = available


func show_phase(room: int, authority: bool, connecting: bool, advance := "") -> void:
	if room > 0 or connecting:
		show_station(-1, authority)
		$Prompt.hide()
	$HUD.visible = room > 0
	$Help.visible = room > 0
	$Advance.visible = advance != "" and authority
	$Advance.text = advance


func show_station(station: int, authority: bool) -> void:
	lobby.visible = station >= 0
	$LobbyBackdrop.visible = lobby.visible
	if not lobby.visible:
		return
	$Lobby/Title.text = tr(["CHARACTER_STATION", "ARSENAL_STATION", "PASSIVES_STATION", "DEPARTURE_STATION"][station])
	character_select.visible = station == 0
	weapon_select.visible = station == 1
	for item in shop_buttons:
		shop_buttons[item].visible = (
			(station == 0 and item == "character")
			or (station == 1 and item in ["hammer", "daggers"])
			or (station == 2 and item in ["health", "speed"])
		)
	$Lobby/Start.visible = station == 3 and authority
	$Lobby/Waiting.visible = station == 3 and not authority


func show_prompt(message: String) -> void:
	$Prompt.show()
	$Prompt.text = message


func show_player(player: Node2D) -> void:
	$HUD.update_player(player)


func show_status(message: String) -> void:
	$Status.text = message
