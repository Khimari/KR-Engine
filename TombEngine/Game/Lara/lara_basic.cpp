#include "framework.h"
#include "Game/Lara/lara_basic.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_monkey.h"
#include "Game/Lara/lara_slide.h"
#include "Game/Lara/lara_tests.h"
#include "Game/pickup/pickup.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

#include "Renderer/Renderer.h"
using TEN::Renderer::g_Renderer;

using namespace TEN::Config;
using namespace TEN::Entities::Player;
using namespace TEN::Input;

// ------------------------------
// BASIC MOVEMENT & MISCELLANEOUS
// Control & Collision Functions
// ------------------------------

// --------------
// MISCELLANEOUS:
// --------------

void lara_void_func(ItemInfo* item, CollisionInfo* coll)
{
	return;
}

void lara_default_col(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HeadingOrient.y = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeDown = true;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);
	LaraResetGravityStatus(item, coll);
}

// Boulder death.
void lara_as_special(ItemInfo* item, CollisionInfo* coll)
{
	Camera.flags = CameraFlag::FollowCenter;
	Camera.targetAngle = ANGLE(170.0f);
	Camera.targetElevation = ANGLE(-25.0f);
}

void lara_as_null(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
}

void lara_as_controlled(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.flags = CameraFlag::FollowCenter;

	if (item->Animation.FrameNumber == GetAnimData(*item).frameEnd - 1)
	{
		player.Control.HandStatus = HandStatus::Free;

		if (Camera.UseForcedFixedCamera)
			Camera.UseForcedFixedCamera = false;
	}
}

void lara_as_controlled_no_look(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
}

// State:	  LS_VAULT (164)
// Collision: lara_void_func()
void lara_as_vault(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;

	EasePlayerElevation(item, player.Context.ProjectedFloorHeight - item->Pose.Position.y);
	item->Pose.Orientation.Lerp(player.Context.TargetOrientation, 0.4f);

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	  LS_AUTO_JUMP (62)
// Collision: lara_as_jump_prepare()
void lara_as_auto_jump(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;

	item->Pose.Orientation.Lerp(player.Context.TargetOrientation, 0.4f);
}

// ---------------
// BASIC MOVEMENT:
// ---------------

// State:	  LS_WALK_FORWARD (0)
// Collision: lara_col_walk_forward()
void lara_as_walk_forward(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);
	player.Control.Count.Run = std::clamp<unsigned int>(
		player.Control.Count.Run + 1,
		0, IsUsingModernControls() ? (PLAYER_MODERN_CONTROL_RUN_JUMP_TIME / 2) : (PLAYER_TANK_CONTROL_RUN_JUMP_TIME / 2) + 4);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	// HACK: Interaction alignment.
	if (player.Control.IsMoving)
	{
		ResetPlayerTurnRateY(*item);
		return;
	}

	// Turn.
	if (IsUsingModernControls())
	{
		HandlePlayerTurn(*item, PLAYER_STANDARD_TURN_ALPHA, LARA_LEAN_MAX / 2, IsPlayerStrafing(*item), TURN_FLAGS);
	}
	else
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			if (IsUsingClassicControls())
			{
				ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX);
			}
			else if (IsUsingEnhancedControls())
			{
				ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_MED_TURN_RATE_MAX);
				HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE / 6, LARA_LEAN_MAX / 2);
			}
		}
	}

	if (IsUsingModernControls() ?
		(IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right)) :
		IsHeld(In::Forward))
	{
		if (IsHeld(In::Action))
		{
			auto vaultContext = TestLaraVault(item, coll);
			if (vaultContext.has_value())
			{
				item->Animation.TargetState = vaultContext->TargetState;
				SetLaraVault(item, coll, *vaultContext);
				return;
			}
		}

		if (isWading)
		{
			item->Animation.TargetState = LS_WADE_FORWARD;
		}
		else if (IsHeld(In::Walk))
		{
			/*short relMoveAngle = GetPlayerRelMoveAngle(*item);
			if (abs(relMoveAngle) > ANGLE(90.0f) && HasStateDispatch(item, LS_WALK_FORWARD_TURN_180))
			{
				if (HasStateDispatch(item, LS_WALK_FORWARD_TURN_180))
					item->Pose.Orientation.y += ANGLE(180.0f);

				item->Animation.TargetState = LS_WALK_FORWARD_TURN_180;
			}
			else*/
			{
				item->Animation.TargetState = LS_WALK_FORWARD;
			}
		}
		else
		{
			item->Animation.TargetState = LS_RUN_FORWARD;
		}

		return;
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LA_WALK_FORWARD (0)
// Control: lara_as_walk_forward()
void lara_col_walk_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Setup.
	player.Control.HeadingOrient.y = GetPlayerHeadingAngleY(*item);
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	coll->Setup.LowerFloorBound = STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.BlockFloorSlopeDown = true;
	coll->Setup.BlockDeathFloorDown = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (TestAndDoLaraLadderClimb(item, coll))
		return;

	if (LaraDeflectEdge(item, coll))
	{
		item->Animation.TargetState = LS_SOFT_SPLAT;
		if (GetStateDispatch(item, GetAnimData(*item)))
		{
			item->Animation.ActiveState = LS_SOFT_SPLAT;
			return;
		}

		LaraCollideStop(item, coll);
	}

	if (CanChangeElevation(*item, *coll) && coll->CollisionType != CollisionType::Front)
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_RUN_FORWARD (1)
// Collision: lara_col_run_forward()
void lara_as_run_forward(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	player.Control.Look.Mode = LookMode::Horizontal;
	player.Control.Count.Run = std::clamp<unsigned int>(player.Control.Count.Run + 1, 0, PLAYER_TANK_CONTROL_RUN_JUMP_TIME);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	// Turn.
	if (IsUsingModernControls())
	{
		HandlePlayerTurn(*item, PLAYER_STANDARD_TURN_ALPHA, LARA_LEAN_MAX, IsPlayerStrafing(*item), TURN_FLAGS);
	}
	else
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_FAST_TURN_RATE_MAX);
			HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE, LARA_LEAN_MAX);
		}
	}

	if (IsHeld(In::Jump) || player.Control.IsRunJumpQueued)
	{
		if (!IsHeld(In::Sprint) && CanRunJumpForward(*item, *coll))
		{
			item->Animation.TargetState = LS_JUMP_FORWARD;
			return;
		}

		if (!IsUsingClassicControls())
			player.Control.IsRunJumpQueued = CanQueueRunningJump(*item, *coll);
	}

	if ((IsHeld(In::Roll) || (HasOppositeAction(*item) && g_Configuration.EnableOppositeActionRoll)) &&
		CanRoll180Running(*item))
	{
		item->Animation.TargetState = LS_ROLL_180_FORWARD;
		return;
	}

	if (IsHeld(In::Crouch) && CanCrouch(*item, *coll))
	{
		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	if (IsUsingModernControls() ?
		(IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right)) :
		IsHeld(In::Forward))
	{
		if (IsHeld(In::Action))
		{
			auto vaultContext = TestLaraVault(item, coll);
			if (vaultContext.has_value())
			{
				item->Animation.TargetState = vaultContext->TargetState;
				SetLaraVault(item, coll, *vaultContext);
				return;
			}
		}

		if (isWading)
		{
			item->Animation.TargetState = LS_WADE_FORWARD;
		}
		else if (IsHeld(In::Walk))
		{
			item->Animation.TargetState = LS_WALK_FORWARD;
		}
		else if (IsHeld(In::Sprint) && player.Status.Stamina > LARA_STAMINA_MIN)
		{
			item->Animation.TargetState = LS_SPRINT;
		}
		else
		{
			if (IsPlayerStrafing(*item))
			{
				item->Animation.TargetState = CanStrafeBackward(*item, *coll) ? LS_SKIP_BACK : LS_RUN_FORWARD;
			}
			else
			{
				item->Animation.TargetState = LS_RUN_FORWARD;
			}
		}

		return;
	}

	// Reset.
	if (IsUsingModernControls())
	{
		item->Animation.TargetState = HasStateDispatch(item, LS_RUN_FORWARD_CANCEL) ? LS_RUN_FORWARD_CANCEL : LS_IDLE;
	}
	else
	{
		item->Animation.TargetState = LS_IDLE;
	}
}

