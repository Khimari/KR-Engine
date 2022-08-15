#include "framework.h"
#include "Game/Lara/lara_surface.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_swim.h"
#include "Game/Lara/lara_tests.h"
#include "Specific/level.h"
#include "Specific/input.h"

using namespace TEN::Input;

// -----------------------------
// WATER SURFACE TREAD
// Control & Collision Functions
// -----------------------------

// State:		LS_ONWATER_DIVE (35)
// Collision:	lara_col_surface_dive()
void lara_as_surface_dive(ItemInfo* item, CollisionInfo* coll)
{
	if (TrInput & IN_FORWARD)
		item->Pose.Orientation.x -= Angle::DegToRad(1.0f);
}

// State:		LS_ONWATER_DIVE (35)
// Control:		lara_as_surface_dive()
void lara_col_surface_dive(ItemInfo* item, CollisionInfo* coll)
{
	LaraSwimCollision(item, coll);
}

// State:		LS_ONWATER_IDLE (33)
// Collision:	lara_col_surface_idle()
void lara_as_surface_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	item->Animation.Velocity.y -= LARA_SWIM_VELOCITY_DECEL;
	if (item->Animation.Velocity.y < 0)
		item->Animation.Velocity.y = 0;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (TrInput & IN_LOOK)
	{
		LookUpDown(item);
		return;
	}

	if (TrInput & (IN_LEFT | IN_RIGHT))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_MED_TURN_RATE_MAX);

	if (DbInput & IN_JUMP)
	{
		SetLaraSwimDiveAnimation(item);
		return;
	}
	
	if (TrInput & IN_ROLL || (TrInput & IN_FORWARD && TrInput & IN_BACK))
	{
		item->Animation.TargetState = LS_ROLL_FORWARD;
		return;
	}

	if (TrInput & IN_FORWARD)
	{
		auto climbOutContext = TestLaraWaterClimbOut(item, coll);

		if (TrInput & IN_ACTION && climbOutContext.Success)
			SetContextWaterClimbOut(item, coll, climbOutContext);
		else
			item->Animation.TargetState = LS_ONWATER_FORWARD;
		
		return;
	}
	else if (TrInput & IN_BACK)
	{
		item->Animation.TargetState = LS_ONWATER_BACK;
		return;
	}
	
	if (TrInput & IN_LSTEP || (TrInput & IN_WALK && TrInput & IN_LEFT))
	{
		item->Animation.TargetState = LS_ONWATER_LEFT;
		return;
	}
	else if (TrInput & IN_RSTEP || (TrInput & IN_WALK && TrInput & IN_RIGHT))
	{
		item->Animation.TargetState = LS_ONWATER_RIGHT;
		return;
	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_IDLE (33)
// Control:		lara_as_surface_idle()
void lara_col_surface_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y;
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_FORWARD (34)
// Collision:	lara_col_surface_swim_forward()
void lara_as_surface_swim_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (TrInput & (IN_LEFT | IN_RIGHT))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_MED_TURN_RATE_MAX);

	if (DbInput & IN_JUMP)
		SetLaraSwimDiveAnimation(item);

	// TODO
	/*if (TrInput & IN_ROLL || (TrInput & IN_FORWARD && TrInput & IN_BACK))
	{
		item->Animation.TargetState = LS_ROLL_FORWARD;
		return;
	}*/

	if (TrInput & IN_FORWARD)
	{
		auto climbOutContext = TestLaraWaterClimbOut(item, coll);
		
		if (TrInput & IN_ACTION && climbOutContext.Success)
		{
			//item->Animation.TargetState = climbOutContext.TargetState; // TODO
			SetContextWaterClimbOut(item, coll, climbOutContext);
			return;
		}
		else
		{
			item->Animation.TargetState = LS_ONWATER_FORWARD;

			item->Animation.Velocity.y += LARA_SWIM_VELOCITY_ACCEL;
			if (item->Animation.Velocity.y > LARA_TREAD_VELOCITY_MAX)
				item->Animation.Velocity.y = LARA_TREAD_VELOCITY_MAX;
		}

		return;
	}
	
	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_FORWARD (34)
