#include "framework.h"
#include "Game/Lara/States/EdgeHang.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/items.h"
#include "Game/Lara/Context.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_overhang.h"
#include "Game/Lara/lara_tests.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;

namespace TEN::Entities::Player
{
	// State:	  LS_HANG_IDLE (10)
	// Collision: lara_col_hang_idle()
	void lara_as_hang_idle(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		player.Control.IsClimbingLadder = false;
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;
		Camera.targetAngle = 0;
		Camera.targetElevation = ANGLE(-45.0f);

		if (item->HitPoints <= 0)
		{
			SetPlayerEdgeHangRelease(*item);
			return;
		}

		if (IsHeld(In::Look))
			LookUpDown(item);

		if (IsHeld(In::Action))
		{
			if (TestLaraClimbIdle(item, coll))
			{
				item->Animation.TargetState = LS_LADDER_IDLE;
				return;
			}

			if (IsHeld(In::Jump) && Context::CanPerformLedgeJump(*item, *coll))
			{
				if (IsHeld(In::Back))
				{
					item->Animation.TargetState = LS_JUMP_FORWARD;
				}
				else
				{
					item->Animation.TargetState = LS_JUMP_UP;
				}

				return;
			}

			if (IsHeld(In::Forward))
			{
				if (Context::CanClimbLedgeToCrouch(*item, *coll))
				{
					item->Animation.TargetState = LS_HANG_TO_CROUCH;
					return;
				}
				else if (Context::CanClimbLedgeToStand(*item, *coll))
				{
					if (IsHeld(In::Crouch))
					{
						item->Animation.TargetState = LS_HANG_TO_CROUCH;
					}
					else if (IsHeld(In::Walk) && Context::CanPerformLedgeHandstand(*item, *coll))
					{
						item->Animation.TargetState = LS_HANDSTAND;
					}
					else
					{
						item->Animation.TargetState = LS_GRABBING;
					}

					return;
				}

				if (Context::CanShimmyUp(*item, *coll))
				{
					// TODO: State dispatch.
					SetAnimation(item, LA_LADDER_SHIMMY_UP);
					return;
				}
			}
			else if (IsHeld(In::Back))
			{
				if (Context::CanShimmyDown(*item, *coll))
				{
					// TODO: State dispatch.
					SetAnimation(item, LA_LADDER_SHIMMY_UP);
					return;
				}
			}

			if (IsHeld(In::Left) || IsHeld(In::LeftStep))
			{
				if (Context::CanShimmyLeft(*item, *coll) && HasStateDispatch(item, LS_SHIMMY_LEFT))
				{
					item->Animation.TargetState = LS_SHIMMY_LEFT;
					return;
				}

				auto cornerShimmyState = GetPlayerCornerShimmyState(*item, *coll);
				if (cornerShimmyState.has_value())
				{
					item->Animation.TargetState = cornerShimmyState.value();
					return;
				}
			}
			else if (IsHeld(In::Right) || IsHeld(In::RightStep))
			{
				if (Context::CanShimmyRight(*item, *coll) && HasStateDispatch(item, LS_SHIMMY_RIGHT))
				{
					item->Animation.TargetState = LS_SHIMMY_RIGHT;
					return;
				}

				auto cornerShimmyState = GetPlayerCornerShimmyState(*item, *coll);
				if (cornerShimmyState.has_value())
				{
					item->Animation.TargetState = cornerShimmyState.value();
					return;
				}
			}

			item->Animation.TargetState = LS_HANG_IDLE;
			return;
		}

		SetPlayerEdgeHangRelease(*item);
	}

	// State:	LS_HANG_IDLE (10)
	// Control: lara_as_hang_idle()
	void lara_col_hang_idle(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = *GetLaraInfo(item);

		item->Animation.IsAirborne = false;
		item->Animation.Velocity.y = 0.0f;
		player.Control.MoveAngle = item->Pose.Orientation.y;
		coll->Setup.Mode = CollisionProbeMode::FreeFlat;
		coll->Setup.Height = LARA_HEIGHT_STRETCH;

		HandlePlayerEdgeHang(item, coll);
	}