// State:	LS_RUN_FORWARD (1)
// Control: lara_as_run_forward()
void lara_col_run_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Setup.
	player.Control.HeadingOrient.y = GetPlayerHeadingAngleY(*item);
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);
	LaraResetGravityStatus(item, coll);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (TestAndDoLaraLadderClimb(item, coll))
		return;

	if (LaraDeflectEdge(item, coll))
	{
		ResetPlayerLean(item);

		if (TestLaraWall(item, OFFSET_RADIUS(coll->Setup.Radius), -CLICK(2.5f)) ||
			coll->HitTallObject)
		{
			item->Animation.TargetState = LS_SPLAT;
			if (GetStateDispatch(item, GetAnimData(*item)))
			{
				Rumble(0.4f, 0.15f);

				item->Animation.ActiveState = LS_SPLAT;
				return;
			}
		}

		item->Animation.TargetState = LS_SOFT_SPLAT;
		if (GetStateDispatch(item, GetAnimData(*item)))
		{
			item->Animation.ActiveState = LS_SOFT_SPLAT;
			return;
		}

		LaraCollideStop(item, coll);
	}

	if (CanChangeElevation(*item, *coll) && coll->CollisionType != CollisionType::Front)
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_IDLE (2), LS_SPLAT_SOFT (170)
// Collision: lara_col_idle()
void lara_as_idle(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);
	bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, item);
	player.Control.CanLook = !((isWading && isInSwamp) || item->Animation.AnimNumber == LA_SWANDIVE_ROLL);

	player.Control.Look.Mode = LookMode::Free;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	// HACK: Interaction alignment.
	if (player.Control.IsMoving)
	{
		ResetPlayerTurnRateY(*item);
		return;
	}

	// TODO: Handle waterskin and mechanical scarab.
	if (UseSpecialItem(item))
		return;

	if (player.Control.Look.IsUsingBinoculars)
		return;

	if (IsClicked(In::Walk))
		player.Control.ToggleWalk = true;

	// Turn.
	if (IsUsingClassicControls())
	{
		if ((IsHeld(In::Forward) || abs(player.Control.TurnRate.y) >= LARA_MED_TURN_RATE_MAX) &&
			(IsHeld(In::Left) || IsHeld(In::Right)))
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_FAST_TURN_RATE_MAX);
		}
		else
		{
			ResetPlayerTurnRateY(*item);
		}
	}
	else if (IsUsingEnhancedControls())
	{
		// Jump locks orientation.
		if (!IsHeld(In::Jump))
		{
			// Sidestep locks orientation.
			if ((IsHeld(In::StepLeft) || (IsHeld(In::Walk) && IsHeld(In::Left))) ||
				(IsHeld(In::StepRight) || (IsHeld(In::Walk) && IsHeld(In::Right))))
			{
				ResetPlayerTurnRateY(*item);
			}
			else if (IsHeld(In::Left) || IsHeld(In::Right))
			{
				ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_MED_FAST_TURN_RATE_MAX);
			}
		}
	}
	else if (IsUsingModernControls())
	{
		float turnAlpha = isWading ?
			PLAYER_WADE_TURN_ALPHA * (isInSwamp ? 0.5f : 1.0f) :
			PLAYER_STANDARD_TURN_ALPHA;

		if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
		{
			// Directional jump intent locks orientation.
			if (!(IsPlayerStrafing(*item) || (IsHeld(In::Jump) && IsHeld(In::Walk))))
			{
				HandlePlayerTurn(*item, turnAlpha, 0, false, TURN_FLAGS);
			}
		}
		else if (IsPlayerStrafing(*item))
		{
			HandlePlayerTurn(*item, turnAlpha, 0, false, TURN_FLAGS);
		}
	}

	if (IsHeld(In::Jump))
	{
		if ((IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right)) &&
			g_Configuration.EnableAutoClimb)
		{
			auto vaultContext = TestLaraVault(item, coll);
			if (vaultContext.has_value())
			{
				item->Animation.TargetState = vaultContext->TargetState;
				SetLaraVault(item, coll, *vaultContext);
				return;
			}
		}

		if (CanPerformJump(*item, *coll))
		{
			auto jumpDirection = GetPlayerJumpDirection(*item, *coll);
			if (jumpDirection != JumpDirection::None)
			{
				item->Animation.TargetState = LS_JUMP_PREPARE;
				player.Control.JumpDirection = jumpDirection;
				return;
			}
		}
	}

	if (IsHeld(In::Roll) || (HasOppositeAction(*item) && g_Configuration.EnableOppositeActionRoll))
	{
		if (IsHeld(In::Walk) || CanTurn180(*item, *coll))
		{
			item->Animation.TargetState = LS_TURN_180;
		}
		else
		{
			item->Animation.TargetState = LS_ROLL_180_FORWARD;
		}

		return;
	}

	if (IsHeld(In::Crouch) && CanCrouch(*item, *coll))
	{
		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	if (IsHeld(In::Look) && CanPlayerLookAround(*item))
	{
		item->Animation.TargetState = LS_IDLE;
		return;
	}

	if (IsUsingModernControls())
	{
		if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
		{
			if (IsHeld(In::Action))
			{
				auto vaultContext = TestLaraVault(item, coll);
				if (vaultContext.has_value())
				{
					item->Animation.TargetState = vaultContext->TargetState;
					SetLaraVault(item, coll, *vaultContext);
					return;
				}
			}

			if (CanStrafeBackward(*item, *coll))
			{
				if (CanWadeBackward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_BACK;
				}
				else if (IsHeld(In::Walk))
				{
					if (CanWalkBackward(*item, *coll))
						item->Animation.TargetState = LS_WALK_BACK;
				}
				else if (CanRunBackward(*item, *coll))
				{
					item->Animation.TargetState = LS_SKIP_BACK;
				}

				return;
			}

			if (CanWadeForward(*item, *coll))
			{
				item->Animation.TargetState = LS_WADE_FORWARD;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkForward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_FORWARD;
					return;
				}
			}
			else if (CanRunForward(*item, *coll))
			{
				if (IsHeld(In::Sprint))
				{
					item->Animation.TargetState = LS_SPRINT;
				}
				else
				{
					item->Animation.TargetState = LS_RUN_FORWARD;
				}

				return;
			}
		}
	}
	else
	{
		if (IsHeld(In::Forward))
		{
			if (IsHeld(In::Action))
			{
				auto vaultContext = TestLaraVault(item, coll);
				if (vaultContext.has_value())
				{
					item->Animation.TargetState = vaultContext->TargetState;
					SetLaraVault(item, coll, *vaultContext);
					return;
				}
			}

			if (CanWadeForward(*item, *coll))
			{
				item->Animation.TargetState = LS_WADE_FORWARD;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkForward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_FORWARD;
					return;
				}
			}
			else if (CanRunForward(*item, *coll))
			{
				if (IsHeld(In::Sprint))
				{
					item->Animation.TargetState = LS_SPRINT;
				}
				else
				{
					item->Animation.TargetState = LS_RUN_FORWARD;
				}

				return;
			}
		}
		else if (IsHeld(In::Back))
		{
			if (CanWadeBackward(*item, *coll))
			{
				item->Animation.TargetState = LS_WALK_BACK;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkBackward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_BACK;
					return;
				}
			}
			else if (CanRunBackward(*item, *coll))
			{
				item->Animation.TargetState = LS_HOP_BACK;
				return;
			}
		}

		if (IsHeld(In::Left))
		{
			if ((IsHeld(In::Sprint) || CanTurnFast(*item, *coll, false)) &&
				!isWading)
			{
				item->Animation.TargetState = LS_TURN_LEFT_FAST;
			}
			else
			{
				item->Animation.TargetState = LS_TURN_LEFT_SLOW;
			}

			return;
		}
		else if (IsHeld(In::Right))
		{
			if ((IsHeld(In::Sprint) || CanTurnFast(*item, *coll, true)) &&
				!isWading)
			{
				item->Animation.TargetState = LS_TURN_RIGHT_FAST;
			}
			else
			{
				item->Animation.TargetState = LS_TURN_RIGHT_SLOW;
			}

			return;
		}
	}

	if (IsHeld(In::StepLeft) || ((IsHeld(In::Walk) && IsHeld(In::Left)) && !IsUsingModernControls()))
	{
		if (CanSidestepLeft(*item, *coll))
		{
			item->Animation.TargetState = LS_STEP_LEFT;
		}
		else
		{
			item->Animation.TargetState = LS_IDLE;
		}

		return;
	}
	else if (IsHeld(In::StepRight) || ((IsHeld(In::Walk) && IsHeld(In::Right)) && !IsUsingModernControls()))
	{
		if (CanSidestepRight(*item, *coll))
		{
			item->Animation.TargetState = LS_STEP_RIGHT;
		}
		else
		{
			item->Animation.TargetState = LS_IDLE;
		}

		return;
	}

	if (CanStrikeAfkPose(*item, *coll) && player.Control.Count.Pose >= PLAYER_POSE_TIME)
	{
		item->Animation.TargetState = LS_POSE;
		return;
	}

	// Reset.
	if (IsPlayerStrafing(*item))
	{
		int turnStateID = GetPlayerStrafeTurnStateID(*item);
		if (turnStateID != NO_VALUE)
		{
			item->Animation.TargetState = turnStateID;
			return;
		}

		item->Animation.TargetState = LS_IDLE;
	}
	else
	{
		item->Animation.TargetState = LS_IDLE;
	}
}

