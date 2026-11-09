extends Control

const RELEASE_ROOT := "https://github.com/NicolasFerec/godot-roguelite/releases/latest/download/"
const GAME_DATA := "user://../Arène coop"
const INSTALL_ROOT := "user://versions"

var platform := "windows-x86_64" if OS.get_name() == "Windows" else "linux-x86_64"
var executable := "Arene-coop.exe" if OS.get_name() == "Windows" else "Arene-coop.x86_64"
var version_pattern := RegEx.create_from_string("^(0|[1-9][0-9]{0,5})\\.(0|[1-9][0-9]{0,5})\\.(0|[1-9][0-9]{0,5})$")
var request := HTTPRequest.new()
var archive_path := ""


func _ready() -> void:
	var preferences := ConfigFile.new()
	preferences.load(GAME_DATA + "/preferences.cfg")
	TranslationServer.set_locale(preferences.get_value("interface", "language", "fr"))
	add_child(request)
	request.use_threads = true
	$Retry.pressed.connect(_start)
	_start()


func _process(_delta: float) -> void:
	if not archive_path.is_empty():
		var total := request.get_body_size()
		if total > 0:
			$Progress.value = 100.0 * request.get_downloaded_bytes() / total


func _start() -> void:
	$Retry.hide()
	$Progress.value = 0
	$Status.text = tr("UPDATER_CHECK")
	var installed := _installed_versions()
	if not await _update(installed):
		$Status.text = tr("UPDATER_FIRST_FAILED" if installed.is_empty() else "UPDATER_OFFLINE")
		if installed.is_empty():
			$Retry.show()
			return
		await get_tree().create_timer(1.4).timeout
	_launch(_installed_versions())


func _update(installed: Array[String]) -> bool:
	request.timeout = 7
	request.body_size_limit = 16384
	if request.request(RELEASE_ROOT + "update.json") != OK:
		return false
	var response: Array = await request.request_completed
	if response[0] != HTTPRequest.RESULT_SUCCESS or response[1] != 200:
		return false
	var manifest = JSON.parse_string(response[3].get_string_from_utf8())
	if not manifest is Dictionary:
		return false
	var version = manifest.get("version")
	if not version is String or version_pattern.search(version) == null:
		return false
	if not installed.is_empty() and not _newer(version, installed[0]):
		return true
	var checksums = manifest.get("sha256")
	if not checksums is Dictionary:
		return false
	var checksum = checksums.get(platform)
	if not checksum is String or checksum.length() != 64 or not checksum.is_valid_hex_number(false):
		return false

	# A separate staging directory keeps interrupted downloads away from the installed game.
	var staging := INSTALL_ROOT.path_join(version + "-" + str(OS.get_process_id()) + ".tmp")
	if DirAccess.make_dir_recursive_absolute(staging) != OK:
		return false
	var downloaded := await _download(version, staging, checksum)
	if not downloaded:
		return false
	$Status.text = tr("UPDATER_INSTALL") % version
	await RenderingServer.frame_post_draw
	var installed_ok := _unpack(staging)
	DirAccess.remove_absolute(staging.path_join("game.zip"))
	return installed_ok and DirAccess.rename_absolute(staging, INSTALL_ROOT.path_join(version)) == OK


func _download(version: String, staging: String, checksum: String) -> bool:
	$Status.text = tr("UPDATER_DOWNLOAD") % version
	archive_path = staging.path_join("game.zip")
	request.timeout = 120
	request.body_size_limit = 200 * 1024 * 1024
	request.download_file = archive_path
	var started := request.request("https://github.com/NicolasFerec/godot-roguelite/releases/download/v" + version + "/Arene-coop-" + platform + ".zip")
	var ok := false
	if started == OK:
		var response: Array = await request.request_completed
		ok = response[0] == HTTPRequest.RESULT_SUCCESS and response[1] == 200
	ok = ok and FileAccess.get_sha256(archive_path) == checksum
	if not ok:
		DirAccess.remove_absolute(archive_path)
	archive_path = ""
	request.download_file = ""
	return ok


func _unpack(staging: String) -> bool:
	var allowed := [executable, "GODOT-LICENSE.txt", "GODOT-COPYRIGHT.txt", "GODOTSTEAM-LICENSE.md", "KENNEY-LICENSE.txt"]
	if OS.get_name() == "Windows":
		allowed.append_array(["steam_api64.dll", "libgodotsteam.windows.template_release.x86_64.dll"])
	else:
		allowed.append_array(["libsteam_api.so", "libgodotsteam.linux.template_release.x86_64.so"])
	var zip := ZIPReader.new()
	if zip.open(staging.path_join("game.zip")) != OK or zip.get_files().size() != allowed.size():
		return false
	for filename in allowed:
		if not zip.file_exists(filename):
			return false
		var output := FileAccess.open(staging.path_join(filename), FileAccess.WRITE)
		if output == null or not output.store_buffer(zip.read_file(filename)):
			return false
	zip.close()
	if OS.get_name() != "Windows":
		return FileAccess.set_unix_permissions(staging.path_join(executable), 493) == OK
	return true


func _installed_versions() -> Array[String]:
	var versions: Array[String] = []
	for folder in DirAccess.get_directories_at(INSTALL_ROOT):
		if version_pattern.search(folder) != null and FileAccess.file_exists(INSTALL_ROOT.path_join(folder).path_join(executable)):
			versions.append(folder)
	versions.sort_custom(_newer)
	return versions


func _newer(a: String, b: String) -> bool:
	var left := a.split(".")
	var right := b.split(".")
	for index in range(3):
		if left[index] != right[index]:
			return left[index].to_int() > right[index].to_int()
	return false


func _launch(versions: Array[String]) -> void:
	$Status.text = tr("UPDATER_START")
	for version in versions:
		var path := ProjectSettings.globalize_path(INSTALL_ROOT.path_join(version).path_join(executable))
		if OS.create_process(path, OS.get_cmdline_user_args()) != -1:
			get_tree().quit()
			return
	$Status.text = tr("UPDATER_LAUNCH_FAILED")
	$Retry.show()
