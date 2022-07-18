#include "framework.h"
#include "Specific/input.h"

#include <OISException.h>
#include <OISForceFeedback.h>
#include <OISInputManager.h>
#include <OISJoyStick.h>
#include <OISKeyboard.h>

#include "Game/camera.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Renderer/Renderer11.h"
#include "Game/savegame.h"
#include "Sound/sound.h"

using namespace OIS;
using TEN::Renderer::g_Renderer;

namespace TEN::Input
{
	const char* g_KeyNames[] =
	{
			"<None>",		"Esc",			"1",			"2",			"3",			"4",			"5",			"6",
			"7",			"8",			"9",			"0",			"-",			"+",			"Back",			"Tab",
			"Q",			"W",			"E",			"R",			"T",			"Y",			"U",			"I",
			"O",			"P",			"<",			">",			"Enter",		"Ctrl",			"A",			"S",
			"D",			"F",			"G",			"H",			"J",			"K",			"L",			";",
			"'",			"`",			"Shift",		"#",			"Z",			"X",			"C",			"V",
			"B",			"N",			"M",			",",			".",			"/",			"Shift",		"Pad X",
			"Alt",			"Space",		"Caps Lock",	NULL,			NULL,			NULL,			NULL,			NULL,

			NULL,			NULL,			NULL,			NULL,			NULL,			"Num Lock",		"Scroll Lock",	"Pad 7",
			"Pad 8",		"Pad 9",		"Pad -",		"Pad 4",		"Pad 5",		"Pad 6",		"Pad +",		"Pad 1",
			"Pad 2",		"Pad 3",		"Pad 0",		"Pad.",			NULL,			NULL,			"\\",			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,

			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			"Pad Enter",	"Ctrl",			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			"Shift",		NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			"Pad /",		NULL,			NULL,
			"Alt",			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,

			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			"Home",
			"Up",			"Page Up",		NULL,			"Left",			NULL,			"Right",		NULL,			"End",
			"Down",			"Page Down",	"Insert",		"Del",			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,
			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,			NULL,

			"Joy 1", 		"Joy 2",		"Joy 3",		"Joy 4", 		"Joy 5",		"Joy 6", 		"Joy 7",		"Joy 8",
			"Joy 9",		"Joy 10",		"Joy 11",		"Joy 12",		"Joy 13",		"Joy 14",		"Joy 15",		"Joy 16",

			"X-",			"X+",			"Y-",			"Y+",			"Z-",			"Z+",			"W-",			"W+",
			"Joy LT",		"Joy LT",		"Joy RT",		"Joy RT",		"D-Pad Up",		"D-Pad Down",	"D-Pad Left",	"D-Pad Right"
	};

	constexpr int AXIS_DEADZONE = 8000;

	// OIS interfaces
	InputManager*  oisInputManager = nullptr;
	Keyboard*	   oisKeyboard	   = nullptr;
	JoyStick*	   oisGamepad	   = nullptr;
	ForceFeedback* oisRumble	   = nullptr;
	Effect*		   oisEffect	   = nullptr;

	// Rumble functionality
	RumbleData rumbleData = {};

	// Globals
	int DbInput;
	int TrInput;

	std::vector<InputAction> ActionMap;
	std::vector<bool>		 KeyMap;
	std::vector<float>		 AxisMap;

	// Default keys
	bool ConflictingKeys[KEY_COUNT];
	short KeyboardLayout[2][KEY_COUNT] =
	{
		{
			KC_UP, KC_DOWN, KC_LEFT, KC_RIGHT, KC_PERIOD, KC_SLASH, KC_RSHIFT, KC_RMENU, KC_RCONTROL, KC_SPACE, KC_COMMA, KC_NUMPAD0, KC_END, KC_ESCAPE, KC_P, KC_PGUP, KC_PGDOWN,
			/*KC_RCONTROL, KC_DOWN, KC_SLASH, KC_RSHIFT, KC_RMENU, KC_SPACE*/
		},
		{
			KC_UP, KC_DOWN, KC_LEFT, KC_RIGHT, KC_PERIOD, KC_SLASH, KC_RSHIFT, KC_RMENU, KC_RCONTROL, KC_SPACE, KC_COMMA, KC_NUMPAD0, KC_END, KC_ESCAPE, KC_P, KC_PGUP, KC_PGDOWN,
			/*KC_RCONTROL, KC_DOWN, KC_SLASH, KC_RSHIFT, KC_RMENU, KC_SPACE*/
		}
	};