// State:	LS_IDLE (2), LS_POSE (4), LS_SPLAT_SOFT (170)
// Control: lara_as_idle(), lara_as_pose()
void lara_col_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	// Setup.
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	player.Control.HeadingOrient.y = GetPlayerHeadingAngleY(*item) + ((item->Animation.Velocity.z >= 0.0f) ? 0 : ANGLE(180.0f));
	coll->Setup.LowerFloorBound = isWading ? NO_LOWER_BOUND : STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeDown = !isWading;
	coll->Setup.BlockFloorSlopeUp = !isWading;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	// TODO: Better clamp handling. This can result in the player standing above or below the floor. -- Sezz 2022.04.01
	/*if (TestLaraHitCeiling(coll))
	{
	SetLaraHitCeiling(item, coll);
	return;
	}*/

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (TestAndDoLaraLadderClimb(item, coll))
		return;

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_POSE (4)
// Collision: lara_col_idle()
void lara_as_pose(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Free;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (CanStrikeAfkPose(*item, *coll))
	{
		if (IsHeld(In::Roll))
		{
			item->Animation.TargetState = LS_ROLL_180_FORWARD;
			return;
		}

		if (IsWakeActionHeld())
		{
			item->Animation.TargetState = LS_IDLE;
			return;
		}

		item->Animation.TargetState = LS_POSE;
		return;
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	  LS_HOP_BACK (5)
// Collision: lara_col_hop_back()
void lara_as_hop_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;

	// Turn.
	if (IsHeld(In::Left) || IsHeld(In::Right))
	{
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_MED_TURN_RATE_MAX);
		HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE / 4, LARA_LEAN_MAX / 3);
	}

	if (!IsUsingClassicControls())
	{
		if (IsHeld(In::Roll))
		{
			item->Animation.TargetState = LS_ROLL_180_FORWARD;
			return;
		}
	}

	if (IsHeld(In::Back))
	{
		item->Animation.TargetState = LS_HOP_BACK;
		return;
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_HOP_BACK (5)
// Control: lara_as_hop_back()
void lara_col_hop_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Setup.
	player.Control.HeadingOrient.y = item->Pose.Orientation.y + ANGLE(180.0f);
	item->Animation.Velocity.y = 0;
	item->Animation.IsAirborne = false;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (coll->Middle.Floor > (STEPUP_HEIGHT / 2))
	{
		SetLaraFallBackAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (LaraDeflectEdge(item, coll))
		LaraCollideStop(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_SKIP_BACK (207)
// Collision: lara_col_skip_back()
void lara_as_skip_back(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;

	// Turn.
	HandlePlayerTurn(*item, PLAYER_STANDARD_TURN_ALPHA, LARA_LEAN_MAX, true, TURN_FLAGS);

	if (IsHeld(In::Jump))
	{
		item->Animation.TargetState = LS_JUMP_BACK;
		return;
	}

	if (IsHeld(In::Roll))
	{
		item->Animation.TargetState = LS_ROLL_180_FORWARD;
		return;
	}

	if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
	{
		if (IsPlayerStrafing(*item))
		{
			item->Animation.TargetState = CanStrafeBackward(*item, *coll) ? LS_SKIP_BACK : LS_RUN_FORWARD;
			return;
		}
		else
		{
			item->Animation.TargetState = LS_RUN_FORWARD;
			return;
		}
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_SKIP_BACK (207)
// Control: lara_as_skip_back()
void lara_col_skip_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Setup.
	player.Control.HeadingOrient.y = GetPlayerHeadingAngleY(*item);
	item->Animation.Velocity.y = 0.0f;
	item->Animation.IsAirborne = false;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (coll->Middle.Floor > (STEPUP_HEIGHT / 2))
	{
		SetLaraFallBackAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (LaraDeflectEdge(item, coll))
		LaraCollideStop(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_TURN_RIGHT_SLOW (6), LS_TURN_LEFT_SLOW (7)
// Collision: lara_col_turn_slow()
void lara_as_turn_slow(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);
	bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, item);
	player.Control.CanLook = (isWading && isInSwamp) ? false : true;

	player.Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	// Turn.
	if (IsUsingModernControls())
	{
		float turnAlpha = isWading ? (PLAYER_WADE_TURN_ALPHA * (isInSwamp ? 0.5f : 1.0f)) : PLAYER_STANDARD_TURN_ALPHA;
		HandlePlayerTurn(*item, turnAlpha, 0, true, TURN_FLAGS);
	}
	else
	{
		if (isWading)
		{
			if (IsUsingClassicControls())
			{
				ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_WADE_TURN_RATE_MAX / 2);
			}
			else
			{
				ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, isInSwamp ? LARA_SWAMP_TURN_RATE_MAX : LARA_WADE_TURN_RATE_MAX);
			}
		}
		else
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_MED_FAST_TURN_RATE_MAX);
		}
	}

	if (IsUsingClassicControls())
	{
		if (IsHeld(In::Forward))
		{
			if (IsHeld(In::Action))
			{
				auto vaultContext = TestLaraVault(item, coll);
				if (vaultContext.has_value())
				{
					item->Animation.TargetState = vaultContext->TargetState;
					SetLaraVault(item, coll, *vaultContext);
					return;
				}
			}

			if (CanWadeForward(*item, *coll))
			{
				item->Animation.TargetState = LS_WADE_FORWARD;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkForward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_FORWARD;
					return;
				}
			}
			else if (CanRunForward(*item, *coll))
			{
				if (IsHeld(In::Sprint))
				{
					item->Animation.TargetState = LS_SPRINT;
				}
				else
				{
					item->Animation.TargetState = LS_RUN_FORWARD;
				}

				return;
			}
		}
	}
	else
	{
		if (IsHeld(In::Jump))
		{
			if (IsHeld(In::Forward) && g_Configuration.EnableAutoClimb)
			{
				auto vaultContext = TestLaraVault(item, coll);
				if (vaultContext.has_value())
				{
					item->Animation.TargetState = vaultContext->TargetState;
					SetLaraVault(item, coll, *vaultContext);
					return;
				}
			}

			if (CanPerformJump(*item, *coll))
			{
				auto jumpDirection = GetPlayerJumpDirection(*item, *coll);
				if (jumpDirection != JumpDirection::None)
				{
					item->Animation.TargetState = LS_JUMP_PREPARE;
					player.Control.JumpDirection = jumpDirection;
					return;
				}
			}
		}

		if ((IsHeld(In::Roll) || (HasOppositeAction(*item) && g_Configuration.EnableOppositeActionRoll)) &&
			!isWading)
		{
			item->Animation.TargetState = LS_ROLL_180_FORWARD;
			return;
		}

		if (IsHeld(In::Crouch) && CanCrouch(*item, *coll))
		{
			item->Animation.TargetState = LS_CROUCH_IDLE;
			return;
		}

		if (IsHeld(In::Forward))
		{
			if (IsHeld(In::Action))
			{
				auto vaultContext = TestLaraVault(item, coll);
				if (vaultContext.has_value())
				{
					item->Animation.TargetState = vaultContext->TargetState;
					SetLaraVault(item, coll, *vaultContext);
					return;
				}
			}

			if (CanWadeForward(*item, *coll))
			{
				item->Animation.TargetState = LS_WADE_FORWARD;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkForward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_FORWARD;
					return;
				}
			}
			else if (CanRunForward(*item, *coll))
			{
				if (IsHeld(In::Sprint))
				{
					item->Animation.TargetState = LS_SPRINT;
				}
				else
				{
					item->Animation.TargetState = LS_RUN_FORWARD;
				}

				return;
			}
		}
		else if (IsHeld(In::Back))
		{
			if (CanWadeBackward(*item, *coll))
			{
				item->Animation.TargetState = LS_WALK_BACK;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkBackward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_BACK;
					return;
				}
			}
			else if (CanRunBackward(*item, *coll))
			{
				item->Animation.TargetState = LS_HOP_BACK;
				return;
			}
		}
	}

	if (IsHeld(In::StepLeft) || (IsHeld(In::Walk) && IsHeld(In::Left)) &&
		CanSidestepLeft(*item, *coll))
	{
		item->Animation.TargetState = LS_STEP_LEFT;
		return;
	}
	else if (IsHeld(In::StepRight) || (IsHeld(In::Walk) && IsHeld(In::Right)) &&
		CanSidestepRight(*item, *coll))
	{
		item->Animation.TargetState = LS_STEP_RIGHT;
		return;
	}

	if (IsHeld(In::Left) && item->Animation.ActiveState == LS_TURN_LEFT_SLOW)
	{
		if (player.Control.TurnRate.y < -LARA_MED_TURN_RATE_MAX &&
			!isWading)
		{
			item->Animation.TargetState = LS_TURN_LEFT_FAST;
		}
		else
		{
			item->Animation.TargetState = LS_TURN_LEFT_SLOW;
		}

		return;
	}
	else if (IsHeld(In::Right) && item->Animation.ActiveState == LS_TURN_RIGHT_SLOW)
	{
		if (player.Control.TurnRate.y > LARA_MED_TURN_RATE_MAX &&
			!isWading)
		{
			item->Animation.TargetState = LS_TURN_RIGHT_FAST;
		}
		else
		{
			item->Animation.TargetState = LS_TURN_RIGHT_SLOW;
		}

		return;
	}

	// Reset.
	if (IsPlayerStrafing(*item))
	{
		int turnStateID = GetPlayerStrafeTurnStateID(*item);
		if (turnStateID != NO_VALUE)
		{
			item->Animation.TargetState = turnStateID;
			return;
		}

		item->Animation.TargetState = LS_IDLE;
	}
	else
	{
		item->Animation.TargetState = LS_IDLE;
	}
}

