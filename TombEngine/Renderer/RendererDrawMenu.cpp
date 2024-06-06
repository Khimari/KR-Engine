#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/animation.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Internal/TEN/Flow//Level/FlowLevel.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/trutils.h"
#include "Specific/winmain.h"

using namespace TEN::Config;
using namespace TEN::Gui;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Math;

extern TEN::Renderer::RendererHudBar* g_SFXVolumeBar;
extern TEN::Renderer::RendererHudBar* g_MusicVolumeBar;

namespace TEN::Renderer
{
	// Horizontal alignment constants
	constexpr auto MenuLeftSideEntry = 200;
	constexpr auto MenuCenterEntry = 400;
	constexpr auto MenuRightSideEntry = 500;

	constexpr auto MenuLoadNumberLeftSide = 80;
	constexpr auto MenuLoadNameLeftSide   = 150;

	// Vertical spacing templates
	constexpr auto MenuVerticalLineSpacing = 30;
	constexpr auto MenuVerticalNarrowLineSpacing = 24;
	constexpr auto MenuVerticalBlockSpacing = 50;
	
	// Vertical menu positioning templates
	constexpr auto MenuVerticalControlsSettings = 100;
	constexpr auto MenuVerticalKeyBindingsSettings = 30;
	constexpr auto MenuVerticalGameplaySettings = 100;
	constexpr auto MenuVerticalDisplaySettings = 100;
	constexpr auto MenuVerticalSoundSettings = 220;
	constexpr auto MenuVerticalBottomCenter = 400;
	constexpr auto MenuVerticalStatisticsTitle = 150;
	constexpr auto MenuVerticalOptionsTitle = 350;
	constexpr auto MenuVerticalPause = 220;
	constexpr auto MenuVerticalOptionsPause = 275;

	constexpr auto MenuVerticalBottomOptions = (MenuVerticalNarrowLineSpacing * 15) + (MenuVerticalBlockSpacing * 2.5f);

	// Heading logo positioning
	constexpr auto LogoTop = 50;
	constexpr auto LogoWidth = 300;
	constexpr auto LogoHeight = 150;

	// Used with distance travelled
	constexpr auto UnitsToMeters = 419;

	// Helper functions to jump caret to new line
	inline void GetNextLinePosition(int* value) { *value += MenuVerticalLineSpacing; }
	inline void GetNextNarrowLinePosition(int* value) { *value += MenuVerticalNarrowLineSpacing; }
	inline void GetNextBlockPosition(int* value) { *value += MenuVerticalBlockSpacing; }

	// Helper functions to construct string flags
	inline int SF(bool selected = false) { return (int)PrintStringFlags::Outline | (selected ? (int)PrintStringFlags::Blink : 0); }
	inline int SF_Center(bool selected = false) { return (int)PrintStringFlags::Outline | (int)PrintStringFlags::Center | (selected ? (int)PrintStringFlags::Blink : 0); }
	
	// Helper functions to get specific generic strings
	inline const char* Str_Enabled(bool enabled = false) { return g_GameFlow->GetString(enabled ? STRING_ENABLED : STRING_DISABLED); }
	inline const char* Str_LoadSave(bool save = false) { return g_GameFlow->GetString(save ? STRING_SAVE_GAME : STRING_LOAD_GAME); }

	// These bars are only used in menus.
	TEN::Renderer::RendererHudBar* g_MusicVolumeBar = nullptr;
	TEN::Renderer::RendererHudBar* g_SFXVolumeBar	= nullptr;

	void Renderer::InitializeMenuBars(int y)
	{
		static const auto soundSettingColors = std::array<Vector4, RendererHudBar::COLOR_COUNT>
		{
			// Top
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),

			// Center
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),

