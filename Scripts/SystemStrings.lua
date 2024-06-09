--[[
NOTE: It's possible to expand string entry tables with extra language variations
corresponding to languages in the table at the end of this file.
--]]

local strings =
{
	actions_accelerate = { "Accelerate" },
	actions_action = { "Action" },
	actions_backward = { "Backward" },
	actions_brake = { "Brake/Dismount" },
	actions_crouch = { "Crouch" },
	actions_deselect = { "Deselect" },
	actions_draw = { "Draw" },
	actions_fire = { "Fire" },
	actions_flare = { "Flare" },
	actions_forward = { "Forward" },
	actions_inventory = { "Inventory" },
	actions_jump = { "Jump" },
	actions_large_medipack = { "Large Medipack" },
	actions_left = { "Left" },
	actions_load = { "Load" },
	actions_look = { "Look" },
	actions_next_weapon = { "Next Weapon" },
	actions_pause = { "Pause" },
	actions_previous_weapon = { "Previous Weapon" },
	actions_reverse = { "Reverse" },
	actions_right = { "Right" },
	actions_roll = { "Roll" },
	actions_save = { "Save" },
	actions_select = { "Select" },
	actions_slow = { "Slower" },
	actions_small_medipack = { "Small Medipack" },
	actions_speed = { "Faster" },
	actions_sprint = { "Sprint" },
	actions_step_left = { "Step Left" },
	actions_step_right = { "Step Right" },
	actions_walk = { "Walk" },
	actions_weapon_1 = { "Weapon 1" },
	actions_weapon_10 = { "Weapon 10" },
	actions_weapon_2 = { "Weapon 2" },
	actions_weapon_3 = { "Weapon 3" },
	actions_weapon_4 = { "Weapon 4" },
	actions_weapon_5 = { "Weapon 5" },
	actions_weapon_6 = { "Weapon 6" },
	actions_weapon_7 = { "Weapon 7" },
	actions_weapon_8 = { "Weapon 8" },
	actions_weapon_9 = { "Weapon 9" },
	all = { "All" },
	ambient_occlusion = { "Ambient Occlusion" },
	ammo_used = { "Ammo Used" },
	antialiasing = { "Antialiasing" },
	apply = { "Apply" },
	auto_monkey_swing_jump = { "Auto Monkey Jump" },
	auto_targeting = { "Auto Targeting" },
	back = { "Back" },
	cancel = { "Cancel" },
	caustics = { "Underwater Caustics" },
	choose_ammo = { "Choose Ammo" },
	choose_weapon = { "Choose Weapon" },
	climb_toggle = { "Climb Toggle" },
	close = { "Close" },
	combine = { "Combine" },
	combine_with = { "Combine With" },
	control_mode = { "Control Mode" },
	control_mode_classic = { "Classic" },
	control_mode_enhanced = { "Enhanced" },
	control_mode_modern = { "Modern" },
	crouch_toggle = { "Crouch Toggle" },
	display_adapter = { "Display Adapter" },
	distance_travelled = { "Distance Travelled" },
	empty = { "Empty" },
	enable_sound = { "Enable Sounds" },
	equip = { "Equip" },
	examine = { "Examine" },
	general_actions = { "General Actions" },
	high = { "High" },
	invert_camera_x_axis = { "Invert Camera X Axis" },
	invert_camera_y_axis = { "Invert Camera Y Axis" },
	level_secrets_found = { "Secrets Found in Level" },
	load_game = { "Load Game" },
	low = { "Low" },
	medium = { "Medium" },
	menu_actions = { "Menu Actions" },
	mouse_sensitivity = { "Mouse Sensitivity" },
	music_volume = { "Music Volume" },
	new_game = { "New Game" },
	none = { "None" },
	off = { "Off" },
	ok = { "OK" },
	on = { "On" },
	opposite_action_roll = { "Opposite Action Roll" },
	options_controls = { "Controls" },
	options_display = { "Display" },
	options_exit_game = { "Exit Game" },
	options_exit_to_title = { "Exit to Title" },
	options_gameplay = { "Gameplay" },
	options_key_bindings = { "Key Bindings" },
	options_options = { "Options" },
	options_pause = { "Pause" },
	options_sound = { "Sound" },
	options_statistics = { "Statistics" },
	output_settings = { "Output Settings" },
	player = { "Player" },
	quick_actions = { "Quick Actions" },
	render_options = { "Render Options" },
	reset_to_defaults = { "Reset to Defaults" },
	reverb = { "Reverb" },
	rumble = { "Rumble" },
	save_game = { "Save Game" },
	savegame_timestamp = { "%02d Days %02d:%02d:%02d" },
	screen_resolution = { "Screen Resolution" },
	select_level = { "Select Level" },
	separate = { "Separate" },
	sfx_volume = { "SFX Volume" },
	shadows = { "Shadows" },
	sound = { "Sound" },
	subtitles = { "Subtitles" },
	swim_control_mode = { "Swim Control Mode" },
	swim_control_mode_omnidirectional = { "Omnidirectional" },
	swim_control_mode_planar = { "Planar" },
	tank_camera_control = { "Tank Camera Control" },
	target_highlighter = { "Target Highlighter" },
	time_taken = { "Time Taken" },
	total_secrets_found = { "Secrets Found Total" },
	use = { "Use" },
	used_medipacks = { "Medipacks Used" },
	vehicle_actions = { "Vehicle Actions" },
	view = { "View" },
	volumetric_fog = { "Volumetric Fog" },
	waiting_for_input = { "Waiting For Input..." },
	walk_toggle = { "Walk Toggle" },
	window_title = { "TombEngine" },
	window_mode = { "Window Mode" },
	window_mode_fullscreen = { "Fullscreen" },
	window_mode_windowed = { "Windowed" },
	unlimited = { "Unlimited %s" }
}

TEN.Flow.SetStrings(strings)

local languages =
{
	"English",
	"Italian",
	"German",
	"French",
	"Dutch",
	"Spanish",
	"Japanese",
	"Russian"
}

TEN.Flow.SetLanguageNames(languages)
