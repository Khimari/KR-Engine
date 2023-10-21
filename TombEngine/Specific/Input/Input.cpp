#include "framework.h"
#include "Specific/Input/Input.h"

#include <OISException.h>
#include <OISForceFeedback.h>
#include <OISInputManager.h>
#include <OISJoyStick.h>
#include <OISKeyboard.h>
#include <OISMouse.h>

#include "Game/camera.h"
#include "Game/Gui.h"
#include "Game/items.h"
#include "Game/savegame.h"
#include "Math/Math.h"
#include "Renderer/Renderer11.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/trutils.h"
#include "Specific/winmain.h"

using namespace OIS;
using namespace TEN::Gui;
using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

// Big TODO: Entire input system shouldn't be left exposed like this.

namespace TEN::Input
{
	constexpr auto AXIS_SCALE			 = 1.5f;
	constexpr auto AXIS_DEADZONE		 = 8000;
	constexpr auto AXIS_OFFSET			 = 0.2f;
	constexpr auto MOUSE_AXIS_CONSTRAINT = 100.0f;

	// OIS interfaces
	InputManager*  OisInputManager = nullptr;
	Keyboard*	   OisKeyboard	   = nullptr;
	Mouse*		   OisMouse		   = nullptr;
	JoyStick*	   OisGamepad	   = nullptr;
	ForceFeedback* OisRumble	   = nullptr;
	Effect*		   OisEffect	   = nullptr;

	// Globals
	RumbleData RumbleInfo = {};
	std::unordered_map<int, float>						KeyMap;
	std::unordered_map<InputAxisID, Vector2>			AxisMap;
	std::unordered_map<InputActionID, InputAction>		ActionMap;
	std::unordered_map<InputActionID, ActionQueueState> ActionQueueMap;

	void InitializeEffect()
	{
		OisEffect = new Effect(Effect::ConstantForce, Effect::Constant);
		OisEffect->direction = Effect::North;
		OisEffect->trigger_button = 0;
		OisEffect->trigger_interval = 0;
		OisEffect->replay_length = Effect::OIS_INFINITE;
		OisEffect->replay_delay = 0;
		OisEffect->setNumAxes(1);

		auto& pConstForce = *dynamic_cast<ConstantEffect*>(OisEffect->getForceEffect());
		pConstForce.level = 0;
		pConstForce.envelope.attackLength = 0;
		pConstForce.envelope.attackLevel = 0;
		pConstForce.envelope.fadeLength = 0;
		pConstForce.envelope.fadeLevel = 0;
	}