// Control:		lara_as_surface_swim_forward()
void lara_col_surface_swim_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	LaraSurfaceCollision(item, coll);

	TestLaraLadderClimbOut(item, coll);
}

// State:		LS_ONWATER_LEFT (48)
// Collision:	lara_col_surface_swim_left()
void lara_as_surface_swim_left(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (!(TrInput & IN_WALK))	// WALK locks orientation.
	{
		if (TrInput & (IN_LEFT | IN_RIGHT))
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_SLOW_MED_TURN_RATE_MAX);
	}

	if (DbInput & IN_JUMP)
		SetLaraSwimDiveAnimation(item);

	if (TrInput & IN_LSTEP || (TrInput & IN_WALK && TrInput & IN_LEFT))
	{
		item->Animation.TargetState = LS_ONWATER_LEFT;

		item->Animation.Velocity.y += LARA_SWIM_VELOCITY_ACCEL;
		if (item->Animation.Velocity.y > LARA_TREAD_VELOCITY_MAX)
			item->Animation.Velocity.y = LARA_TREAD_VELOCITY_MAX;

		return;

	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_LEFT (48)
// Control:		lara_as_surface_swim_left()
void lara_col_surface_swim_left(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y - Angle::DegToRad(90.0f);
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_RIGHT (49)
// Collision:	lara_col_surface_swim_right()
void lara_as_surface_swim_right(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (!(TrInput & IN_WALK))	// WALK locks orientation.
	{
		if (TrInput & (IN_LEFT | IN_RIGHT))
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_SLOW_MED_TURN_RATE_MAX);
	}

	if (DbInput & IN_JUMP)
		SetLaraSwimDiveAnimation(item);

	if (TrInput & IN_RSTEP || (TrInput & IN_WALK && TrInput & IN_RIGHT))
	{
		item->Animation.TargetState = LS_ONWATER_RIGHT;

		item->Animation.Velocity.y += LARA_SWIM_VELOCITY_ACCEL;
		if (item->Animation.Velocity.y > LARA_TREAD_VELOCITY_MAX)
			item->Animation.Velocity.y = LARA_TREAD_VELOCITY_MAX;

		return;
	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_RIGHT (49)
// Conrol:		lara_as_surface_swim_right()
void lara_col_surface_swim_right(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y + Angle::DegToRad(90.0f);
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_BACK (47)
// Collision:	lara_col_surface_swim_back()
void lara_as_surface_swim_back(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (TrInput & (IN_LEFT | IN_RIGHT))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_SLOW_MED_TURN_RATE_MAX);

	if (DbInput & IN_JUMP)
		SetLaraSwimDiveAnimation(item);

	if (TrInput & IN_BACK)
	{
		item->Animation.TargetState = LS_ONWATER_BACK;

		item->Animation.Velocity.y += LARA_SWIM_VELOCITY_ACCEL;
		if (item->Animation.Velocity.y > LARA_TREAD_VELOCITY_MAX)
			item->Animation.Velocity.y = LARA_TREAD_VELOCITY_MAX;

		return;
	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_BACK (47)
// Control:		lara_as_surface_swim_back()
void lara_col_surface_swim_back(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y + Angle::DegToRad(180.0f);
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_EXIT (55)
// Collision:	lara_default_col()
void lara_as_surface_climb_out(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.flags = CF_FOLLOW_CENTER;
	Camera.laraNode = LM_HIPS;	// Forces the camera to follow Lara instead of snapping.

	EaseOutLaraHeight(item, lara->ProjectedFloorHeight - item->Pose.Position.y);
	item->Pose.Orientation.InterpolateLinear(lara->TargetOrientation, 0.4f, Angle::DegToRad(0.1f));
	item->Animation.TargetState = LS_IDLE;
}