	// State:	  LS_SHIMMY_LEFT (30)
	// Collision: lara_col_shimmy_left()
	void lara_as_shimmy_left(ItemInfo* item, CollisionInfo* coll)
	{
		coll->Setup.Mode = CollisionProbeMode::FreeFlat;
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;
		coll->Setup.Height = LARA_HEIGHT_STRETCH;
		Camera.targetAngle = 0;
		Camera.targetElevation = ANGLE(-45.0f);

		if (item->HitPoints <= 0)
		{
			SetPlayerEdgeHangRelease(*item);
			return;
		}

		if (IsHeld(In::Action))
		{
			if (IsHeld(In::Left) || IsHeld(In::LeftStep))
			{
				item->Animation.TargetState = LS_SHIMMY_LEFT;
				return;
			}

			item->Animation.TargetState = LS_HANG_IDLE;
			return;
		}

		SetPlayerEdgeHangRelease(*item);
	}

	// State:	LS_SHIMMY_LEFT (30)
	// Control: lara_as_shimmy_left()
	void lara_col_shimmy_left(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		player.Control.MoveAngle = item->Pose.Orientation.y - ANGLE(90.0f);
		coll->Setup.Radius = LARA_RADIUS;

		HandlePlayerEdgeHang(item, coll);
		player.Control.MoveAngle = item->Pose.Orientation.y - ANGLE(90.0f);
	}

	// State:	  LS_SHIMMY_RIGHT (31)
	// Collision: lara_col_shimmy_right()
	void lara_as_shimmy_right(ItemInfo* item, CollisionInfo* coll)
	{
		coll->Setup.Mode = CollisionProbeMode::FreeFlat;
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;
		coll->Setup.Height = LARA_HEIGHT_STRETCH;
		Camera.targetAngle = 0;
		Camera.targetElevation = ANGLE(-45.0f);

		if (item->HitPoints <= 0)
		{
			SetPlayerEdgeHangRelease(*item);
			return;
		}

		if (IsHeld(In::Action))
		{
			if (IsHeld(In::Right) || IsHeld(In::RightStep))
			{
				item->Animation.TargetState = LS_SHIMMY_RIGHT;
				return;
			}

			item->Animation.TargetState = LS_HANG_IDLE;
			return;
		}

		SetPlayerEdgeHangRelease(*item);
	}

	// State:	LS_SHIMMY_RIGHT (31)
	// Control: lara_as_shimmy_right()
	void lara_col_shimmy_right(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		player.Control.MoveAngle = item->Pose.Orientation.y + ANGLE(90.0f);
		coll->Setup.Radius = LARA_RADIUS;

		HandlePlayerEdgeHang(item, coll);
		player.Control.MoveAngle = item->Pose.Orientation.y + ANGLE(90.0f);
	}

	// State:	  LS_SHIMMY_OUTER_LEFT (107), LS_SHIMMY_OUTER_RIGHT (108), LS_SHIMMY_INNER_LEFT (109), LS_SHIMMY_INNER_RIGHT (110)
	// Collision: lara_default_col()
	void lara_as_shimmy_corner(ItemInfo* item, CollisionInfo* coll)
	{
		coll->Setup.Height = LARA_HEIGHT_STRETCH;
		Camera.targetAngle = 0;
		Camera.targetElevation = ANGLE(-33.0f);
		Camera.laraNode = LM_TORSO;

		SetPlayerCornerShimmyEnd(*item, *coll, TestLastFrame(item));
	}

	// State:	  LS_HANDSTAND (54)
	// Collision: lara_default_col()
	void lara_as_handstand(ItemInfo* item, CollisionInfo* coll)
	{
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;
	}
}