	void InitializeInput(HWND handle)
	{
		TENLog("Initializing input system...", LogLevel::Info);

		RumbleInfo = {};

		// Initialize key map.
		for (int i = 0; i < KEY_SLOT_COUNT; i++)
			KeyMap[i] = 0.0f;

		// Initialize input axis map.
		for (int i = 0; i < (int)InputAxisID::Count; i++)
		{
			auto inputAxis = (InputAxisID)i;
			AxisMap[inputAxis] = Vector2::Zero;
		}

		// Initialize input action and input action queue maps.
		for (int i = 0; i < (int)InputActionID::Count; i++)
		{
			auto actionID = (InputActionID)i;
			ActionMap[actionID] = InputAction(actionID);
			ActionQueueMap[actionID] = ActionQueueState::None;
		}

		try
		{
			// Use OIS ParamList since default behaviour blocks WIN key and steals mouse.
			auto paramList = ParamList{};
			auto wnd = std::ostringstream{};
			wnd << (size_t)handle;
			paramList.insert(std::make_pair(std::string("WINDOW"), wnd.str()));
			paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_FOREGROUND")));
			paramList.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_NONEXCLUSIVE")));
			paramList.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_FOREGROUND")));
			paramList.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_NONEXCLUSIVE")));

			OisInputManager = InputManager::createInputSystem(paramList);
			OisInputManager->enableAddOnFactory(InputManager::AddOn_All);

			if (OisInputManager->getNumberOfDevices(OISKeyboard) == 0)
			{
				TENLog("Keyboard not found.", LogLevel::Warning);
			}
			else
			{
				OisKeyboard = (Keyboard*)OisInputManager->createInputObject(OISKeyboard, true);
			}

			if (OisInputManager->getNumberOfDevices(OISMouse) == 0)
			{
				TENLog("Mouse not found.", LogLevel::Warning);
			}
			else
			{
				OisMouse = (Mouse*)OisInputManager->createInputObject(OISMouse, true);
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("An exception occured during input system init: " + std::string(ex.eText), LogLevel::Error);
		}

		int deviceCount = OisInputManager->getNumberOfDevices(OISJoyStick);
		if (deviceCount > 0)
		{
			TENLog("Found " + std::to_string(deviceCount) + " connected game controller" + ((deviceCount > 1) ? "s." : "."), LogLevel::Info);

			try
			{
				OisGamepad = (JoyStick*)OisInputManager->createInputObject(OISJoyStick, true);
				TENLog("Using '" + OisGamepad->vendor() + "' device for input.", LogLevel::Info);

				// Try to initialize vibration interface.
				OisRumble = (ForceFeedback*)OisGamepad->queryInterface(Interface::ForceFeedback);
				if (OisRumble != nullptr)
				{
					TENLog("Controller supports vibration.", LogLevel::Info);
					InitializeEffect();
				}

				// If controller is XInput and default bindings were successfully assigned, save configuration.
				if (ApplyDefaultXInputBindings())
				{
					g_Configuration.EnableRumble = (OisRumble != nullptr);
					g_Configuration.EnableThumbstickCamera = true;
					SaveConfiguration();
				}
			}
			catch (OIS::Exception& ex)
			{
				TENLog("An exception occured during game controller init: " + std::string(ex.eText), LogLevel::Error);
			}
		}
	}

	void DeinitializeInput()
	{
		if (OisKeyboard != nullptr)
			OisInputManager->destroyInputObject(OisKeyboard);

		if (OisMouse != nullptr)
			OisInputManager->destroyInputObject(OisMouse);

		if (OisGamepad != nullptr)
			OisInputManager->destroyInputObject(OisGamepad);

		if (OisEffect != nullptr)
		{
			delete OisEffect;
			OisEffect = nullptr;
		}

		InputManager::destroyInputSystem(OisInputManager);
	}

	void ClearInputData()
	{
		for (auto& [key, value] : KeyMap)
			value = 0.0f;

		for (auto& [axisID, axis] : AxisMap)
			axis = Vector2::Zero;
	}

	void ApplyActionQueue()
	{
		for (int i = 0; i < (int)InputActionID::Count; i++)
		{
			auto actionID = (InputActionID)i;
			switch (ActionQueueMap[actionID])
			{
			default:
			case ActionQueueState::None:
				break;

			case ActionQueueState::Update:
				ActionMap[actionID].Update(true);
				break;

			case ActionQueueState::Clear:
				ActionMap[actionID].Clear();
				break;
			}
		}
	}

	void ClearActionQueue()
	{
		for (auto& [actionID, queue] : ActionQueueMap)
			queue = ActionQueueState::None;
	}

	static bool TestBoundKey(int key)
	{
		for (int i = 1; i >= 0; i--)
		{
			auto deviceID = (InputDeviceID)i;
			for (int j = 0; j < (int)InputActionID::Count; j++)
			{
				auto actionID = (InputActionID)j;
				if (g_Bindings.GetBoundKey(deviceID, actionID) != KC_UNASSIGNED)
					return true;
			}
		}

		return false;
	}

	// Merge right and left Ctrl, Shift, and Alt keys.
	int WrapSimilarKeys(int source)
	{
		switch (source)
		{
		case KC_LCONTROL:
			return KC_RCONTROL;

		case KC_LSHIFT:
			return KC_RSHIFT;

		case KC_LMENU:
			return KC_RMENU;
		}

		return source;
	}

	void DefaultConflict()
	{
		for (int i = 0; i < (int)InputActionID::Count; i++)
		{
			auto actionID = (InputActionID)i;

			g_Bindings.SetConflict(actionID, false);

			int key = g_Bindings.GetBoundKey(InputDeviceID::KeyboardMouse, (InputActionID)i);
			for (int j = 0; j < (int)InputActionID::Count; j++)
			{
				if (key != g_Bindings.GetBoundKey(InputDeviceID::Custom, (InputActionID)j))
					continue;

				g_Bindings.SetConflict(actionID, true);
				break;
			}
		}
	}

	static void SetDiscreteAxisValues(unsigned int keyID)
	{
		for (int i = 0; i < (int)InputDeviceID::Count; i++)
		{
			auto deviceID = (InputDeviceID)i;
			if (g_Bindings.GetBoundKey(deviceID, In::Forward) == keyID)
			{
				AxisMap[InputAxisID::Move].y = 1.0f;
			}
			else if (g_Bindings.GetBoundKey(deviceID, In::Back) == keyID)
			{
				AxisMap[InputAxisID::Move].y = -1.0f;
			}
			else if (g_Bindings.GetBoundKey(deviceID, In::Left) == keyID)
			{
				AxisMap[InputAxisID::Move].x = -1.0f;
			}
			else if (g_Bindings.GetBoundKey(deviceID, In::Right) == keyID)
			{
				AxisMap[InputAxisID::Move].x = 1.0f;
			}
		}
	}

	static void ReadKeyboard()
	{
		if (OisKeyboard == nullptr)
			return;

		try
		{
			OisKeyboard->capture();

			// Poll keyboard keys.
			for (int i = 0; i < KEYBOARD_KEY_COUNT; i++)
			{
				if (!OisKeyboard->isKeyDown((KeyCode)i))
					continue;

				int key = WrapSimilarKeys(i);
				KeyMap[key] = 1.0f;

				// Interpret discrete directional keypresses as analog axis values.
				SetDiscreteAxisValues(key);
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll keyboard input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}

	static void ReadMouse()
	{
		if (OisMouse == nullptr)
			return;

		try
		{
			OisMouse->capture();
			auto& state = OisMouse->getMouseState();

			// Update active area resolution.
			auto screenRes = g_Renderer.GetScreenResolution();
			state.width = screenRes.x;
			state.height = screenRes.y;

			// Poll mouse buttons.
			for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
				KeyMap[KEYBOARD_KEY_COUNT + i] = state.buttonDown((MouseButtonID)i) ? 1.0f : 0.0f;

			// Register multiple directional keypresses mapped to mouse axes.
			int baseIndex = KEYBOARD_KEY_COUNT + MOUSE_BUTTON_COUNT;
			for (int pass = 0; pass < (MOUSE_AXIS_COUNT * 2); pass++)
			{
				switch (pass)
				{
				// Mouse X-
				case 0:
					if (state.X.rel >= 0)
						continue;
					break;

				// Mouse X+
				case 1:
					if (state.X.rel <= 0)
						continue;
					break;

				// Mouse Y-
				case 2:
					if (state.Y.rel >= 0)
						continue;
					break;

				// Mouse Y+
				case 3:
					if (state.Y.rel <= 0)
						continue;
					break;

				// Mouse Z-
				case 4:
					if (state.Z.rel >= 0)
						continue;
					break;

				// Mouse Z+
				case 5:
					if (state.Z.rel <= 0)
						continue;
					break;
				}

				KeyMap[baseIndex + pass] = 1.0f;

				// Interpret discrete directional keypresses as mouse axis values.
				SetDiscreteAxisValues(baseIndex + pass);
			}

			// TODO: Will need to take screen aspect ratio into account once mouse axes start being used. -- Sezz 2023.10.20
			// Normalize raw mouse axis values to range [-1.0f, 1.0f].
			auto rawAxes = Vector2(state.X.rel, state.Y.rel);
			auto normAxes = Vector2(
				(((rawAxes.x - -SCREEN_SPACE_RES.x) * 2) / (SCREEN_SPACE_RES.x - -SCREEN_SPACE_RES.x)) - 1.0f,
				(((rawAxes.y - -SCREEN_SPACE_RES.y) * 2) / (SCREEN_SPACE_RES.y - -SCREEN_SPACE_RES.y)) - 1.0f);

			// Apply sensitivity and smoothing.
			float sensitivity = (g_Configuration.MouseSensitivity * 0.1f) + 0.4f;
			float smoothing = 1.0f - (g_Configuration.MouseSmoothing * 0.1f);
			normAxes *= sensitivity * smoothing;

			// Set mouse axis values.
			AxisMap[InputAxisID::Mouse] = normAxes;
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll mouse input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}
	
	static void ReadGameController()
	{
		if (OisGamepad == nullptr)
			return;

		try
		{
			OisGamepad->capture();
			const auto& state = OisGamepad->getJoyStickState();

			// Poll buttons.
			for (int key = 0; key < state.mButtons.size(); key++)
				KeyMap[KEYBOARD_KEY_COUNT + MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2) + key] = state.mButtons[key] ? 1.0f : 0.0f;

			// Poll axes.
			for (int axis = 0; axis < state.mAxes.size(); axis++)
			{
				// NOTE: Anything above 6 existing XBOX/PS controller axes not supported (2 sticks plus 2 triggers).
				if (axis >= GAMEPAD_AXIS_COUNT)
					break;

				// Filter out deadzone.
				if (abs(state.mAxes[axis].abs) < AXIS_DEADZONE)
					continue;

				// Calculate raw normalized analog value (for camera).
				float normalizedValue = float(state.mAxes[axis].abs + (state.mAxes[axis].abs > 0 ? -AXIS_DEADZONE : AXIS_DEADZONE)) /
					float(SHRT_MAX - AXIS_DEADZONE);

				// Calculate scaled analog value for movement.
				// NOTE: [0.2f, 1.7f] range gives most organic rates.
				float scaledValue = (abs(normalizedValue) * AXIS_SCALE) + AXIS_OFFSET;

				// Calculate and reset discrete input slots.
				int negKey = KEYBOARD_KEY_COUNT + MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2) + GAMEPAD_BUTTON_COUNT + (axis * 2);
				int posKey = KEYBOARD_KEY_COUNT + MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2) + GAMEPAD_BUTTON_COUNT + (axis * 2) + 1;
				KeyMap[negKey] = (normalizedValue > 0) ? abs(normalizedValue) : 0.0f;
				KeyMap[posKey] = (normalizedValue < 0) ? abs(normalizedValue) : 0.0f;

				// Determine discrete input registering based on analog value.
				int usedKey = (normalizedValue > 0) ? negKey : posKey;

				// Register analog input in certain direction.
				// If axis is bound as directional controls, register axis as directional input.
				// Otherwise, register as camera movement input (for future).
				// NOTE: abs() operations are needed to avoid issues with inverted axes on different controllers.

				if (g_Bindings.GetBoundKey(InputDeviceID::Custom, In::Forward) == usedKey)
				{
					AxisMap[InputAxisID::Move].y = abs(scaledValue);
				}
				else if (g_Bindings.GetBoundKey(InputDeviceID::Custom, In::Back) == usedKey)
				{
					AxisMap[InputAxisID::Move].y = -abs(scaledValue);
				}
				else if (g_Bindings.GetBoundKey(InputDeviceID::Custom, In::Left)  == usedKey)
				{
					AxisMap[InputAxisID::Move].x = -abs(scaledValue);
				}
				else if (g_Bindings.GetBoundKey(InputDeviceID::Custom, In::Right) == usedKey)
				{
					AxisMap[InputAxisID::Move].x = abs(scaledValue);
				}
				else if (!TestBoundKey(usedKey))
				{
					if ((axis % 2) == 0)
					{
						AxisMap[InputAxisID::Camera].y = normalizedValue;
					}
					else
					{
						AxisMap[InputAxisID::Camera].x = normalizedValue;
					}
				}
			}

			// Poll POVs.
			// NOTE: Controllers usually have one, but scan all just in case.
			for (int pov = 0; pov < GAMEPAD_POV_AXIS_COUNT; pov++)
			{
				if (state.mPOV[pov].direction == Pov::Centered)
					continue;

				// Register multiple directional keypresses mapped to analog axes.
				int baseIndex = KEYBOARD_KEY_COUNT + MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2) + GAMEPAD_BUTTON_COUNT + (GAMEPAD_AXIS_COUNT * 2);
				for (int pass = 0; pass < GAMEPAD_POV_AXIS_COUNT; pass++)
				{
					unsigned int index = KEYBOARD_KEY_COUNT + MOUSE_BUTTON_COUNT + (MOUSE_AXIS_COUNT * 2) + GAMEPAD_BUTTON_COUNT + (GAMEPAD_AXIS_COUNT * 2);

					switch (pass)
					{
					// D-Pad Up
					case 0:
						if ((state.mPOV[pov].direction & Pov::North) == 0)
							continue;
						break;

					// D-Pad Down
					case 1:
						if ((state.mPOV[pov].direction & Pov::South) == 0)
							continue;
						break;

					// D-Pad Left
					case 2:
						if ((state.mPOV[pov].direction & Pov::West) == 0)
							continue;
						break;

					// D-Pad Right
					case 3:
						if ((state.mPOV[pov].direction & Pov::East) == 0)
							continue;
						break;
					}

					index += pass;
					KeyMap[index] = 1.0f;
					SetDiscreteAxisValues(index);
				}
			}
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Unable to poll game controller input: " + std::string(ex.eText), LogLevel::Warning);
		}
	}

