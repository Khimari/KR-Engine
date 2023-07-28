#pragma once
#include "Specific/Input/Bindings.h"
#include "Specific/Input/InputAction.h"
#include "Specific/Input/Keys.h"

struct ItemInfo;

namespace TEN::Input
{
	// Deprecated.
	enum InputKey
	{
		KEY_FORWARD,
		KEY_BACK,
		KEY_LEFT,
		KEY_RIGHT,
		KEY_LEFT_STEP,
		KEY_RIGHT_STEP,
		KEY_WALK,
		KEY_SPRINT,
		KEY_CROUCH,
		KEY_JUMP,
		KEY_ROLL,
		KEY_ACTION,
		KEY_DRAW,
		KEY_LOOK,

		KEY_ACCELERATE,
		KEY_REVERSE,
		KEY_SPEED,
		KEY_SLOW,
		KEY_BRAKE,
		KEY_FIRE,

		KEY_FLARE,
		KEY_SMALL_MEDIPACK,
		KEY_LARGE_MEDIPACK,
		KEY_PREVIOUS_WEAPON,
		KEY_NEXT_WEAPON,
		KEY_WEAPON_1,
		KEY_WEAPON_2,
		KEY_WEAPON_3,
		KEY_WEAPON_4,
		KEY_WEAPON_5,
		KEY_WEAPON_6,
		KEY_WEAPON_7,
		KEY_WEAPON_8,
		KEY_WEAPON_9,
		KEY_WEAPON_10,

		KEY_SELECT,
		KEY_DESELECT,
		KEY_PAUSE,
		KEY_INVENTORY,
		KEY_SAVE,
		KEY_LOAD,

		KEY_COUNT
	};

	// Deprecated.
	enum InputActions
	{
		IN_NONE = 0,

		IN_FORWARD = (1 << KEY_FORWARD),
		IN_BACK	   = (1 << KEY_BACK),
		IN_LEFT	   = (1 << KEY_LEFT),
		IN_RIGHT   = (1 << KEY_RIGHT),
		IN_LSTEP   = (1 << KEY_LEFT_STEP),
		IN_RSTEP   = (1 << KEY_RIGHT_STEP),
		IN_WALK	   = (1 << KEY_WALK),
		IN_SPRINT  = (1 << KEY_SPRINT),
		IN_CROUCH  = (1 << KEY_CROUCH),
		IN_JUMP	   = (1 << KEY_JUMP),
		IN_ROLL	   = (1 << KEY_ROLL),
		IN_ACTION  = (1 << KEY_ACTION),
		IN_DRAW	   = (1 << KEY_DRAW),
		IN_LOOK	   = (1 << KEY_LOOK)
	};
	
	enum InputAxis
	{
		MoveVertical,
		MoveHorizontal,
		CameraVertical,
		CameraHorizontal,
		Count
	};

	enum class QueueState
	{
		None,
		Push,
		Clear
	};

	enum class RumbleMode
	{
		None,
		Left,
		Right,
		Both
	};

	struct RumbleData
	{
		float	   Power	 = 0.0f;
		RumbleMode Mode		 = RumbleMode::None;
		float	   LastPower = 0.0f;
		float	   FadeSpeed = 0.0f;
	};

	extern std::vector<InputAction> ActionMap;
	extern std::vector<QueueState>	ActionQueue;
	extern std::vector<bool>		KeyMap;
	extern std::vector<float>		AxisMap;

	extern int DbInput; // Debounce input.
	extern int TrInput; // Throttle input.

	void InitializeInput(HWND handle);
	void DeinitializeInput();
	void DefaultConflict();
	void UpdateInputActions(ItemInfo* item, bool applyQueue = false);
	void ApplyActionQueue();
	void ClearActionQueue();
	void ClearAllActions();
	void Rumble(float power, float delayInSec = 0.3f, RumbleMode mode = RumbleMode::Both);
	void StopRumble();
    void ApplyDefaultBindings();
    bool ApplyDefaultXInputBindings();

	// TODO: Later, all these global action accessor functions should be tied to a specific controller/player.
	// Having them loose like this is very inelegant, but since this is only the first iteration, they will do for now. -- Sezz 2022.10.12
	void  ClearAction(ActionID actionID);
	bool  NoAction();
	bool  IsClicked(ActionID actionID);
	bool  IsHeld(ActionID actionID, float delayInSec = 0.0f);
	bool  IsPulsed(ActionID actionID, float delayInSec, float initialDelayInSec = 0.0f);
	bool  IsReleased(ActionID actionID, float maxDelayInSec = INFINITY);
	float GetActionValue(ActionID actionID);
	float GetActionTimeActive(ActionID actionID);
	float GetActionTimeInactive(ActionID actionID);

	bool IsDirectionalActionHeld();
	bool IsWakeActionHeld();
	bool IsOpticActionHeld();
}