			// Bottom
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),
			Vector4(0.18f, 0.3f, 0.72f, 1.0f)
		};

		int shift = MenuVerticalLineSpacing / 2;

		g_MusicVolumeBar = new RendererHudBar(_device.Get(), Vector2(MenuRightSideEntry, y + shift), RendererHudBar::SIZE_DEFAULT, 1, soundSettingColors);
		GetNextLinePosition(&y);
		g_SFXVolumeBar = new RendererHudBar(_device.Get(), Vector2(MenuRightSideEntry, y + shift), RendererHudBar::SIZE_DEFAULT, 1, soundSettingColors);
	}

	void Renderer::RenderOptionsMenu(Menu menu, int initialY)
	{
		constexpr auto	  RIGHT_ARROW_X_OFFSET = DISPLAY_SPACE_RES.x - MenuLeftSideEntry;
		static const auto LEFT_ARROW_STRING	   = std::string("<");
		static const auto RIGHT_ARROW_STRING   = std::string(">");

		int y = 0;
		auto titleOption = g_Gui.GetSelectedOption();

		char stringBuffer[32] = {};
		auto screenResolution = g_Configuration.SupportedScreenResolutions[g_Gui.GetCurrentSettings().SelectedScreenResolution];
		sprintf(stringBuffer, "%d x %d", screenResolution.x, screenResolution.y);

		auto* shadowMode = g_Gui.GetCurrentSettings().Configuration.ShadowType != ShadowMode::None ?
			(g_Gui.GetCurrentSettings().Configuration.ShadowType == ShadowMode::Lara ? STRING_SHADOWS_PLAYER : STRING_SHADOWS_ALL) : STRING_SHADOWS_NONE;

		auto antialiasingModeIdString = std::string();
		switch (g_Gui.GetCurrentSettings().Configuration.AntialiasingMode)
		{
		default:
		case AntialiasingMode::None:
			antialiasingModeIdString = STRING_ANTIALIASING_NONE;
			break;

		case AntialiasingMode::Low:
			antialiasingModeIdString = STRING_ANTIALIASING_LOW;
			break;

		case AntialiasingMode::Medium:
			antialiasingModeIdString = STRING_ANTIALIASING_MEDIUM;
			break;

		case AntialiasingMode::High:
			antialiasingModeIdString = STRING_ANTIALIASING_HIGH;
			break;
		}

		auto controlModeIdString = std::string();
		switch (g_Gui.GetCurrentSettings().Configuration.ControlMode)
		{
		default:
		case ControlMode::Classic:
			controlModeIdString = STRING_CONTROL_MODE_CLASSIC;
			break;

		case ControlMode::Enhanced:
			controlModeIdString = STRING_CONTROL_MODE_ENHANCED;
			break;

		case ControlMode::Modern:
			controlModeIdString = STRING_CONTROL_MODE_MODERN;
			break;
		}

		auto swimControlModeIdString = std::string();
		switch (g_Gui.GetCurrentSettings().Configuration.SwimControlMode)
		{
		default:
		case SwimControlMode::Omnidirectional:
			swimControlModeIdString = STRING_SWIM_CONTROL_MODE_OMNI;
			break;

		case SwimControlMode::Planar:
			swimControlModeIdString = STRING_SWIM_CONTROL_MODE_PLANAR;
			break;
		}

		switch (menu)
		{
		case Menu::Options:
			// Set up parameters.
			y = initialY;

			// Heading
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_OPTIONS), PRINTSTRING_COLOR_ORANGE, SF_Center());
			GetNextBlockPosition(&y);

			// Controls
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_CONTROLS), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == 0));
			GetNextLinePosition(&y);

			// Gameplay
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_GAMEPLAY), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == 1));
			GetNextLinePosition(&y);
			
			// Display
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_DISPLAY), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == 2));
			GetNextLinePosition(&y);

			// Sound
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_SOUND), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == 3));
			
			break;

		case Menu::Controls:
			// Set up parameters.
			y = MenuVerticalControlsSettings;

			// Heading
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_CONTROLS), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			// Key Bindings
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_KEY_BINDINGS), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == 0));
			GetNextBlockPosition(&y);

			// Tank camera control
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_TANK_CAMERA_CONTROL), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 1));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableTankCameraControl), PRINTSTRING_COLOR_WHITE, SF(titleOption == 1));
			GetNextLinePosition(&y);

			// Invert camera X axis
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_INVERT_CAMERA_X_AXIS), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 2));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.InvertCameraXAxis), PRINTSTRING_COLOR_WHITE, SF(titleOption == 2));
			GetNextLinePosition(&y);

			// Invert camera Y axis
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_INVERT_CAMERA_Y_AXIS), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 3));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.InvertCameraYAxis), PRINTSTRING_COLOR_WHITE, SF(titleOption == 3));
			GetNextLinePosition(&y);

			// Rumble
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_RUMBLE), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 4));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableRumble), PRINTSTRING_COLOR_WHITE, SF(titleOption == 4));
			GetNextBlockPosition(&y);

			// Mouse sensitivity
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_MOUSE_SENSITIVITY), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 5));
			AddString(MenuRightSideEntry, y, std::to_string(g_Gui.GetCurrentSettings().Configuration.MouseSensitivity).c_str(), PRINTSTRING_COLOR_WHITE, SF(titleOption == 5));

			y = MenuVerticalBottomOptions;

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 6));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 7));
			break;

		case Menu::Gameplay:
			// Set up parameters.
			y = MenuVerticalGameplaySettings;

			// Heading
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_GAMEPLAY), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			// Control mode
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_CONTROL_MODE), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 0));
			AddString(MenuRightSideEntry, y, g_GameFlow->GetString(controlModeIdString.c_str()), PRINTSTRING_COLOR_WHITE, SF(titleOption == 0));
			GetNextLinePosition(&y);

			// Swim control mode
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SWIM_CONTROL_MODE), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 1));
			AddString(MenuRightSideEntry, y, g_GameFlow->GetString(swimControlModeIdString.c_str()), PRINTSTRING_COLOR_WHITE, SF(titleOption == 1));
			GetNextLinePosition(&y);

			// Walk toggle
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_WALK_TOGGLE), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 2));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableWalkToggle), PRINTSTRING_COLOR_WHITE, SF(titleOption == 2));
			GetNextLinePosition(&y);

			// Crouch toggle
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_CROUCH_TOGGLE), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 3));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableCrouchToggle), PRINTSTRING_COLOR_WHITE, SF(titleOption == 3));
			GetNextLinePosition(&y);

			// Auto grab
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AUTO_CLIMB), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 4));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableAutoClimb), PRINTSTRING_COLOR_WHITE, SF(titleOption == 4));
			GetNextLinePosition(&y);

			// Auto monkey swing jump
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AUTO_MONKEY_SWING_JUMP), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 5));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableAutoMonkeySwingJump), PRINTSTRING_COLOR_WHITE, SF(titleOption == 5));
			GetNextLinePosition(&y);

			// Auto targeting
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AUTO_TARGETING), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 6));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableAutoTargeting), PRINTSTRING_COLOR_WHITE, SF(titleOption == 6));
			GetNextLinePosition(&y);

			// Opposite action roll
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_OPPOSITE_ACTION_ROLL), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 7));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableOppositeActionRoll), PRINTSTRING_COLOR_WHITE, SF(titleOption == 7));
			GetNextBlockPosition(&y);

			// Target highlighter
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_TARGET_HIGHLIGHTER), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 8));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableTargetHighlighter), PRINTSTRING_COLOR_WHITE, SF(titleOption == 8));

			y = MenuVerticalBottomOptions;

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 9));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 10));
			break;

		case Menu::Display:
			// Set up parameters.
			y = MenuVerticalDisplaySettings;

			// Heading
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_DISPLAY), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			// Screen resolution
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SCREEN_RESOLUTION), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 0));
			AddString(MenuRightSideEntry, y, stringBuffer, PRINTSTRING_COLOR_WHITE, SF(titleOption == 0));
			GetNextLinePosition(&y);

			// Windowed mode
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_WINDOWED), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 1));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableWindowedMode), PRINTSTRING_COLOR_WHITE, SF(titleOption == 1));
			GetNextLinePosition(&y);

			// Enable dynamic shadows
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SHADOWS), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 2));
			AddString(MenuRightSideEntry, y, g_GameFlow->GetString(shadowMode), PRINTSTRING_COLOR_WHITE, SF(titleOption == 2));
			GetNextLinePosition(&y);

			// Enable caustics
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_CAUSTICS), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 3));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableCaustics), PRINTSTRING_COLOR_WHITE, SF(titleOption == 3));
			GetNextLinePosition(&y);

			// Enable antialiasing
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_ANTIALIASING), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 4));
			AddString(MenuRightSideEntry, y, g_GameFlow->GetString(antialiasingModeIdString.c_str()), PRINTSTRING_COLOR_WHITE, SF(titleOption == 4));
			GetNextLinePosition(&y);

			// Enable ambient occlusion
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AMBIENT_OCCLUSION), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 5));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableAmbientOcclusion), PRINTSTRING_COLOR_WHITE, SF(titleOption == 5));
			GetNextBlockPosition(&y);

			// Subtitles
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SUBTITLES), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 6));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableSubtitles), PRINTSTRING_COLOR_WHITE, SF(titleOption == 6));

			y = MenuVerticalBottomOptions;

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 7));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 8));
			break;

		case Menu::Sound:
			// Set up parameters.
			y = MenuVerticalSoundSettings;

			// Heading
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_SOUND), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			// Initialize bars if not yet done. Necessary because Y coord is calculated on the fly.
			if (g_MusicVolumeBar == nullptr)
				InitializeMenuBars(y);

			// Music volume
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_MUSIC_VOLUME), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 0));
			DrawBar(g_Gui.GetCurrentSettings().Configuration.MusicVolume / 100.0f, *g_MusicVolumeBar, ID_SFX_BAR_TEXTURE, 0, false);
			GetNextLinePosition(&y);

			// SFX volume
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_SFX_VOLUME), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 1));
			DrawBar(g_Gui.GetCurrentSettings().Configuration.SfxVolume / 100.0f, *g_SFXVolumeBar, ID_SFX_BAR_TEXTURE, 0, false);
			GetNextLinePosition(&y);

			// Reverb
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_REVERB), PRINTSTRING_COLOR_ORANGE, SF(titleOption == 2));
			AddString(MenuRightSideEntry, y, Str_Enabled(g_Gui.GetCurrentSettings().Configuration.EnableReverb), PRINTSTRING_COLOR_WHITE, SF(titleOption == 2));
			GetNextBlockPosition(&y);

			y = MenuVerticalBottomOptions;

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 3));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == 4));
			break;

		case Menu::GeneralActions:
		{
			// Set up needed parameters.
			y = MenuVerticalKeyBindingsSettings;

			// Arrows
			AddString(RIGHT_ARROW_X_OFFSET, y, RIGHT_ARROW_STRING.c_str(), PRINTSTRING_COLOR_YELLOW, SF(true));

			// Heading
			auto titleString = std::string(g_GameFlow->GetString(STRING_GENERAL_ACTIONS));
			AddString(MenuCenterEntry, y, titleString.c_str(), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			// General action listing
			for (int k = 0; k < GeneralActionStrings.size(); k++)
			{
				AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(GeneralActionStrings[k].c_str()), PRINTSTRING_COLOR_ORANGE, SF(titleOption == k));

				if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
				{
					AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), PRINTSTRING_COLOR_YELLOW, SF(true));
				}
				else
				{
					int index = Bindings[1][k] ? Bindings[1][k] : Bindings[0][k];
					AddString(MenuRightSideEntry, y, g_KeyNames[index].c_str(), PRINTSTRING_COLOR_WHITE, SF(false));
				}

				if (k < (GeneralActionStrings.size() - 1))
					GetNextNarrowLinePosition(&y);
			}

			y = MenuVerticalBottomOptions;

			// Reset to defaults
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == GeneralActionStrings.size()));
			GetNextLinePosition(&y);

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (GeneralActionStrings.size() + 1)));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (GeneralActionStrings.size() + 2)));
			break;
		}

		case Menu::VehicleActions:
		{
			// Set up needed parameters.
			y = MenuVerticalKeyBindingsSettings;

			// Arrows
			AddString(MenuLeftSideEntry, y, LEFT_ARROW_STRING.c_str(), PRINTSTRING_COLOR_YELLOW, SF(true));
			AddString(RIGHT_ARROW_X_OFFSET, y, RIGHT_ARROW_STRING.c_str(), PRINTSTRING_COLOR_YELLOW, SF(true));

			// Heading
			auto titleString = std::string(g_GameFlow->GetString(STRING_VEHICLE_ACTIONS));
			AddString(MenuCenterEntry, y, titleString.c_str(), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			int baseIndex = (int)In::Accelerate;

			// Vehicle action listing
			for (int k = 0; k < VehicleActionStrings.size(); k++)
			{
				AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(VehicleActionStrings[k].c_str()), PRINTSTRING_COLOR_ORANGE, SF(titleOption == k));

				if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
				{
					AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), PRINTSTRING_COLOR_YELLOW, SF(true));
				}
				else
				{
					int index = Bindings[1][baseIndex + k] ? Bindings[1][baseIndex + k] : Bindings[0][baseIndex + k];
					AddString(MenuRightSideEntry, y, g_KeyNames[index].c_str(), PRINTSTRING_COLOR_WHITE, SF(false));
				}

				if (k < (VehicleActionStrings.size() - 1))
				{
					GetNextNarrowLinePosition(&y);
				}
				else
				{
					GetNextBlockPosition(&y);
				}
			}

			y = MenuVerticalBottomOptions;

			// Reset to defaults
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == VehicleActionStrings.size()));
			GetNextLinePosition(&y);

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (VehicleActionStrings.size() + 1)));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (VehicleActionStrings.size() + 2)));
			break;
		}

		case Menu::QuickActions:
		{
			// Set up needed parameters.
			y = MenuVerticalKeyBindingsSettings;

			// Arrows
			AddString(MenuLeftSideEntry, y, LEFT_ARROW_STRING.c_str(), PRINTSTRING_COLOR_YELLOW, SF(true));
			AddString(RIGHT_ARROW_X_OFFSET, y, RIGHT_ARROW_STRING.c_str(), PRINTSTRING_COLOR_YELLOW, SF(true));

			// Heading
			auto titleString = std::string(g_GameFlow->GetString(STRING_QUICK_ACTIONS));
			AddString(MenuCenterEntry, y, titleString.c_str(), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			int baseIndex = (int)In::Flare;

			// Quick action listing
			for (int k = 0; k < QuickActionStrings.size(); k++)
			{
				AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(QuickActionStrings[k].c_str()), PRINTSTRING_COLOR_ORANGE, SF(titleOption == k));

				if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
				{
					AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), PRINTSTRING_COLOR_YELLOW, SF(true));
				}
				else
				{
					int index = Bindings[1][baseIndex + k] ? Bindings[1][baseIndex + k] : Bindings[0][baseIndex + k];
					AddString(MenuRightSideEntry, y, g_KeyNames[index].c_str(), PRINTSTRING_COLOR_WHITE, SF(false));
				}

				if (k < (QuickActionStrings.size() - 1))
					GetNextNarrowLinePosition(&y);
			}

			y = MenuVerticalBottomOptions;

			// Reset to defaults
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == QuickActionStrings.size()));
			GetNextLinePosition(&y);

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (QuickActionStrings.size() + 1)));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (QuickActionStrings.size() + 2)));
			break;
		}

		case Menu::MenuActions:
		{
			// Set up parameters.
			y = MenuVerticalKeyBindingsSettings;

			// Arrows
			AddString(MenuLeftSideEntry, y, LEFT_ARROW_STRING.c_str(), PRINTSTRING_COLOR_YELLOW, SF(true));

			// Heading
			auto titleString = std::string(g_GameFlow->GetString(STRING_MENU_ACTIONS));
			AddString(MenuCenterEntry, y, titleString.c_str(), PRINTSTRING_COLOR_YELLOW, SF_Center());
			GetNextBlockPosition(&y);

			int baseIndex = (int)In::Select;

			// Menu action listing.
			for (int k = 0; k < MenuActionStrings.size(); k++)
			{
				AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(MenuActionStrings[k].c_str()), PRINTSTRING_COLOR_ORANGE, SF(titleOption == k));

				if (g_Gui.GetCurrentSettings().NewKeyWaitTimer > 0.0f && titleOption == k)
				{
					AddString(MenuRightSideEntry, y, g_GameFlow->GetString(STRING_WAITING_FOR_INPUT), PRINTSTRING_COLOR_YELLOW, SF(true));
				}
				else
				{
					int index = Bindings[1][baseIndex + k] ? Bindings[1][baseIndex + k] : Bindings[0][baseIndex + k];
					AddString(MenuRightSideEntry, y, g_KeyNames[index].c_str(), PRINTSTRING_COLOR_WHITE, SF(false));
				}

				if (k < (MenuActionStrings.size() - 1))
					GetNextNarrowLinePosition(&y);
			}

			y = MenuVerticalBottomOptions;

			// Reset to defaults
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_RESET_TO_DEFAULTS), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == MenuActionStrings.size()));
			GetNextLinePosition(&y);

			// Apply
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_APPLY), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (MenuActionStrings.size() + 1)));
			GetNextLinePosition(&y);

			// Cancel
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_CANCEL), PRINTSTRING_COLOR_ORANGE, SF_Center(titleOption == (MenuActionStrings.size() + 2)));
			break;
		}
		}
	}

	void Renderer::RenderTitleMenu(Menu menu)
	{
		int y = MenuVerticalBottomCenter;
		int titleOption = g_Gui.GetSelectedOption();
		int selectedOption = 0;

		switch (menu)
		{
		case Menu::Title:

			// New game
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_NEW_GAME), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == selectedOption));
			GetNextLinePosition(&y);
			selectedOption++;

			// Load game
			if (g_GameFlow->IsLoadSaveEnabled())
			{
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_LOAD_GAME), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == selectedOption));
				GetNextLinePosition(&y);
				selectedOption++;
			}

			// Options
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_OPTIONS), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == selectedOption));
			GetNextLinePosition(&y);
			selectedOption++;

			// Exit game
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_EXIT_GAME), PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == selectedOption));
			break;

		case Menu::LoadGame:
			RenderLoadSaveMenu();
			break;

		case Menu::SelectLevel:
			// Set up parameters.
			y = MenuVerticalLineSpacing;

			// Heading
			AddString(MenuCenterEntry, 26, g_GameFlow->GetString(STRING_SELECT_LEVEL), PRINTSTRING_COLOR_ORANGE, SF_Center());
			GetNextBlockPosition(&y);

			// Level listing (starts with 1 because 0 is always title)
			for (int i = 1; i < g_GameFlow->GetNumLevels(); i++, selectedOption++)
			{
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(g_GameFlow->GetLevel(i)->NameStringKey.c_str()),
					PRINTSTRING_COLOR_WHITE, SF_Center(titleOption == selectedOption));
				GetNextNarrowLinePosition(&y);
			}

			break;

		case Menu::Options:
		case Menu::Controls:
		case Menu::Gameplay:
		case Menu::Display:
		case Menu::Sound:
		case Menu::GeneralActions:
		case Menu::VehicleActions:
		case Menu::QuickActions:
		case Menu::MenuActions:
			RenderOptionsMenu(menu, MenuVerticalOptionsTitle);
			break;

		default:
			TENLog("Failed to render title submenu.", LogLevel::Error);
			break;
		}
	}

	void Renderer::RenderPauseMenu(Menu menu)
	{
		int y = 0;
		auto pauseOption = g_Gui.GetSelectedOption();

		switch (g_Gui.GetMenuToDisplay())
		{
		case Menu::Pause:

			// Set up parameters.
			y = MenuVerticalPause;

			// Heading
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_PAUSE), PRINTSTRING_COLOR_ORANGE, SF_Center());
			GetNextBlockPosition(&y);

			// Statistics
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_STATISTICS), PRINTSTRING_COLOR_WHITE, SF_Center(pauseOption == 0));
			GetNextLinePosition(&y);

			// Options
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_OPTIONS), PRINTSTRING_COLOR_WHITE, SF_Center(pauseOption == 1));
			GetNextLinePosition(&y);

			// Exit to title
			AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_EXIT_TO_TITLE), PRINTSTRING_COLOR_WHITE, SF_Center(pauseOption == 2));
			break;

		case Menu::Statistics:
			DrawStatistics();
			break;

		case Menu::Options:
		case Menu::Controls:
		case Menu::Gameplay:
		case Menu::Display:
		case Menu::Sound:
		case Menu::GeneralActions:
		case Menu::VehicleActions:
		case Menu::QuickActions:
		case Menu::MenuActions:
			RenderOptionsMenu(menu, MenuVerticalOptionsPause);
			break;

		default:
			TENLog("Failed to render menu.", LogLevel::Error);
			break;
		}

		DrawLines2D();
		DrawAllStrings();
	}

	void Renderer::RenderLoadSaveMenu()
	{
		if (!g_GameFlow->IsLoadSaveEnabled())
		{
			g_Gui.SetInventoryMode(InventoryMode::InGame);
			return;
		}

		// Set up parameters.
		int y = MenuVerticalLineSpacing;
		short selection = g_Gui.GetLoadSaveSelection();
		char stringBuffer[255];

		// Heading
		AddString(MenuCenterEntry, MenuVerticalNarrowLineSpacing, Str_LoadSave(g_Gui.GetInventoryMode() == InventoryMode::Save), 
			PRINTSTRING_COLOR_ORANGE, SF_Center());
		GetNextBlockPosition(&y);

		// Savegame listing
		for (int n = 0; n < SAVEGAME_MAX; n++)
		{
			auto& save = SaveGame::Infos[n];

			if (!save.Present)
			{
				AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_EMPTY), PRINTSTRING_COLOR_WHITE, SF_Center(selection == n));
			}
			else
			{
				// Number
				sprintf(stringBuffer, "%03d", save.Count);
				AddString(MenuLoadNumberLeftSide, y, stringBuffer, PRINTSTRING_COLOR_WHITE, SF(selection == n));

				// Level name
				AddString(MenuLoadNameLeftSide, y, (char*)save.LevelName.c_str(), PRINTSTRING_COLOR_WHITE, SF(selection == n));

				// Timestamp
				sprintf(stringBuffer, g_GameFlow->GetString(STRING_SAVEGAME_TIMESTAMP), save.Days, save.Hours, save.Minutes, save.Seconds);
				AddString(MenuRightSideEntry, y, stringBuffer, PRINTSTRING_COLOR_WHITE, SF(selection == n));
			}

			GetNextLinePosition(&y);
		}

		DrawAllStrings();
	}

	void Renderer::DrawStatistics()
	{
		char buffer[40];

		ScriptInterfaceLevel* lvl = g_GameFlow->GetLevel(CurrentLevel);
		auto y = MenuVerticalStatisticsTitle;

		// Heading
		AddString(MenuCenterEntry, y, g_GameFlow->GetString(STRING_OPTIONS_STATISTICS), PRINTSTRING_COLOR_ORANGE, SF_Center());
		GetNextBlockPosition(&y);

		// Level name
		AddString(MenuCenterEntry, y, g_GameFlow->GetString(lvl->NameStringKey.c_str()), PRINTSTRING_COLOR_YELLOW, SF_Center());
		GetNextBlockPosition(&y);

		// Time taken
		auto gameTime = GetGameTime(GameTimer);
		sprintf(buffer, "%02d:%02d:%02d", (gameTime.Days * DAY_UNIT) + gameTime.Hours, gameTime.Minutes, gameTime.Seconds);
		AddString(MenuRightSideEntry, y, buffer, PRINTSTRING_COLOR_WHITE, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_TIME_TAKEN), PRINTSTRING_COLOR_ORANGE, SF());
		GetNextLinePosition(&y);

		// Distance travelled
		sprintf(buffer, "%dm", SaveGame::Statistics.Game.Distance / UnitsToMeters);
		AddString(MenuRightSideEntry, y, buffer, PRINTSTRING_COLOR_WHITE, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_DISTANCE_TRAVELLED), PRINTSTRING_COLOR_ORANGE, SF());
		GetNextLinePosition(&y);

		// Ammo used
		sprintf(buffer, "%d", SaveGame::Statistics.Game.AmmoUsed);
		AddString(MenuRightSideEntry, y, buffer, PRINTSTRING_COLOR_WHITE, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_AMMO_USED), PRINTSTRING_COLOR_ORANGE, SF());
		GetNextLinePosition(&y);

		// Medipacks used
		sprintf(buffer, "%d", SaveGame::Statistics.Game.HealthUsed);
		AddString(MenuRightSideEntry, y, buffer, PRINTSTRING_COLOR_WHITE, SF());
		AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_USED_MEDIPACKS), PRINTSTRING_COLOR_ORANGE, SF());
		GetNextLinePosition(&y);

		// Secrets found in Level
		if (g_GameFlow->GetLevel(CurrentLevel)->GetSecrets() > 0)
		{
			std::bitset<32> levelSecretBitSet(SaveGame::Statistics.Level.Secrets);
			sprintf(buffer, "%d / %d", (int)levelSecretBitSet.count(), g_GameFlow->GetLevel(CurrentLevel)->GetSecrets());
			AddString(MenuRightSideEntry, y, buffer, PRINTSTRING_COLOR_WHITE, SF());
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_LEVEL_SECRETS_FOUND), PRINTSTRING_COLOR_ORANGE, SF());
			GetNextLinePosition(&y);
		}

		// Secrets found total
		if (g_GameFlow->TotalNumberOfSecrets > 0)
		{
			sprintf(buffer, "%d / %d", SaveGame::Statistics.Game.Secrets, g_GameFlow->TotalNumberOfSecrets);
			AddString(MenuRightSideEntry, y, buffer, PRINTSTRING_COLOR_WHITE, SF());
			AddString(MenuLeftSideEntry, y, g_GameFlow->GetString(STRING_TOTAL_SECRETS_FOUND), PRINTSTRING_COLOR_ORANGE, SF());
		}

		DrawAllStrings();
	}

	void Renderer::RenderNewInventory()
	{
		g_Gui.DrawCurrentObjectList(LaraItem, RingTypes::Inventory);

		if (g_Gui.GetRing(RingTypes::Ammo).RingActive)
			g_Gui.DrawCurrentObjectList(LaraItem, RingTypes::Ammo);

		g_Gui.DrawAmmoSelector();
		g_Gui.FadeAmmoSelector();
		g_Gui.DrawCompass(LaraItem);

		DrawAllStrings();
	}

	void Renderer::DrawDisplayPickup(const DisplayPickup& pickup)
	{
		constexpr auto COUNT_STRING_INF	   = "Inf";
		constexpr auto COUNT_STRING_OFFSET = Vector2(DISPLAY_SPACE_RES.x / 40, 0.0f);

		// Draw display pickup.
		DrawObjectIn2DSpace(pickup.ObjectID, pickup.Position, pickup.Orientation, pickup.Scale);

		// Draw count string.
		if (pickup.Count != 1)
		{
			auto countString = (pickup.Count != -1) ? std::to_string(pickup.Count) : COUNT_STRING_INF;
			auto countStringPos = pickup.Position + COUNT_STRING_OFFSET;

			AddString(countString, countStringPos, Color(PRINTSTRING_COLOR_WHITE), pickup.StringScale, SF());
		}
	}

	// TODO: Handle opacity
	void Renderer::DrawObjectIn2DSpace(int objectNumber, Vector2 pos2D, EulerAngles orient, float scale, float opacity, int meshBits)
	{
		constexpr auto AMBIENT_LIGHT_COLOR = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		auto screenRes = GetScreenResolution();
		auto factor = Vector2(
			screenRes.x / DISPLAY_SPACE_RES.x,
			screenRes.y / DISPLAY_SPACE_RES.y);

		pos2D *= factor;
		scale *= (factor.x > factor.y) ? factor.y : factor.x;

		int index = g_Gui.ConvertObjectToInventoryItem(objectNumber);
		if (index != -1)
		{
			const auto& invObject = InventoryObjectTable[index];

			pos2D.y += invObject.YOffset;
			orient += invObject.Orientation;
		}

		auto viewMatrix = Matrix::CreateLookAt(Vector3(0.0f, 0.0f, BLOCK(2)), Vector3::Zero, Vector3::Down);
		auto projMatrix = Matrix::CreateOrthographic(_screenWidth, _screenHeight, -BLOCK(1), BLOCK(1));

		auto& moveableObject = _moveableObjects[objectNumber];
		if (!moveableObject)
			return;

		const auto& object = Objects[objectNumber];
		if (object.animIndex != -1)
		{
			auto frameData = AnimFrameInterpData
			{
				&g_Level.Frames[GetAnimData(object.animIndex).FramePtr],
				&g_Level.Frames[GetAnimData(object.animIndex).FramePtr],
				0.0f
			};
			UpdateAnimation(nullptr, *moveableObject, frameData, 0xFFFFFFFF);
		}

		auto pos = _viewportToolkit.Unproject(Vector3(pos2D.x, pos2D.y, 1.0f), projMatrix, viewMatrix, Matrix::Identity);

		// Set vertex buffer.
		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		// Set shaders.
		_context->VSSetShader(_vsInventory.Get(), nullptr, 0);
		_context->PSSetShader(_psInventory.Get(), nullptr, 0);

		// Set matrices.
		CCameraMatrixBuffer hudCamera;
		hudCamera.CamDirectionWS = -Vector4::UnitZ;
		hudCamera.ViewProjection = viewMatrix * projMatrix;
		_cbCameraMatrices.UpdateData(hudCamera, _context.Get());
		BindConstantBufferVS(ConstantBufferRegister::Camera, _cbCameraMatrices.get());

		for (int n = 0; n < (*moveableObject).ObjectMeshes.size(); n++)
		{
			if (meshBits && !(meshBits & (1 << n)))
				continue;
			
			auto* mesh = (*moveableObject).ObjectMeshes[n];

			// HACK: Rotate compass needle.
			if (objectNumber == ID_COMPASS_ITEM && n == 1)
				(*moveableObject).LinearizedBones[n]->ExtraRotation = EulerAngles(0, g_Gui.CompassNeedleAngle - ANGLE(180.0f), 0).ToQuaternion();

			// Construct world matrix.
			auto tMatrix = Matrix::CreateTranslation(pos.x, pos.y, pos.z + BLOCK(1));
			auto rotMatrix = orient.ToRotationMatrix();
			auto scaleMatrix = Matrix::CreateScale(scale);
			auto worldMatrix = scaleMatrix * rotMatrix * tMatrix;

			if (object.animIndex != -1)
				_stItem.World = (*moveableObject).AnimationTransforms[n] * worldMatrix;
			else
				_stItem.World = (*moveableObject).BindPoseTransforms[n] * worldMatrix;

			_stItem.BoneLightModes[n] = (int)LightMode::Dynamic;
			_stItem.Color = Vector4::One;
			_stItem.AmbientLight = AMBIENT_LIGHT_COLOR;

			_cbItem.UpdateData(_stItem, _context.Get());
			BindConstantBufferVS(ConstantBufferRegister::Item, _cbItem.get());
			BindConstantBufferPS(ConstantBufferRegister::Item, _cbItem.get());
			
			for (const auto& bucket : mesh->Buckets)
			{
				if (bucket.NumVertices == 0)
					continue;

				SetBlendMode(BlendMode::Opaque);
				SetCullMode(CullMode::CounterClockwise);
				SetDepthState(DepthState::Write);

				BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[bucket.Texture]), SamplerStateRegister::AnisotropicClamp);
				
				 if (bucket.BlendMode != BlendMode::Opaque)
					Renderer::SetBlendMode(bucket.BlendMode, true);
				
				SetAlphaTest(
					(bucket.BlendMode == BlendMode::AlphaTest) ? AlphaTestMode::GreatherThan : AlphaTestMode::None,
					ALPHA_TEST_THRESHOLD);

				DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);
				_numMoveablesDrawCalls++;
			}
		}
	}

	void Renderer::RenderTitleImage()
	{
		Texture2D texture;
		SetTextureOrDefault(texture, TEN::Utils::ToWString(g_GameFlow->IntroImagePath.c_str()));

		if (!texture.Texture)
			return;

		int timeout = 20;
		float currentFade = FADE_FACTOR;

		while (timeout || currentFade > 0.0f)
		{
			if (timeout)
			{
				if (currentFade < 1.0f)
					currentFade = std::clamp(currentFade += FADE_FACTOR, 0.0f, 1.0f);
				else
					timeout--;
			}
			else
			{
				currentFade = std::clamp(currentFade -= FADE_FACTOR, 0.0f, 1.0f);
			}

			DrawFullScreenImage(texture.ShaderResourceView.Get(), Smoothstep(currentFade), _backBuffer.RenderTargetView.Get(), _backBuffer.DepthStencilView.Get());
			Synchronize();
			_swapChain->Present(0, 0);
			_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		}
	}

	void Renderer::DrawExamines()
	{
		constexpr auto SCREEN_POS = Vector2(400.0f, 300.0f);

		static EulerAngles orient = EulerAngles::Identity;
		static float scaler = 1.2f;

		short invItem = g_Gui.GetRing(RingTypes::Inventory).CurrentObjectList[g_Gui.GetRing(RingTypes::Inventory).CurrentObjectInList].InventoryItem;

		auto& object = InventoryObjectTable[invItem];

		if (IsHeld(In::Forward))
			orient.x += ANGLE(3.0f);

		if (IsHeld(In::Back))
			orient.x -= ANGLE(3.0f);

		if (IsHeld(In::Left))
			orient.y += ANGLE(3.0f);

		if (IsHeld(In::Right))
			orient.y -= ANGLE(3.0f);

		if (IsHeld(In::Sprint))
			scaler += 0.03f;

		if (IsHeld(In::Crouch))
			scaler -= 0.03f;

		if (scaler > 1.6f)
			scaler = 1.6f;

		if (scaler < 0.8f)
			scaler = 0.8f;

		float savedScale = object.Scale1;
		object.Scale1 = scaler;
		DrawObjectIn2DSpace(g_Gui.ConvertInventoryItemToObject(invItem), SCREEN_POS, orient, object.Scale1);
		object.Scale1 = savedScale;
	}

	void Renderer::DrawDiary()
	{
		constexpr auto SCREEN_POS = Vector2(400.0f, 300.0f);

		const auto& object = InventoryObjectTable[INV_OBJECT_OPEN_DIARY];
		unsigned int currentPage = Lara.Inventory.Diary.CurrentPage;

		DrawObjectIn2DSpace(g_Gui.ConvertInventoryItemToObject(INV_OBJECT_OPEN_DIARY), SCREEN_POS, object.Orientation, object.Scale1);

		for (int i = 0; i < MAX_DIARY_STRINGS_PER_PAGE; i++)
		{
			if (!Lara.Inventory.Diary.Pages[Lara.Inventory.Diary.CurrentPage].Strings[i].Position.x && !Lara.Inventory.Diary.Pages[Lara.Inventory.Diary.CurrentPage].
				Strings[i].Position.y && !Lara.Inventory.Diary.Pages[Lara.Inventory.Diary.CurrentPage].Strings[i].StringID)
			{
				break;
			}

			//AddString(Lara.Diary.Pages[currentPage].Strings[i].x, Lara.Diary.Pages[currentPage].Strings[i].y, g_GameFlow->GetString(Lara.Diary.Pages[currentPage].Strings[i].stringID), PRINTSTRING_COLOR_WHITE, 0);
		}

		DrawAllStrings();
	}

	void Renderer::RenderInventoryScene(RenderTarget2D* renderTarget, TextureBase* background, float backgroundFade)
	{
		// Set basic render states
		SetBlendMode(BlendMode::Opaque, true);
		SetDepthState(DepthState::Write, true);
		SetCullMode(CullMode::CounterClockwise, true);

		// Bind and clear render target
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(), _renderTarget.DepthStencilView.Get());
		_context->RSSetViewports(1, &_viewport);
		ResetScissor();

		if (background != nullptr)
		{
			DrawFullScreenImage(background->ShaderResourceView.Get(), backgroundFade, _renderTarget.RenderTargetView.Get(), _renderTarget.DepthStencilView.Get());
		}

		_context->ClearDepthStencilView(_renderTarget.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		// Set vertex buffer
		_context->IASetVertexBuffers(0, 1, _moveablesVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetIndexBuffer(_moveablesIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);

		// Set shaders
		_context->VSSetShader(_vsInventory.Get(), nullptr, 0);
		_context->PSSetShader(_psInventory.Get(), nullptr, 0);

		// Set texture
		BindTexture(TextureRegister::ColorMap, &std::get<0>(_moveablesTextures[0]), SamplerStateRegister::AnisotropicClamp);
		BindTexture(TextureRegister::NormalMap, &std::get<1>(_moveablesTextures[0]), SamplerStateRegister::AnisotropicClamp);

		if (CurrentLevel == 0)
		{
			auto titleMenu = g_Gui.GetMenuToDisplay();
			bool drawLogo = (titleMenu == Menu::Title || titleMenu == Menu::Options);

			if (drawLogo && _logo.Texture != nullptr)
			{
				float factorX = (float)_screenWidth / DISPLAY_SPACE_RES.x;
				float factorY = (float)_screenHeight / DISPLAY_SPACE_RES.y;
				float scale = _screenWidth > _screenHeight ? factorX : factorY;

				int logoLeft   = (DISPLAY_SPACE_RES.x / 2) - (LogoWidth / 2);
				int logoRight  = (DISPLAY_SPACE_RES.x / 2) + (LogoWidth / 2);
				int logoBottom = LogoTop + LogoHeight;

				RECT rect;
				rect.left   = logoLeft   * scale;
				rect.right  = logoRight  * scale;
				rect.top    = LogoTop    * scale;
				rect.bottom = logoBottom * scale;

				_spriteBatch->Begin(SpriteSortMode_BackToFront, _renderStates->NonPremultiplied());
				_spriteBatch->Draw(_logo.ShaderResourceView.Get(), rect, Vector4::One * g_ScreenEffect.ScreenFadeCurrent);
				_spriteBatch->End();
			}

			RenderTitleMenu(titleMenu);
		}
		else
		{
			switch (g_Gui.GetInventoryMode())
			{
			case InventoryMode::Load:
			case InventoryMode::Save:
				RenderLoadSaveMenu();
				break;

			case InventoryMode::InGame:
				RenderNewInventory();
				break;

			case InventoryMode::Statistics:
				DrawStatistics();
				break;

			case InventoryMode::Examine:
				DrawExamines();
				break;

			case InventoryMode::Pause:
				RenderPauseMenu(g_Gui.GetMenuToDisplay());
				break;

			case InventoryMode::Diary:
				DrawDiary();
				break;
			}
		}

		_context->ClearDepthStencilView(_renderTarget.DepthStencilView.Get(), D3D11_CLEAR_STENCIL | D3D11_CLEAR_DEPTH, 1.0f, 0);

		switch (g_Configuration.AntialiasingMode)
		{
		case AntialiasingMode::None:
			break;

		case AntialiasingMode::Low:
			ApplyFXAA(&_renderTarget, _gameCamera);
			break;

		case AntialiasingMode::Medium:
		case AntialiasingMode::High:
			ApplySMAA(&_renderTarget, _gameCamera);
			break;
		}

		CopyRenderTarget(&_renderTarget, renderTarget, _gameCamera);
	}

	void Renderer::SetLoadingScreen(std::wstring& fileName)
	{
		SetTextureOrDefault(_loadingScreenTexture, fileName);
	}

	void Renderer::RenderLoadingScreen(float percentage)
	{
		// Set basic render states
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);

		do
		{
			// Clear screen
			_context->ClearRenderTargetView(_backBuffer.RenderTargetView.Get(), Colors::Black);
			_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			// Bind the back buffer
			_context->OMSetRenderTargets(1, _backBuffer.RenderTargetView.GetAddressOf(), _backBuffer.DepthStencilView.Get());
			_context->RSSetViewports(1, &_viewport);
			ResetScissor();

			// Draw the full screen background
			if (_loadingScreenTexture.Texture)
				DrawFullScreenQuad(_loadingScreenTexture.ShaderResourceView.Get(), Vector3(g_ScreenEffect.ScreenFadeCurrent));

			if (g_ScreenEffect.ScreenFadeCurrent && percentage > 0.0f && percentage < 100.0f)
				DrawLoadingBar(percentage);

			_swapChain->Present(0, 0);
			_context->ClearState();

			Synchronize();
			UpdateFadeScreenAndCinematicBars();

		} while (g_ScreenEffect.ScreenFading || !g_ScreenEffect.ScreenFadedOut);
	}

	void Renderer::RenderInventory()
	{
		_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_STENCIL | D3D11_CLEAR_DEPTH, 1.0f, 0);
		_context->ClearRenderTargetView(_backBuffer.RenderTargetView.Get(), Colors::Black);
		
		RenderInventoryScene(&_backBuffer, &_dumpScreenRenderTarget, 0.5f);
		
		_swapChain->Present(0, 0);
	}

	void Renderer::RenderTitle()
	{
		RenderScene(&_dumpScreenRenderTarget, false, _gameCamera);

		_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_STENCIL | D3D11_CLEAR_DEPTH, 1.0f, 0);
		_context->ClearRenderTargetView(_backBuffer.RenderTargetView.Get(), Colors::Black);

		RenderInventoryScene(&_backBuffer, &_dumpScreenRenderTarget, 1.0f);
		DrawAllStrings();

		_swapChain->Present(0, 0);
	}

	void Renderer::DrawDebugInfo(RenderView& view)
	{
		if (CurrentLevel != 0)
		{
			_currentY = 20;

			ROOM_INFO* r = &g_Level.Rooms[LaraItem->RoomNumber];

			float aspectRatio = _screenWidth / (float)_screenHeight;
			int thumbWidth = _screenWidth / 6;
			RECT rect;
			int thumbY = 0;

			switch (_debugPage)
			{
			case RendererDebugPage::None:
				break;

			case RendererDebugPage::RendererStats:
				PrintDebugMessage("RENDERER STATS");
				PrintDebugMessage("FPS: %3.2f", _fps);
				PrintDebugMessage("Resolution: %d x %d", _screenWidth, _screenHeight);
				PrintDebugMessage("GPU: %s", g_Configuration.AdapterName.c_str());
				PrintDebugMessage("Update time: %d", _timeUpdate);
				PrintDebugMessage("Frame time: %d", _timeFrame);
				PrintDebugMessage("ControlPhase() time: %d", ControlPhaseTime);
				PrintDebugMessage("Room collector time: %d", _timeRoomsCollector);
				PrintDebugMessage("TOTAL Draw calls: %d", _numDrawCalls);
				PrintDebugMessage("    Rooms: %d", _numRoomsDrawCalls);
				PrintDebugMessage("    Movables: %d", _numMoveablesDrawCalls);
				PrintDebugMessage("    Statics: %d", _numStaticsDrawCalls);
				PrintDebugMessage("    Instanced Statics: %d", _numInstancedStaticsDrawCalls);
				PrintDebugMessage("    Sprites: %d", _numSpritesDrawCalls);
				PrintDebugMessage("    Instanced Sprites: %d", _numInstancedSpritesDrawCalls);
				PrintDebugMessage("TOTAL Triangles: %d", _numTriangles);
				PrintDebugMessage("Sprites: %d", view.SpritesToDraw.size());
				PrintDebugMessage("SORTED Draw calls: %d", (_numSortedRoomsDrawCalls + _numSortedMoveablesDrawCalls + _numSortedStaticsDrawCalls
					+ _numSortedSpritesDrawCalls));
				PrintDebugMessage("    Rooms: %d", _numSortedRoomsDrawCalls);
				PrintDebugMessage("    Movables: %d", _numSortedMoveablesDrawCalls);
				PrintDebugMessage("    Statics: %d", _numSortedStaticsDrawCalls);
				PrintDebugMessage("    Sprites: %d", _numSortedSpritesDrawCalls);
				PrintDebugMessage("SHADOW MAPS Draw calls: %d", _numShadowMapDrawCalls);
				PrintDebugMessage("DEBRIS Draw calls: %d", _numDebrisDrawCalls);
				PrintDebugMessage("Rooms: %d", view.RoomsToDraw.size());
				PrintDebugMessage("    CheckPortal() calls: %d", _numCheckPortalCalls);
				PrintDebugMessage("    GetVisibleRooms() calls: %d", _numGetVisibleRoomsCalls);
				PrintDebugMessage("    Dot products: %d", _numDotProducts);
				 
				_spriteBatch->Begin(SpriteSortMode_Deferred, _renderStates->Opaque()); 

				rect.left = _screenWidth - thumbWidth;
				rect.top = thumbY;
				rect.right = rect.left+ thumbWidth;
				rect.bottom = rect.top+thumbWidth / aspectRatio;
				  
				_spriteBatch->Draw(_normalsRenderTarget.ShaderResourceView.Get(), rect);
				thumbY += thumbWidth / aspectRatio;

				rect.left = _screenWidth - thumbWidth;
				rect.top = thumbY;
				rect.right = rect.left + thumbWidth;
				rect.bottom = rect.top + thumbWidth / aspectRatio;
				   
				//_spriteBatch->Draw(_depthRenderTarget.ShaderResourceView.Get(), rect);
				//thumbY += thumbWidth / aspectRatio;

				rect.left = _screenWidth - thumbWidth;
				rect.top = thumbY;
				rect.right = rect.left + thumbWidth;
				rect.bottom = rect.top + thumbWidth / aspectRatio;

				_spriteBatch->Draw(_SSAOBlurredRenderTarget.ShaderResourceView.Get(), rect);
				thumbY += thumbWidth / aspectRatio;
				  
				if (g_Configuration.AntialiasingMode > AntialiasingMode::Low)
				{
					rect.left = _screenWidth - thumbWidth;
					rect.top = thumbY;
					rect.right = rect.left + thumbWidth;
					rect.bottom = rect.top + thumbWidth / aspectRatio;

					_spriteBatch->Draw(_SMAAEdgesRenderTarget.ShaderResourceView.Get(), rect);
					thumbY += thumbWidth / aspectRatio;

					rect.left = _screenWidth - thumbWidth;
					rect.top = thumbY;
					rect.right = rect.left + thumbWidth;
					rect.bottom = rect.top + thumbWidth / aspectRatio;

					_spriteBatch->Draw(_SMAABlendRenderTarget.ShaderResourceView.Get(), rect);
					thumbY += thumbWidth / aspectRatio;
				}

				_spriteBatch->End();

				break;

			case RendererDebugPage::DimensionStats:
				PrintDebugMessage("DIMENSION STATS");
				PrintDebugMessage("Pos: %d, %d, %d", LaraItem->Pose.Position.x, LaraItem->Pose.Position.y, LaraItem->Pose.Position.z);
				PrintDebugMessage("Orient: %d, %d, %d", LaraItem->Pose.Orientation.x, LaraItem->Pose.Orientation.y, LaraItem->Pose.Orientation.z);
				PrintDebugMessage("RoomNumber: %d", LaraItem->RoomNumber);
				PrintDebugMessage("Room location: %d, %d", LaraItem->Location.RoomNumber, LaraItem->Location.Height);
				PrintDebugMessage("BoxNumber: %d", LaraItem->BoxNumber);
				PrintDebugMessage("WaterSurfaceDist: %d", Lara.Context.WaterSurfaceDist);
				PrintDebugMessage("Room: %d, %d, %d, %d", r->x, r->z, r->x + r->xSize * BLOCK(1), r->z + r->zSize * BLOCK(1));
				PrintDebugMessage("Room.y, minFloor, maxCeiling: %d, %d, %d ", r->y, r->minfloor, r->maxceiling);
				PrintDebugMessage("Camera.Position: %.3f, %.3f, %.3f", g_Camera.Position.x, g_Camera.Position.y, g_Camera.Position.z);
				PrintDebugMessage("Camera.LookAt: %.3f, %.3f, %.3f", g_Camera.LookAt.x, g_Camera.LookAt.y, g_Camera.LookAt.z);
				PrintDebugMessage("Camera.RoomNumber: %d", g_Camera.RoomNumber);
				PrintDebugMessage("Camera azimuth, altitude: %d, %d", g_Camera.actualAngle, g_Camera.actualElevation);
				break;

			case RendererDebugPage::PlayerStats:
				PrintDebugMessage("PLAYER STATS");
				PrintDebugMessage("AnimObjectID: %d", LaraItem->Animation.AnimObjectID);
				PrintDebugMessage("AnimNumber: %d", LaraItem->Animation.AnimNumber);
				PrintDebugMessage("FrameNumber: %d", LaraItem->Animation.FrameNumber);
				PrintDebugMessage("ActiveState: %d", LaraItem->Animation.ActiveState);
				PrintDebugMessage("TargetState: %d", LaraItem->Animation.TargetState);
				PrintDebugMessage("Velocity: %.3f, %.3f, %.3f", LaraItem->Animation.Velocity.z, LaraItem->Animation.Velocity.y, LaraItem->Animation.Velocity.x);
				PrintDebugMessage("IsAirborne: %d", LaraItem->Animation.IsAirborne);
				PrintDebugMessage("HandStatus: %d", Lara.Control.HandStatus);
				PrintDebugMessage("WaterStatus: %d", Lara.Control.WaterStatus);
				PrintDebugMessage("CanClimbLadder: %d", Lara.Control.CanClimbLadder);
				PrintDebugMessage("CanMonkeySwing: %d", Lara.Control.CanMonkeySwing);
				PrintDebugMessage("Target HitPoints: %d", Lara.TargetEntity ? Lara.TargetEntity->HitPoints : 0);
				break;

			case RendererDebugPage::InputStats:
			{
				auto clickedActions = BitField((int)In::Count);
				auto heldActions = BitField((int)In::Count);
				auto releasedActions = BitField((int)In::Count);

				for (const auto& action : ActionMap)
				{
					if (action.IsClicked())
						clickedActions.Set((int)action.GetID());

					if (action.IsHeld())
						heldActions.Set((int)action.GetID());

					if (action.IsReleased())
						releasedActions.Set((int)action.GetID());
				}
				
				PrintDebugMessage("INPUT STATS");
				PrintDebugMessage(("Clicked actions: " + clickedActions.ToString()).c_str());
				PrintDebugMessage(("Held actions: " + heldActions.ToString()).c_str());
				PrintDebugMessage(("Released actions: " + releasedActions.ToString()).c_str());
				PrintDebugMessage("Move axes: %.3f, %.3f", GetMoveAxis().x, GetMoveAxis().y);
				PrintDebugMessage("Camera axes: %.3f, %.3f", GetCameraAxis().x, GetCameraAxis().y);
				PrintDebugMessage("Mouse axes: %.3f, %.3f", GetMouseAxis().x, GetMouseAxis().y);
				PrintDebugMessage("Cursor pos: %.3f, %.3f", GetMouse2DPosition().x, GetMouse2DPosition().y);
			}
				break;

			case RendererDebugPage::CollisionStats:
				PrintDebugMessage("COLLISION STATS");
				PrintDebugMessage("Collision type: %d", LaraCollision.CollisionType);
				PrintDebugMessage("Bridge item ID: %d", LaraCollision.Middle.Bridge);
				PrintDebugMessage("Front floor: %d", LaraCollision.Front.Floor);
				PrintDebugMessage("Front left floor: %d", LaraCollision.FrontLeft.Floor);
				PrintDebugMessage("Front right floor: %d", LaraCollision.FrontRight.Floor);
				PrintDebugMessage("Front ceil: %d", LaraCollision.Front.Ceiling);
				PrintDebugMessage("Front left ceil: %d", LaraCollision.FrontLeft.Ceiling);
				PrintDebugMessage("Front right ceil: %d", LaraCollision.FrontRight.Ceiling);
				break;
				
			case RendererDebugPage::PathfindingStats:
				PrintDebugMessage("PATHFINDING STATS");
				PrintDebugMessage("BoxNumber: %d", LaraItem->BoxNumber);
				break;
				
			case RendererDebugPage::WireframeMode:
				PrintDebugMessage("WIREFRAME MODE");
				break;

			default:
				break;
			}
		}
	}

	void Renderer::SwitchDebugPage(bool goBack)
	{
		int page = (int)_debugPage;
		goBack ? --page : ++page;

		if (page < (int)RendererDebugPage::None)
		{
			page = (int)RendererDebugPage::Count - 1;
		}
		else if (page > (int)RendererDebugPage::WireframeMode)
		{
			page = (int)RendererDebugPage::None;
		}

		_debugPage = (RendererDebugPage)page;
	}
}