	static float Key(InputActionID actionID)
	{
		for (int i = (int)InputDeviceID::Count - 1; i >= 0; i--)
		{
			auto deviceID = (InputDeviceID)i;

			if (deviceID == InputDeviceID::KeyboardMouse && g_Bindings.TestConflict(actionID))
				continue;

			int key = g_Bindings.GetBoundKey((InputDeviceID)i, actionID);
			if (KeyMap[key] != 0.0f)
				return KeyMap[key];
		}

		return 0.0f;
	}

	void SolveActionCollisions()
	{
		// Block simultaneous Left+Right actions.
		if (IsHeld(In::Left) && IsHeld(In::Right))
		{
			ClearAction(In::Left);
			ClearAction(In::Right);
		}
	}

	static void HandleHotkeyActions()
	{
		// Save screenshot.
		static bool dbScreenshot = true;
		if (KeyMap[KC_SYSRQ] && dbScreenshot)
			g_Renderer.SaveScreenshot();
		dbScreenshot = !KeyMap[KC_SYSRQ];

		// Toggle fullscreen.
		static bool dbFullscreen = true;
		if ((KeyMap[KC_LMENU] || KeyMap[KC_RMENU]) && KeyMap[KC_RETURN] && dbFullscreen)
		{
			g_Configuration.EnableWindowedMode = !g_Configuration.EnableWindowedMode;
			SaveConfiguration();
			g_Renderer.ToggleFullScreen();
		}
		dbFullscreen = !((KeyMap[KC_LMENU] || KeyMap[KC_RMENU]) && KeyMap[KC_RETURN]);

		if (!DebugMode)
			return;

		// Switch debug page.
		static bool dbDebugPage = true;
		if ((KeyMap[KC_F10] || KeyMap[KC_F11]) && dbDebugPage)
			g_Renderer.SwitchDebugPage(KeyMap[KC_F10]);
		dbDebugPage = !(KeyMap[KC_F10] || KeyMap[KC_F11]);
	}