// State:	LS_TURN_RIGHT_SLOW (6), LS_TURN_LEFT_SLOW (7)
// Control: lara_as_turn_slow()
void lara_col_turn_slow(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_idle(item, coll);
}

// State:	  LS_DEATH (8)
// Collision: lara_col_death()
void lara_as_death(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.z = 0.0f;
	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;

	ResetPlayerTurnRateY(*item);

	if (player.Control.Look.OpticRange != 0)
	{
		item->MeshBits = ALL_JOINT_BITS;
		player.Control.Look.OpticRange = 0;
		player.Control.Look.IsUsingLasersight = false;
		player.Inventory.IsBusy = false;
		SetFov(Camera.PrevFov);
	}

	auto box = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose);
	if ((box.Extents.y * 2) <= (LARA_HEIGHT * 0.75f))
		AlignLaraToSurface(item);
}

// State:		LS_DEATH (8)
// Control:		lara_as_death()
void lara_col_death(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.IsAirborne = false;
	player.Control.HeadingOrient.y = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.Radius = LARA_RADIUS_DEATH;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	StopSoundEffect(SFX_TR4_LARA_FALL);
	item->HitPoints = -1;

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:		LS_SPLAT (12)
// Collision:	lara_col_splat()
void lara_as_splat(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Free;
	ResetPlayerTurnRateY(*item);
}

// State:		LS_SPLAT (12)
// Control:		lara_as_splat()
void lara_col_splat(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HeadingOrient.y = item->Pose.Orientation.y;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.BlockFloorSlopeDown = true;
	coll->Setup.LowerFloorBound = STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_WALK_BACK (16)
// Collision: lara_col_walk_back()
void lara_as_walk_back(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);
	bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, item);

	player.Control.CanLook = (isWading && isInSwamp) ? false : true;
	player.Control.Look.Mode = LookMode::Horizontal;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (player.Control.IsMoving)
	{
		ResetPlayerTurnRateY(*item);
		return;
	}

	if (IsUsingModernControls())
	{
		// Turn.
		HandlePlayerTurn(*item, PLAYER_STANDARD_TURN_ALPHA, 0, true, TURN_FLAGS);

		if (IsPlayerStrafing(*item))
		{
			if ((IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right)) &&
				(IsHeld(In::Walk) || isWading))
			{
				item->Animation.TargetState = CanStrafeBackward(*item, *coll) ? LS_WALK_BACK : LS_WALK_FORWARD;
				return;
			}
		}
	}
	else
	{
		// Turn.
		if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			if (isWading && isInSwamp)
			{
				ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX / 3);
				HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE / 3, LARA_LEAN_MAX / 3);
			}
			else
			{
				ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX);
				HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE / 4, LARA_LEAN_MAX / 3);
			}
		}

		if (IsHeld(In::Back) && (IsHeld(In::Walk) || isWading))
		{
			item->Animation.TargetState = LS_WALK_BACK;
			return;
		}
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_WALK_BACK (16)
// Control: lara_as_walk_back()
void lara_col_walk_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	// Setup.
	player.Control.HeadingOrient.y = GetPlayerHeadingAngleY(*item) + (IsUsingModernControls() ? ANGLE(0.0f) : ANGLE(180.0f));
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	coll->Setup.LowerFloorBound = isWading ? NO_LOWER_BOUND : STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeDown = !isWading;
	coll->Setup.BlockFloorSlopeUp = !isWading;
	coll->Setup.BlockDeathFloorDown = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (LaraDeflectEdge(item, coll))
		LaraCollideStop(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_TURN_RIGHT_FAST (20), LS_TURN_LEFT_FAST (152)
// Collision: lara_col_turn_fast()
void lara_as_turn_fast(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	player.Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	// Turn.
	ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, LARA_MED_TURN_RATE_MAX, LARA_FAST_TURN_RATE_MAX);

	if (IsHeld(In::Jump))
	{
		if (IsHeld(In::Forward) && g_Configuration.EnableAutoClimb)
		{
			auto vaultContext = TestLaraVault(item, coll);
			if (vaultContext.has_value())
			{
				item->Animation.TargetState = vaultContext->TargetState;
				SetLaraVault(item, coll, *vaultContext);
				return;
			}
		}

		if (CanPerformJump(*item, *coll))
		{
			auto jumpDirection = GetPlayerJumpDirection(*item, *coll);
			if (jumpDirection != JumpDirection::None)
			{
				item->Animation.TargetState = LS_JUMP_PREPARE;
				player.Control.JumpDirection = jumpDirection;
				return;
			}
		}
	}

	if (IsUsingEnhancedControls())
	{
		if ((IsHeld(In::Roll) || (HasOppositeAction(*item) && g_Configuration.EnableOppositeActionRoll)) &&
			!isWading)
		{
			item->Animation.TargetState = LS_ROLL_180_FORWARD;
			return;
		}

		if (IsHeld(In::Crouch) && CanCrouch(*item, *coll))
		{
			item->Animation.TargetState = LS_CROUCH_IDLE;
			return;
		}

		if (IsHeld(In::Forward))
		{
			if (IsHeld(In::Action))
			{
				auto vaultContext = TestLaraVault(item, coll);
				if (vaultContext.has_value())
				{
					item->Animation.TargetState = vaultContext->TargetState;
					SetLaraVault(item, coll, *vaultContext);
					return;
				}
			}

			if (CanWadeForward(*item, *coll))
			{
				item->Animation.TargetState = LS_WADE_FORWARD;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkForward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_FORWARD;
					return;
				}
			}
			else if (CanRunForward(*item, *coll))
			{
				if (IsHeld(In::Sprint))
				{
					item->Animation.TargetState = LS_SPRINT;
				}
				else
				{
					item->Animation.TargetState = LS_RUN_FORWARD;
				}

				return;
			}
		}
		else if (IsHeld(In::Back))
		{
			if (CanWadeBackward(*item, *coll))
			{
				item->Animation.TargetState = LS_WALK_BACK;
				return;
			}
			else if (IsHeld(In::Walk))
			{
				if (CanWalkBackward(*item, *coll))
				{
					item->Animation.TargetState = LS_WALK_BACK;
					return;
				}
			}
			else if (CanRunBackward(*item, *coll))
			{
				item->Animation.TargetState = LS_HOP_BACK;
				return;
			}
		}
	}

	if (IsHeld(In::StepLeft) || (IsHeld(In::Walk) && IsHeld(In::Left)) &&
		CanSidestepLeft(*item, *coll))
	{
		item->Animation.TargetState = LS_STEP_LEFT;
		return;
	}
	else if (IsHeld(In::StepRight) || (IsHeld(In::Walk) && IsHeld(In::Right)) &&
		CanSidestepRight(*item, *coll))
	{
		item->Animation.TargetState = LS_STEP_RIGHT;
		return;
	}

	if (IsHeld(In::Left) && item->Animation.ActiveState == LS_TURN_LEFT_FAST)
	{
		item->Animation.TargetState = LS_TURN_LEFT_FAST;
		return;
	}
	else if (IsHeld(In::Right) && item->Animation.ActiveState == LS_TURN_RIGHT_FAST)
	{
		item->Animation.TargetState = LS_TURN_RIGHT_FAST;
		return;
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_TURN_RIGHT_FAST (20), LS_TURN_LEFT_FAST (152)
// Control: lara_as_turn_fast()
void lara_col_turn_fast(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_idle(item, coll);
}

// State:	  LS_STEP_RIGHT (21)
// Collision: lara_col_step_right()
void lara_as_step_right(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY;

	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (player.Control.IsMoving)
	{
		ResetPlayerTurnRateY(*item);
		return;
	}

	// Turn.
	if (IsUsingModernControls())
	{
		HandlePlayerTurn(*item, PLAYER_STANDARD_TURN_ALPHA, 0, true, TURN_FLAGS);
	}
	else
	{
		// Walk action locks orientation.
		if (IsHeld(In::Walk))
		{
			ResetPlayerTurnRateY(*item);
		}
		else if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX);
		}
	}

	if (IsHeld(In::StepRight) || ((IsHeld(In::Walk) && IsHeld(In::Right)) && !IsUsingModernControls()))
	{
		item->Animation.TargetState = LS_STEP_RIGHT;
		return;
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_STEP_RIGHT (21)
// Control: lara_as_step_right()
void lara_col_step_right(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	// Setup.
	player.Control.HeadingOrient.y = item->Pose.Orientation.y + ANGLE(90.0f);
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	coll->Setup.LowerFloorBound = isWading ? NO_LOWER_BOUND : CLICK(0.8f);
	coll->Setup.UpperFloorBound = -CLICK(0.8f);
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeDown = !isWading;
	coll->Setup.BlockFloorSlopeUp = !isWading;
	coll->Setup.BlockDeathFloorDown = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (LaraDeflectEdge(item, coll))
	{
		item->Animation.TargetState = LS_SOFT_SPLAT;
		if (GetStateDispatch(item, GetAnimData(*item)))
		{
			item->Animation.ActiveState = LS_SOFT_SPLAT;
			return;
		}

		LaraCollideStop(item, coll);
	}

	if (CanChangeElevation(*item, *coll) || isWading)
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_STEP_LEFT (22)
// Collision: lara_col_step_left()
void lara_as_step_left(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY;

	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (player.Control.IsMoving)
	{
		ResetPlayerTurnRateY(*item);
		return;
	}

	// Turn.
	if (IsUsingModernControls())
	{
		HandlePlayerTurn(*item, PLAYER_STANDARD_TURN_ALPHA, 0, true, TURN_FLAGS);
	}
	else
	{
		// Walk action locks orientation.
		if (IsHeld(In::Walk))
		{
			ResetPlayerTurnRateY(*item);
		}
		else if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX);
		}
	}

	if (IsHeld(In::StepLeft) || ((IsHeld(In::Walk) && IsHeld(In::Left)) && !IsUsingModernControls()))
	{
		item->Animation.TargetState = LS_STEP_LEFT;
		return;
	}

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_STEP_LEFT (22)
// Control: lara_as_step_left()
void lara_col_step_left(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	// Setup.
	player.Control.HeadingOrient.y = item->Pose.Orientation.y - ANGLE(90.0f);
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	coll->Setup.LowerFloorBound = isWading ? NO_LOWER_BOUND : CLICK(0.8f);
	coll->Setup.UpperFloorBound = -CLICK(0.8f);
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeDown = !isWading;
	coll->Setup.BlockFloorSlopeUp = !isWading;
	coll->Setup.BlockDeathFloorDown = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (LaraDeflectEdge(item, coll))
	{
		item->Animation.TargetState = LS_SOFT_SPLAT;
		if (GetStateDispatch(item, GetAnimData(*item)))
		{
			item->Animation.ActiveState = LS_SOFT_SPLAT;
			return;
		}

		LaraCollideStop(item, coll);
	}

	if (CanChangeElevation(*item, *coll) || isWading)
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_TURN_180 (173)
// Collision: lara_col_turn_180()
void lara_as_turn_180(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	ResetPlayerTurnRateY(*item);

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_TURN_180 (173)
// Control: lara_as_turn_180()
void lara_col_turn_180(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_idle(item, coll);
}

// State:	  LS_ROLL_180_BACK (23)
// Collision: lara_col_roll_back()
void lara_as_roll_180_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	ResetPlayerTurnRateY(*item);

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_ROLL_180_BACK (23)
// Control: lara_as_roll_back()
void lara_col_roll_180_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Setup.
	player.Control.HeadingOrient.y = item->Pose.Orientation.y + ANGLE(180.0f);
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	Camera.laraNode = LM_HIPS;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (coll->Middle.Floor > (STEPUP_HEIGHT / 2))
	{
		SetLaraFallBackAnimation(item);
		return;
	}

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_ROLL_180_FORWARD (45)
// Collision: lara_col_roll_180_forward()
void lara_as_roll_180_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	ResetPlayerTurnRateY(*item);

	// Reset.
	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_ROLL_180_FORWARD (45)
// Control: lara_as_roll_180_forward()
void lara_col_roll_180_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HeadingOrient.y = item->Pose.Orientation.y;
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_WADE_FORWARD (65)
// Collision: lara_col_wade_forward()
void lara_as_wade_forward(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);
	bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, item);

	player.Control.Look.Mode = LookMode::Horizontal;
	Camera.targetElevation = -ANGLE(22.0f);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_IDLE;
		return;
	}

	// Turn.
	if (IsUsingModernControls())
	{
		float turnAlpha = PLAYER_WADE_TURN_ALPHA / (isInSwamp ? 3 : 1);
		short leanAngleMax = LARA_LEAN_MAX * (isInSwamp ? 0.6f : 1.0f);
		HandlePlayerTurn(*item, turnAlpha, leanAngleMax, false, TURN_FLAGS);
	}
	else
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			short turnRateMax = (IsUsingClassicControls() && !isInSwamp) ? LARA_FAST_TURN_RATE_MAX : LARA_MED_TURN_RATE_MAX;
			float leanCoeff = isInSwamp ? 0.6f : 1.0f;

			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, turnRateMax);
			HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE  / (isInSwamp ? 3 : 2), LARA_LEAN_MAX * leanCoeff);
		}
	}

	if (IsUsingModernControls() ?
		(IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right)) :
		IsHeld(In::Forward))
	{
		if (IsHeld(In::Action))
		{
			auto vaultContext = TestLaraVault(item, coll);
			if (vaultContext.has_value())
			{
				item->Animation.TargetState = vaultContext->TargetState;
				SetLaraVault(item, coll, *vaultContext);
				return;
			}
		}

		if (player.Control.WaterStatus == WaterStatus::Dry)
		{
			item->Animation.TargetState = LS_RUN_FORWARD;
		}
		else
		{
			item->Animation.TargetState = LS_WADE_FORWARD;
		}

		return;
	}

	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_WADE_FORWARD (65)