	void InitialiseEffect()
	{
		oisEffect = new Effect(Effect::ConstantForce, Effect::Constant);
		oisEffect->direction = Effect::North;
		oisEffect->trigger_button = 0;
		oisEffect->trigger_interval = 0;
		oisEffect->replay_length = Effect::OIS_INFINITE;
		oisEffect->replay_delay = 0;
		oisEffect->setNumAxes(1);

		auto* pConstForce = dynamic_cast<ConstantEffect*>(oisEffect->getForceEffect());
		pConstForce->level = 0;
		pConstForce->envelope.attackLength = 0;
		pConstForce->envelope.attackLevel = 0;
		pConstForce->envelope.fadeLength = 0;
		pConstForce->envelope.fadeLevel = 0;
	}

	void InitialiseInput(HWND handle)
	{
		TENLog("Initializing input system...", LogLevel::Info);

		unsigned int v = oisInputManager->getVersionNumber();
		TENLog("OIS Version: " + std::to_string(v >> 16) + "." + std::to_string((v >> 8) & 0x000000FF) + "." + std::to_string(v & 0x000000FF), LogLevel::Info);

		KeyMap.resize(MAX_INPUT_SLOTS);
		AxisMap.resize(InputAxis::Count);

		rumbleData = {};

		try
		{
			oisInputManager = InputManager::createInputSystem((size_t)handle);
			oisInputManager->enableAddOnFactory(InputManager::AddOn_All);

			if (oisInputManager->getNumberOfDevices(OISKeyboard) == 0)
				TENLog("Keyboard not found!", LogLevel::Warning);
			else
				oisKeyboard = (Keyboard*)oisInputManager->createInputObject(OISKeyboard, true);
		}
		catch (OIS::Exception& ex)
		{
			TENLog("An exception occured during input system init: " + std::string(ex.eText), LogLevel::Error);
		}

		int numDevices = oisInputManager->getNumberOfDevices(OISJoyStick);
		if (numDevices > 0)
		{
			TENLog("Found " + std::to_string(numDevices) + " connected game controller" + (numDevices > 1 ? "s." : "."), LogLevel::Info);

			try
			{
				oisGamepad = (JoyStick*)oisInputManager->createInputObject(OISJoyStick, true);
				TENLog("Using '" + oisGamepad->vendor() + "' device for input.", LogLevel::Info);

				// Try to initialise vibration interface
				oisRumble = (ForceFeedback*)oisGamepad->queryInterface(Interface::ForceFeedback);
				if (oisRumble)
				{
					TENLog("Controller supports vibration.", LogLevel::Info);
					InitialiseEffect();
				}
			}
			catch (OIS::Exception& ex)
			{
				TENLog("An exception occured during game controller init: " + std::string(ex.eText), LogLevel::Error);
			}
		}

		// Initialise input action map.
		for (int i = 0; i < (int)InputActionID::Count; i++)
			ActionMap.push_back(InputAction((InputActionID)i));
	}

	void DeInitialiseInput()
	{
		if (oisKeyboard)
			oisInputManager->destroyInputObject(oisKeyboard);

		if (oisGamepad)
			oisInputManager->destroyInputObject(oisGamepad);

		if (oisEffect)
		{
			delete oisEffect;
			oisEffect = nullptr;
		}

		InputManager::destroyInputSystem(oisInputManager);
	}

	void ClearInputData()
	{
		for (int key = 0; key < KeyMap.size(); key++)
			KeyMap[key] = false;

		for (int axis = 0; axis < InputAxis::Count; axis++)
			AxisMap[axis] = 0.0f;
	}

	bool LayoutContainsIndex(unsigned int index)
	{
		for (int l = 0; l < 2; l++)
		{
			for (int i = 0; i < KEY_COUNT; i++)
			{
				if (KeyboardLayout[l][i] == index)
					return true;
			}
		}

		return false;
	}