	static void UpdateRumble()
	{
		if (!OisRumble || !OisEffect || !RumbleInfo.Power)
			return;

		RumbleInfo.Power -= RumbleInfo.FadeSpeed;

		// Don't update effect too frequently if its value hasn't changed much.
		if (RumbleInfo.Power >= 0.2f &&
			RumbleInfo.LastPower - RumbleInfo.Power < 0.1f)
			return;

		if (RumbleInfo.Power <= 0.0f)
		{
			StopRumble();
			return;
		}

		try
		{
			auto& force = *dynamic_cast<ConstantEffect*>(OisEffect->getForceEffect());
			force.level = RumbleInfo.Power * 10000;

			switch (RumbleInfo.Mode)
			{
			case RumbleMode::Left:
				OisEffect->direction = Effect::EDirection::West;
				break;

			case RumbleMode::Right:
				OisEffect->direction = Effect::EDirection::East;
				break;

			case RumbleMode::Both:
				OisEffect->direction = Effect::EDirection::North;
				break;
			}

			OisRumble->upload(OisEffect);
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Error updating vibration effect: " + std::string(ex.eText), LogLevel::Error);
		}

		RumbleInfo.LastPower = RumbleInfo.Power;
	}

	void UpdateInputActions(ItemInfo* item, bool applyQueue)
	{
		ClearInputData();
		UpdateRumble();
		ReadKeyboard();
		ReadMouse();
		ReadGameController();
		DefaultConflict();

		// Update action map.
		for (auto& [actionID, action] : ActionMap)
			action.Update(Key(action.GetID()));

		if (applyQueue)
			ApplyActionQueue();

		// Additional handling.
		HandleHotkeyActions();
		SolveActionCollisions();
	}