// Control: lara_as_wade_forward()
void lara_col_wade_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	player.Control.HeadingOrient.y = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = !isWading;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (TestAndDoLaraLadderClimb(item, coll))
		return;

	if (LaraDeflectEdge(item, coll))
	{
		ResetPlayerLean(item);

		item->Animation.TargetState = LS_SOFT_SPLAT;
		if (GetStateDispatch(item, GetAnimData(*item)))
		{
			item->Animation.ActiveState = LS_SOFT_SPLAT;
			return;
		}

		LaraCollideStop(item, coll);
	}

	if (CanChangeElevation(*item, *coll) || isWading)
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_SPRINT (73)
// Collision: lara_col_sprint()
void lara_as_sprint(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::VerticalFlex;

	auto& player = GetLaraInfo(*item);

	bool isWading = (player.Control.WaterStatus == WaterStatus::Wade);

	player.Status.Stamina = std::clamp(player.Status.Stamina - 1, 0, (int)LARA_STAMINA_MAX);
	player.Control.Count.Run = std::clamp<unsigned int>(player.Control.Count.Run + 1, 0, PLAYER_TANK_CONTROL_SPRINT_JUMP_TIME);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	// Turn.
	if (IsUsingModernControls())
	{
		HandlePlayerTurn(*item, PLAYER_SPRINT_TURN_ALPHA, LARA_LEAN_MAX, false, TURN_FLAGS);
	}
	else
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX);
			HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE, LARA_LEAN_MAX);
		}
	}

	if (IsHeld(In::Jump) || player.Control.IsRunJumpQueued)
	{
		// TODO: CanSprintJumpForward() should handle HasSprintJump() check.
		if (IsHeld(In::Walk) || !g_GameFlow->HasSprintJump())
		{
			item->Animation.TargetState = LS_SPRINT_DIVE;
			return;
		}
		else if (IsHeld(In::Sprint) && CanSprintJumpForward(*item, *coll))
		{
			item->Animation.TargetState = LS_JUMP_FORWARD;
			return;
		}

		player.Control.IsRunJumpQueued = CanQueueRunningJump(*item, *coll);
	}

	if (IsHeld(In::Crouch) && CanCrouch(*item, *coll))
	{
		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	if (IsUsingModernControls() ?
		(IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right)) :
		IsHeld(In::Forward))
	{
		if (IsHeld(In::Action) && CanVaultFromSprint(*item, *coll))
		{
			auto vaultContext = TestLaraVault(item, coll);
			if (vaultContext.has_value())
			{
				item->Animation.TargetState = vaultContext->TargetState;
				SetLaraVault(item, coll, *vaultContext);
				return;
			}
		}

		if (isWading)
		{
			// TODO: Dispatch to wade forward state directly. --Sezz 2021.09.29
			item->Animation.TargetState = LS_RUN_FORWARD;
		}
		else if (IsHeld(In::Walk))
		{
			item->Animation.TargetState = LS_RUN_FORWARD;
		}
		else if (IsHeld(In::Sprint) && player.Status.Stamina > 0)
		{
			item->Animation.TargetState = LS_SPRINT;
		}
		else
		{
			item->Animation.TargetState = LS_RUN_FORWARD;
		}

		return;
	}

	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_SPRINT (73)
