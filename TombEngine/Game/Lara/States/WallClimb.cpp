#include "framework.h"
#include "Game/Lara/States/WallClimb.h"

#include "Game/camera.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/PlayerContext.h"
#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Entities::Player
{
	void lara_as_wall_climb_idle(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::Free;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableSpasm = false;
		coll->Setup.EnableObjectPush = false;
		Camera.targetElevation = ANGLE(-20.0f);

		if (item->HitPoints <= 0)
		{
			item->Animation.TargetState = LS_REACH;
			SetPlayerEdgeHangRelease(*item);
			return;
		}

		if (IsHeld(In::Action))
		{
			if (IsHeld(In::Crouch) && CanWallClimbToEdgeHang(*item, *coll))
			{
				item->Animation.TargetState = LS_EDGE_HANG_IDLE;
				return;
			}

			if (IsHeld(In::Jump))
			{
				item->Animation.TargetState = LS_JUMP_BACK;
				return;
			}

			if (IsHeld(In::Forward))
			{
				auto climbContext = GetWallClimbUpContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}
			else if (IsHeld(In::Back))
			{
				auto climbContext = GetWallClimbDownContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}

			if (IsHeld(In::Left) || IsHeld(In::StepLeft))
			{
				auto climbContext = GetWallClimbLeftContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}
			else if (IsHeld(In::Right) || IsHeld(In::StepRight))
			{
				auto climbContext = GetWallClimbRightContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}

			item->Animation.TargetState = LS_WALL_CLIMB_IDLE;
			return;
		}

		item->Animation.TargetState = LS_REACH;
		SetPlayerEdgeHangRelease(*item);
	}

	void lara_col_wall_climb_idle(ItemInfo* item, CollisionInfo* coll)
	{
		// ???
	}

	void lara_as_wall_climb_up(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::Horizontal;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableSpasm = false;
		coll->Setup.EnableObjectPush = false;
		Camera.targetElevation = ANGLE(30.0f);

		if (IsHeld(In::Action))
		{
			if (IsHeld(In::Forward) && TestLastFrame(item))
			{
				auto climbContext = GetWallClimbUpContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}

			item->Animation.TargetState = LS_WALL_CLIMB_IDLE;
			return;
		}

		item->Animation.TargetState = LS_REACH;
		SetPlayerEdgeHangRelease(*item);
	}

	void lara_col_wall_climb_up(ItemInfo* item, CollisionInfo* coll)
	{
		// ???
	}

	void lara_as_wall_climb_down(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::Horizontal;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableSpasm = false;
		coll->Setup.EnableObjectPush = false;
		Camera.targetElevation = ANGLE(-45.0f);

		if (IsHeld(In::Action))
		{
			if (IsHeld(In::Back) && TestLastFrame(item))
			{
				auto climbContext = GetWallClimbDownContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}

			item->Animation.TargetState = LS_WALL_CLIMB_IDLE;
			return;
		}

		item->Animation.TargetState = LS_REACH;
		SetPlayerEdgeHangRelease(*item);
	}

	void lara_col_wall_climb_down(ItemInfo* item, CollisionInfo* coll)
	{
		// ???
	}

	void lara_as_wall_climb_left(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::Horizontal;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableSpasm = false;
		coll->Setup.EnableObjectPush = false;
		Camera.targetAngle = ANGLE(-30.0f);
		Camera.targetElevation = ANGLE(-20.0f);

		if (IsHeld(In::Action))
		{
			if (IsHeld(In::Left) || IsHeld(In::StepLeft))
			{
				auto climbContext = GetWallClimbLeftContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}

			item->Animation.TargetState = LS_WALL_CLIMB_IDLE;
			return;
		}

		item->Animation.TargetState = LS_REACH;
		SetPlayerEdgeHangRelease(*item);
	}

	void lara_col_wall_climb_left(ItemInfo* item, CollisionInfo* coll)
	{
		// ???
	}

	void lara_as_wall_climb_right(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::Horizontal;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableSpasm = false;
		coll->Setup.EnableObjectPush = false;
		Camera.targetAngle = ANGLE(30.0f);
		Camera.targetElevation = ANGLE(-20.0f);

		if (IsHeld(In::Action))
		{
			if (IsHeld(In::Right) || IsHeld(In::StepRight))
			{
				auto climbContext = GetWallClimbRightContext(*item, *coll);
				if (climbContext.has_value())
				{
					item->Animation.TargetState = climbContext->TargetStateID;
					SetPlayerClimb(*item, *climbContext);
					return;
				}
			}

			item->Animation.TargetState = LS_WALL_CLIMB_IDLE;
			return;
		}

		item->Animation.TargetState = LS_REACH;
		SetPlayerEdgeHangRelease(*item);
	}

	void lara_col_wall_climb_right(ItemInfo* item, CollisionInfo* coll)
	{
		// ???
	}

	void lara_as_wall_climb_dismount_left(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::None;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;
		Camera.targetAngle = ANGLE(-30.0f);
		Camera.targetElevation = ANGLE(-20.0f);
	}

	void lara_as_wall_climb_dismount_right(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::None;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;
		Camera.targetAngle = ANGLE(30.0f);
		Camera.targetElevation = ANGLE(-20.0f);
	}

	void lara_as_wall_climb_end(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		// Setup
		player.Control.Look.Mode = LookMode::None;
		player.Control.IsClimbingWall = true;
		coll->Setup.EnableObjectPush = false;
		coll->Setup.EnableSpasm = false;
		Camera.flags = CF_FOLLOW_CENTER;
		Camera.targetAngle = ANGLE(-20.0f);
	}

	void lara_col_wall_climb_end(ItemInfo* item, CollisionInfo* coll)
	{
		return;
	}
}