	void ClearAllActions()
	{
		for (auto& [actionID, action] : ActionMap)
			action.Clear();

		for (auto& [actionID, queue] : ActionQueueMap)
			queue = ActionQueueState::None;
	}

	void Rumble(float power, float delayInSec, RumbleMode mode)
	{
		if (!g_Configuration.EnableRumble)
			return;

		power = std::clamp(power, 0.0f, 1.0f);

		if (power == 0.0f || RumbleInfo.Power)
			return;

		RumbleInfo.FadeSpeed = power / (delayInSec * FPS);
		RumbleInfo.Power = power + RumbleInfo.FadeSpeed;
		RumbleInfo.LastPower = RumbleInfo.Power;
	}

	void StopRumble()
	{
		if (!OisRumble || !OisEffect)
			return;

		try
		{
			OisRumble->remove(OisEffect);
		}
		catch (OIS::Exception& ex)
		{
			TENLog("Error when stopping vibration effect: " + std::string(ex.eText), LogLevel::Error);
		}

		RumbleInfo = {};
	}

	static void ApplyBindings(const BindingProfile& set)
	{
		g_Bindings.SetBindingProfile(InputDeviceID::Custom, set);
	}

	void ApplyDefaultBindings()
	{
		ApplyBindings(BindingManager::DEFAULT_XBOX_BINDING_PROFILE);
		ApplyDefaultXInputBindings();
	}