// Control: lara_as_sprint()
void lara_col_sprint(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HeadingOrient.y = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (LaraDeflectEdge(item, coll))
	{
		ResetPlayerLean(item);

		if (TestLaraWall(item, OFFSET_RADIUS(coll->Setup.Radius), -BLOCK(5 / 8.0f)) ||
			coll->HitTallObject)
		{
			item->Animation.TargetState = LS_SPLAT;
			if (GetStateDispatch(item, GetAnimData(*item)))
			{
				Rumble(0.5f, 0.15f);

				item->Animation.ActiveState = LS_SPLAT;
				return;
			}
		}

		item->Animation.TargetState = LS_SOFT_SPLAT;
		if (GetStateDispatch(item, GetAnimData(*item)))
		{
			item->Animation.ActiveState = LS_SOFT_SPLAT;
			return;
		}

		LaraCollideStop(item, coll);
	}

	if (TestAndDoLaraLadderClimb(item, coll))
		return;

	if (CanChangeElevation(*item, *coll) && coll->CollisionType != CollisionType::Front)
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_SPRINT_DIVE (74)
// Collision: lara_col_sprint_dive()
void lara_as_sprint_dive(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Count.Run = std::clamp<unsigned int>(player.Control.Count.Run + 1, 0, PLAYER_TANK_CONTROL_RUN_JUMP_TIME);

	// Turn.
	if (IsUsingClassicControls())
	{
		ResetPlayerTurnRateY(*item);
	}
	else
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX);
			HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE, LARA_LEAN_MAX * 0.6f);
		}
	}

	item->Animation.TargetState = LS_RUN_FORWARD;
}