	void DefaultConflict()
	{
		for (int i = 0; i < KEY_COUNT; i++)
		{
			short key = KeyboardLayout[0][i];

			ConflictingKeys[i] = false;

			for (int j = 0; j < KEY_COUNT; j++)
			{
				if (key != KeyboardLayout[1][j])
					continue;

				ConflictingKeys[i] = true;
				break;
			}
		}
	}

	void SetDiscreteAxisValues(unsigned int index)
	{
		for (int layout = 0; layout <= 1; layout++)
		{
			if (KeyboardLayout[layout][KEY_FORWARD] == index)
				AxisMap[(unsigned int)InputAxis::MoveVertical] = 1.0f;
			else if (KeyboardLayout[layout][KEY_BACK] == index)
				AxisMap[(unsigned int)InputAxis::MoveVertical] = -1.0f;
			else if (KeyboardLayout[layout][KEY_LEFT] == index)
				AxisMap[(unsigned int)InputAxis::MoveHorizontal] = -1.0f;
			else if (KeyboardLayout[layout][KEY_RIGHT] == index)
				AxisMap[(unsigned int)InputAxis::MoveHorizontal] = 1.0f;
		}
	}

	void ReadGameController()
	{
		if (!oisGamepad)
			return;

		try
		{
			// Poll gamepad
			oisGamepad->capture();
			const JoyStickState& state = oisGamepad->getJoyStickState();

			// Scan buttons
			for (int key = 0; key < state.mButtons.size(); key++)
				KeyMap[MAX_KEYBOARD_KEYS + key] = state.mButtons[key];

			// Scan axes
			for (int axis = 0; axis < state.mAxes.size(); axis++)
			{
				// We don't support anything above 6 existing XBOX/PS controller axes (two sticks plus triggers)
				if (axis >= MAX_GAMEPAD_AXES)
					break;

				// Filter out deadzone
				if (abs(state.mAxes[axis].abs) < AXIS_DEADZONE)
					continue;

				// Calculate raw normalized analog value (for camera)
				float normalizedValue = (float)(state.mAxes[axis].abs + (state.mAxes[axis].abs > 0 ? -AXIS_DEADZONE : AXIS_DEADZONE))
					/ (float)(std::numeric_limits<short>::max() - AXIS_DEADZONE);

				// Calculate scaled analog value (for movement).
				// Minimum value of 0.2f and maximum value of 1.7f is empirically the most organic rate from tests.
				float scaledValue = abs(normalizedValue) * 1.5f + 0.2f;

				// Calculate and reset discrete input slots
				unsigned int negKeyIndex = MAX_KEYBOARD_KEYS + MAX_GAMEPAD_KEYS + (axis * 2);
				unsigned int posKeyIndex = MAX_KEYBOARD_KEYS + MAX_GAMEPAD_KEYS + (axis * 2) + 1;
				KeyMap[negKeyIndex] = false;
				KeyMap[posKeyIndex] = false;

				// Decide on the discrete input registering based on analog value
				unsigned int usedIndex = normalizedValue > 0 ? negKeyIndex : posKeyIndex;
				KeyMap[usedIndex] = true;

				// Register analog input in certain direction.
				// If axis is bound as directional controls, register axis as directional input.
				// Otherwise, register as camera movement input (for future).
				// NOTE: abs() operations are needed to avoid issues with inverted axes on different controllers.

				if (KeyboardLayout[1][KEY_FORWARD] == usedIndex)
					AxisMap[InputAxis::MoveVertical] = abs(scaledValue);
				else if (KeyboardLayout[1][KEY_BACK] == usedIndex)
					AxisMap[InputAxis::MoveVertical] = -abs(scaledValue);
				else if (KeyboardLayout[1][KEY_LEFT] == usedIndex)
					AxisMap[InputAxis::MoveHorizontal] = -abs(scaledValue);
				else if (KeyboardLayout[1][KEY_RIGHT] == usedIndex)
					AxisMap[InputAxis::MoveHorizontal] = abs(scaledValue);
				else if (!LayoutContainsIndex(usedIndex))
				{
					unsigned int camAxisIndex = (unsigned int)std::clamp((unsigned int)InputAxis::CameraVertical + axis % 2,
						(unsigned int)InputAxis::CameraVertical,
						(unsigned int)InputAxis::CameraHorizontal);
					AxisMap[camAxisIndex] = normalizedValue;
				}
			}

			// Scan POVs (controllers usually have one, but let's scan all of them for paranoia)
			for (int pov = 0; pov < 4; pov++)
			{
				if (state.mPOV[pov].direction == Pov::Centered)
					continue;

				// Do 4 passes; every pass checks every POV direction. For every direction,
				// separate keypress is registered. This is needed to allow multiple directions
				// pressed at the same time.
				for (int pass = 0; pass < 4; pass++)
				{
					unsigned int index = MAX_KEYBOARD_KEYS + MAX_GAMEPAD_KEYS + MAX_GAMEPAD_AXES * 2;

					switch (pass)
					{
					case 0:
						if (!(state.mPOV[pov].direction & Pov::North))
							continue;
						break;

					case 1:
						if (!(state.mPOV[pov].direction & Pov::South))
							continue;
						break;

					case 2:
						if (!(state.mPOV[pov].direction & Pov::West))
							continue;
						break;

					case 3:
						if (!(state.mPOV[pov].direction & Pov::East))
							continue;
						break;
					}

					index += pass;
					KeyMap[index] = true;
					SetDiscreteAxisValues(index);
				}
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll game controller input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}

	void ReadKeyboard()
	{
		if (!oisKeyboard)
			return;

		try
		{
			oisKeyboard->capture();

			for (int i = 0; i < MAX_KEYBOARD_KEYS; i++)
			{
				if (!oisKeyboard->isKeyDown((KeyCode)i))
				{
					KeyMap[i] = false;
					continue;
				}

				KeyMap[i] = true;

				// Register directional discrete keypresses as max analog axis values
				SetDiscreteAxisValues(i);
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll keyboard input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}

	bool Key(int number)
	{
		for (int layout = 1; layout >= 0; layout--)
		{
			short key = KeyboardLayout[layout][number];

			if (KeyMap[key])
				return true;

			switch (key)
			{
			case KC_RCONTROL:
				return KeyMap[KC_LCONTROL];

			case KC_LCONTROL:
				return KeyMap[KC_RCONTROL];

			case KC_RSHIFT:
				return KeyMap[KC_LSHIFT];

			case KC_LSHIFT:
				return KeyMap[KC_RSHIFT];

			case KC_RMENU:
				return KeyMap[KC_LMENU];

			case KC_LMENU:
				return KeyMap[KC_RMENU];
			}

			if (ConflictingKeys[number])
				return false;
		}

		return false;
	}

	void SolveInputCollisions()
	{
		// Block roll in binocular mode (TODO: Is it needed?)
		if (IsHeld(In::Roll) && BinocularRange)
			ClearInput(In::Roll);

		// Block simultaneous LEFT+RIGHT input.
		if (IsHeld(In::Left) && IsHeld(In::Right))
		{
			ClearInput(In::Left);
			ClearInput(In::Right);
		}

		if (Lara.Inventory.IsBusy)
		{
			//lInput &= (IN_FORWARD | IN_BACK | IN_LEFT | IN_RIGHT | IN_OPTION | IN_LOOK | IN_PAUSE);

			if (IsHeld(In::Forward) && IsHeld(In::Back))
			{
				ClearInput(In::Forward);
				ClearInput(In::Back);
			}
		}
	}

	void HandleLaraHotkeys()
	{
		// Handle hardcoded action-to-key mappings.
		ActionMap[(int)In::Select].Update((KeyMap[KC_RETURN] || Key(KEY_ACTION)) ? 1.0f : 0.0f);
		ActionMap[(int)In::Deselect].Update((KeyMap[KC_ESCAPE] || Key(KEY_DRAW)) ? 1.0f : 0.0f);
		ActionMap[(int)In::Save].Update(KeyMap[KC_F5] ? 1.0f : 0.0f);
		ActionMap[(int)In::Load].Update(KeyMap[KC_F6] ? 1.0f : 0.0f);

		// Handle look switch when locked onto an entity.
		if (Lara.Control.HandStatus == HandStatus::WeaponReady &&
			Lara.TargetEntity != nullptr)
		{
			if (IsHeld(In::Look))
			{
				ActionMap[(int)In::SwitchTarget].Update(1.0f);
				ActionMap[(int)In::Look].Clear();
			}
		}

		// Handle flares.
		if (IsClicked(In::Flare))
		{
			if (LaraItem->Animation.ActiveState == LS_CRAWL_FORWARD ||
				LaraItem->Animation.ActiveState == LS_CRAWL_TURN_LEFT ||
				LaraItem->Animation.ActiveState == LS_CRAWL_TURN_RIGHT ||
				LaraItem->Animation.ActiveState == LS_CRAWL_BACK ||
				LaraItem->Animation.ActiveState == LS_CRAWL_TO_HANG ||
				LaraItem->Animation.ActiveState == LS_CRAWL_TURN_180)
			{
				SoundEffect(SFX_TR4_LARA_NO_ENGLISH, nullptr, SoundEnvironment::Always);
			}
		}

		// Handle weapon hotkeys.

		if (KeyMap[KC_1] && Lara.Weapons[(int)LaraWeaponType::Pistol].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::Pistol;

		if (KeyMap[KC_2] && Lara.Weapons[(int)LaraWeaponType::Shotgun].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::Shotgun;

		if (KeyMap[KC_3] && Lara.Weapons[(int)LaraWeaponType::Revolver].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::Revolver;

		if (KeyMap[KC_4] && Lara.Weapons[(int)LaraWeaponType::Uzi].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::Uzi;

		if (KeyMap[KC_5] && Lara.Weapons[(int)LaraWeaponType::HarpoonGun].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::HarpoonGun;

		if (KeyMap[KC_6] && Lara.Weapons[(int)LaraWeaponType::HK].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::HK;

		if (KeyMap[KC_7] && Lara.Weapons[(int)LaraWeaponType::RocketLauncher].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::RocketLauncher;

		if (KeyMap[KC_8] && Lara.Weapons[(int)LaraWeaponType::GrenadeLauncher].Present)
			Lara.Control.Weapon.RequestGunType = LaraWeaponType::GrenadeLauncher;

		// Handle medipack hotkeys.
		static int medipackTimeout = 0;
		if (KeyMap[KC_0] || KeyMap[KC_9])
		{
			if (!medipackTimeout)
			{
				if ((LaraItem->HitPoints > 0 && LaraItem->HitPoints < LARA_HEALTH_MAX) ||
					Lara.PoisonPotency)
				{
					bool usedMedipack = false;

					if (KeyMap[KC_0] &&
						Lara.Inventory.TotalSmallMedipacks != 0)
					{
						usedMedipack = true;

						LaraItem->HitPoints += LARA_HEALTH_MAX / 2;
						if (LaraItem->HitPoints > LARA_HEALTH_MAX)
							LaraItem->HitPoints = LARA_HEALTH_MAX;

						if (Lara.Inventory.TotalSmallMedipacks != -1)
							Lara.Inventory.TotalSmallMedipacks--;
					}
					else if (KeyMap[KC_9] &&
						Lara.Inventory.TotalLargeMedipacks != 0)
					{
						usedMedipack = true;
						LaraItem->HitPoints = LARA_HEALTH_MAX;

						if (Lara.Inventory.TotalLargeMedipacks != -1)
							Lara.Inventory.TotalLargeMedipacks--;
					}

					if (usedMedipack)
					{
						Lara.PoisonPotency = 0;
						SoundEffect(SFX_TR4_MENU_MEDI, nullptr, SoundEnvironment::Always);
						Statistics.Game.HealthUsed++;
					}

					medipackTimeout = 15;
				}
			}
		}
		else if (medipackTimeout != 0)
			medipackTimeout--;

		// Handle debug page switches.
		static int debugTimeout = 0;
		if (KeyMap[KC_F10] || KeyMap[KC_F11])
		{
			if (!debugTimeout)
			{
				debugTimeout = 1;
				g_Renderer.SwitchDebugPage(KeyMap[KC_F10]);
			}
		}
		else
			debugTimeout = 0;
	}

	void UpdateRumble()
	{
		if (!oisRumble || !oisEffect || !rumbleData.Power)
			return;

		rumbleData.Power -= rumbleData.FadeSpeed;

		// Don't update effect too frequently if its value isn't changed too much
		if (rumbleData.Power >= 0.2f &&
			rumbleData.LastPower - rumbleData.Power < 0.1f)
			return;

		if (rumbleData.Power <= 0.0f)
		{
			StopRumble();
			return;
		}

		try
		{
			auto* force = dynamic_cast<ConstantEffect*>(oisEffect->getForceEffect());
			force->level = rumbleData.Power * 10000;

			switch (rumbleData.Mode)
			{
			case RumbleMode::Left:
				oisEffect->direction = Effect::EDirection::West;
				break;

			case RumbleMode::Right:
				oisEffect->direction = Effect::EDirection::East;
				break;

			case RumbleMode::Both:
				oisEffect->direction = Effect::EDirection::North;
				break;
			}

			oisRumble->upload(oisEffect);
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Error updating vibration effect: " + std::string(ex.eText), LogLevel::Error);
		}

		rumbleData.LastPower = rumbleData.Power;
	}

	bool UpdateInputActions()
	{
		ClearInputData();
		UpdateRumble();
		ReadKeyboard();
		ReadGameController();

		// Clear legacy bitfields.
		DbInput = NULL;
		TrInput = NULL;

		// Update input action map (mappable actions only).
		for (int i = 0; i < KEY_COUNT; i++)
			ActionMap[i].Update(Key(i) ? 1.0f : 0.0f); // TODO: Poll analog value of key. Any key can potentially be a trigger.

		// Additional handling.
		HandleLaraHotkeys();
		SolveInputCollisions();

		// Port actions back to legacy bitfields.
		for (auto& action : ActionMap)
		{
			int inputBit = 1 << (int)action.GetID();

			DbInput |= action.IsClicked() ? inputBit : NULL;
			TrInput |= action.IsHeld() ? inputBit : NULL;
		}

		// Debug display for FORWARD input.
		g_Renderer.PrintDebugMessage("Debug for FORWARD input:");
		ActionMap[(int)In::Forward].PrintDebugInfo();
		
		return true;
	}

	void ClearInputActions()
	{
		for (auto action : ActionMap)
			action.Clear();

		DbInput = NULL;
		TrInput = NULL;
	}

	void Rumble(float power, float delayInSeconds, RumbleMode mode)
	{
		if (!g_Configuration.EnableRumble)
			return;

		power = std::clamp(power, 0.0f, 1.0f);

		if (power == 0.0f || rumbleData.Power)
			return;

		rumbleData.FadeSpeed = power / (delayInSeconds * (float)FPS);
		rumbleData.Power = power + rumbleData.FadeSpeed;
		rumbleData.LastPower = rumbleData.Power;
	}

	void StopRumble()
	{
		if (!oisRumble || !oisEffect)
			return;

		try { oisRumble->remove(oisEffect); }
		catch (OIS::Exception& ex) { TENLog("Error when stopping vibration effect: " + std::string(ex.eText), LogLevel::Error); }

		rumbleData = {};
	}

	bool IsClicked(InputActionID input)
	{
		return ActionMap[(int)input].IsClicked();
	}

	bool IsPulsed(InputActionID input, float delayInSeconds, float initialDelayInSeconds)
	{
		return ActionMap[(int)input].IsPulsed(delayInSeconds, initialDelayInSeconds);
	}

	bool IsHeld(InputActionID input)
	{
		return ActionMap[(int)input].IsHeld();
	}

	bool IsReleased(InputActionID input)
	{
		return ActionMap[(int)input].IsReleased();
	}

	bool NoInput()
	{
		for (auto& action : ActionMap)
		{
			if (action.IsHeld())
				return false;
		}

		return true;
	}

	float GetInputValue(InputActionID input)
	{
		return ActionMap[(int)input].GetValue();
	}

	float GetInputTimeHeld(InputActionID input)
	{
		return ActionMap[(int)input].GetTimeHeld();
	}

	float GetInputTimeInactive(InputActionID input)
	{
		return ActionMap[(int)input].GetTimeInactive();
	}

	void ClearInput(InputActionID input)
	{
		ActionMap[(int)input].Clear();

		int inputBit = 1 << (int)input;
		DbInput &= ~inputBit;
		TrInput &= ~inputBit;
	}
}
