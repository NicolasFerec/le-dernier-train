extends CenterContainer

signal return_requested

const CARD_STYLE := preload("res://ui/run_summary_card.tres")

@onready var rows: VBoxContainer = $Panel/Rows
@onready var cards: GridContainer = $Panel/Rows/Cards


func _ready() -> void:
	$Panel/Rows/Return.pressed.connect(func(): return_requested.emit())


func show_results(victory: bool, cleared: int, total: int, stats: Dictionary, can_return: bool) -> void:
	for child in cards.get_children():
		cards.remove_child(child)
		child.queue_free()

	$Panel/Rows/Outcome.text = "RUN_SUMMARY_VICTORY" if victory else "PARTY_DOWN"
	$Panel/Rows/Rooms.text = tr("RUN_SUMMARY_ROOMS") % [cleared, total]
	var ids := stats.keys()
	ids.sort_custom(func(a: int, b: int) -> bool: return stats[a].slot < stats[b].slot)
	cards.columns = 1 if ids.size() == 1 else 2
	for id in ids:
		_add_card(id, stats[id], ids.size() == 1)
	$Panel/Rows/Return.visible = can_return
	$Panel/Rows/Waiting.visible = not can_return
	show()
	if can_return:
		$Panel/Rows/Return.grab_focus()


func _add_card(id: int, stats: Dictionary, solo: bool) -> void:
	var card := PanelContainer.new()
	card.custom_minimum_size.x = 390 if solo else 302
	card.add_theme_stylebox_override("panel", CARD_STYLE)
	cards.add_child(card)
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 8 if solo else 3)
	card.add_child(content)

	var title := Label.new()
	var name_text: String = stats.name
	if name_text.is_empty():
		name_text = tr("SESSION_PLAYER_FALLBACK") % (stats.slot + 1)
	if not solo and id == multiplayer.get_unique_id():
		name_text = tr("SESSION_YOU") % name_text
	title.auto_translate_mode = Node.AUTO_TRANSLATE_MODE_DISABLED
	title.text = name_text
	title.tooltip_text = name_text
	title.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	title.add_theme_font_size_override("font_size", 25 if solo else 20)
	title.add_theme_color_override("font_color", Color("#f1c995"))
	content.add_child(title)

	var metrics := GridContainer.new()
	metrics.columns = 2
	metrics.add_theme_constant_override("h_separation", 12)
	content.add_child(metrics)
	for metric in [["RUN_SUMMARY_KILLS", stats.kills], ["RUN_SUMMARY_DEALT", stats.dealt], ["RUN_SUMMARY_TAKEN", stats.taken]]:
		var label := Label.new()
		label.text = metric[0]
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var value := Label.new()
		value.text = str(metric[1])
		value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		label.add_theme_font_size_override("font_size", 19 if solo else 16)
		value.add_theme_font_size_override("font_size", 21 if solo else 17)
		metrics.add_child(label)
		metrics.add_child(value)