// State:	LS_SPRINT_DIVE (74)
// Control: lara_as_sprint_dive()
void lara_col_sprint_dive(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HeadingOrient.y = (item->Animation.Velocity.z >= 0.0f) ? item->Pose.Orientation.y : item->Pose.Orientation.y + ANGLE(180.0f);
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	LaraDeflectEdgeJump(item, coll);

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (item->Animation.Velocity.z < 0.0f)
		player.Control.HeadingOrient.y = item->Pose.Orientation.y; // ???

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// TODO: Useful for having the player sweep under a gate. No default animation is currently included,
// but later one will be added to avoid making this an "insider feature". -- Sezz 2023.10.12
// State:	  LS_SPRINT_SLIDE (191)
// Collision: lara_col_sprint_slide()
void lara_as_sprint_slide(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;

	player.Control.Count.Run++;
	if (player.Control.Count.Run > PLAYER_TANK_CONTROL_RUN_JUMP_TIME)
		player.Control.Count.Run = PLAYER_TANK_CONTROL_RUN_JUMP_TIME;

	// Turn.
	if (IsUsingClassicControls())
	{
		ResetPlayerTurnRateY(*item);
	}
	else
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
		{
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_SLOW_TURN_RATE_MAX);
			HandlePlayerTurnLean(item, coll, LARA_LEAN_RATE, LARA_LEAN_MAX * 0.6f);
		}
	}

	if ((player.Control.KeepLow || IsHeld(In::Crouch)) && HasStateDispatch(item, LS_CROUCH_IDLE) &&
		CanCrouch(*item, *coll))
	{
		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	item->Animation.TargetState = LS_RUN_FORWARD;
}

// State:	LS_SPRINT_SLIDE (191)
// Control: lara_as_sprint_slide()
void lara_col_sprint_slide(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HeadingOrient.y = item->Pose.Orientation.y;
	player.Control.KeepLow = IsInLowSpace(*item, *coll);
	player.Control.IsLow = true;
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -(CLICK(1) - 1);
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.HeadingOrient.y;
	GetCollisionInfo(coll, item);

	LaraDeflectEdge(item, coll);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	if (CanChangeElevation(*item, *coll) && coll->CollisionType != CollisionType::Front)
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}
