extends Node

signal changed

const ITEMS := {
	"character": ["SCOUT", 2, ""],
	"hammer": ["UNLOCK_HAMMER", 2, ""],
	"daggers": ["UNLOCK_DAGGERS", 2, "character"],
	"health": ["HEALTH_UPGRADE", 1, ""],
	"speed": ["SPEED_UPGRADE", 2, "health"],
}

var points := 0
var unlocked: Array[String] = []
var character := 0
var weapon := 0
var path := "user://profile.cfg"


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--profile="):
			var label := argument.trim_prefix("--profile=")
			if label.is_valid_identifier():
				path = "user://profile_%s.cfg" % label
	var file := ConfigFile.new()
	var source := path if FileAccess.file_exists(path) else "user://../Arène coop".path_join(path.get_file())
	if file.load(source) == OK:
		points = maxi(0, int(file.get_value("profile", "points", 0)))
		for item in ITEMS:
			if file.get_value("unlocks", item, false) == true:
				unlocked.append(item)
		character = clampi(int(file.get_value("profile", "character", 0)), 0, 1) if unlocked.has("character") else 0
		weapon = clampi(int(file.get_value("profile", "weapon", 0)), 0, 1) if has_secondary() else 0


func buy(item: String) -> Error:
	var cost: int = ITEMS[item][1]
	var prerequisite: String = ITEMS[item][2]
	if unlocked.has(item) or points < cost or (prerequisite != "" and not unlocked.has(prerequisite)):
		return ERR_UNAVAILABLE
	unlocked.append(item)
	points -= cost
	var error := _save()
	if error != OK:
		unlocked.erase(item)
		points += cost
	changed.emit()
	return error


func select_loadout(next_character: int, next_weapon: int) -> Error:
	var previous_character := character
	var previous_weapon := weapon
	character = next_character if unlocked.has("character") else 0
	weapon = next_weapon if has_secondary() else 0
	var error := _save()
	if error != OK:
		character = previous_character
		weapon = previous_weapon
	changed.emit()
	return error


func reward(amount: int) -> Error:
	points += amount
	var error := _save()
	if error != OK:
		points -= amount
	changed.emit()
	return error


func has_secondary() -> bool:
	return unlocked.has("daggers" if character == 1 else "hammer")


func loadout() -> Array[int]:
	return [character, weapon, int(unlocked.has("health")), int(unlocked.has("speed"))]


func _save() -> Error:
	var file := ConfigFile.new()
	file.set_value("profile", "points", points)
	file.set_value("profile", "character", character)
	file.set_value("profile", "weapon", weapon)
	for item in unlocked:
		file.set_value("unlocks", item, true)
	var error := file.save(path + ".tmp")
	if error != OK:
		return error
	return DirAccess.rename_absolute(path + ".tmp", path)