	bool ApplyDefaultXInputBindings()
	{
		if (!OisGamepad)
			return false;

		for (int i = 0; i < (int)InputActionID::Count; i++)
		{
			auto actionID = (InputActionID)i;

			int defaultKey = g_Bindings.GetBoundKey(InputDeviceID::KeyboardMouse, actionID);
			int userKey = g_Bindings.GetBoundKey(InputDeviceID::Custom, actionID);

			if (userKey != KC_UNASSIGNED &&
				userKey != defaultKey)
			{
				return false;
			}
		}

		auto vendor = TEN::Utils::ToLower(OisGamepad->vendor());
		if (vendor.find("xbox") != std::string::npos || vendor.find("xinput") != std::string::npos)
		{
			ApplyBindings(BindingManager::DEFAULT_XBOX_BINDING_PROFILE);
			g_Configuration.Bindings = g_Bindings.GetBindingProfile(InputDeviceID::Custom);

			// Additionally turn on thumbstick camera and vibration.
			g_Configuration.EnableRumble = true;
			g_Configuration.EnableThumbstickCamera = true;

			return true;
		}
		else
		{
			return false;
		}
	}

	Vector2 GetCursorDisplayPosition()
	{
		const auto& state = OisMouse->getMouseState();

		auto areaRes = Vector2(state.width, state.height);
		auto areaPos = Vector2(state.X.abs, state.Y.abs);
		return (SCREEN_SPACE_RES * (areaPos / areaRes));
	}

	void ClearAction(InputActionID actionID)
	{
		ActionMap[actionID].Clear();
	}

	bool NoAction()
	{
		for (const auto& [actionID, action] : ActionMap)
		{
			if (action.IsHeld())
				return false;
		}

		return true;
	}

	bool IsClicked(InputActionID actionID)
	{
		return ActionMap[actionID].IsClicked();
	}

	bool IsHeld(InputActionID actionID, float delayInSec)
	{
		return ActionMap[actionID].IsHeld(delayInSec);
	}

	bool IsPulsed(InputActionID actionID, float delayInSec, float initialDelayInSec)
	{
		return ActionMap[actionID].IsPulsed(delayInSec, initialDelayInSec);
	}

	bool IsReleased(InputActionID actionID, float maxDelayInSec)
	{
		return ActionMap[actionID].IsReleased(maxDelayInSec);
	}

	float GetActionValue(InputActionID actionID)
	{
		return ActionMap[actionID].GetValue();
	}

	float GetActionTimeActive(InputActionID actionID)
	{
		return ActionMap[actionID].GetTimeActive();
	}

	float GetActionTimeInactive(InputActionID actionID)
	{
		return ActionMap[actionID].GetTimeInactive();
	}

	bool IsDirectionalActionHeld()
	{
		return (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right));
	}

	bool IsWakeActionHeld()
	{
		if (IsDirectionalActionHeld() || IsHeld(In::StepLeft) || IsHeld(In::StepRight) ||
			IsHeld(In::Walk) || IsHeld(In::Jump) || IsHeld(In::Sprint) || IsHeld(In::Roll) || IsHeld(In::Crouch) ||
			IsHeld(In::Draw) || IsHeld(In::Flare) || IsHeld(In::Action))
		{
			return true;
		}

		return false;
	}

	bool IsOpticActionHeld()
	{
		return (IsDirectionalActionHeld() || IsHeld(In::Action) || IsHeld(In::Crouch) || IsHeld(In::Sprint));
	}
}
