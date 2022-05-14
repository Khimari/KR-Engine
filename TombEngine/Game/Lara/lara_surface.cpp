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

// -----------------------------
// WATER SURFACE TREAD
// Control & Collision Functions
// -----------------------------

// State:		LS_ONWATER_DIVE (35)
// Collision:	lara_col_surface_dive()
void lara_as_surface_dive(ItemInfo* item, CollisionInfo* coll)
{
	if (TrInput & IN_FORWARD)
		item->Pose.Orientation.SetX(Angle::Normalize(item->Pose.Orientation.GetX() - Angle::DegToRad(1.0f)));
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

	item->Animation.VerticalVelocity -= LARA_SWIM_DECELERATION;
	if (item->Animation.VerticalVelocity < 0)
		item->Animation.VerticalVelocity = 0;

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

	if (TrInput & IN_LEFT)
	{
		lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() - LARA_TURN_RATE * 1.25f);
		if (lara->Control.TurnRate.GetY() < -LARA_MED_TURN_MAX)
			lara->Control.TurnRate.SetY(-LARA_MED_TURN_MAX);
	}
	else if (TrInput & IN_RIGHT)
	{
		lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() + LARA_TURN_RATE * 1.25f);
		if (lara->Control.TurnRate.GetY() > LARA_MED_TURN_MAX)
			lara->Control.TurnRate.SetY(LARA_MED_TURN_MAX);
	}

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

	lara->Control.MoveAngle = item->Pose.Orientation.GetY();
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
	
	if (TrInput & IN_LEFT)
	{
		lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() - LARA_TURN_RATE * 1.25f);
		if (lara->Control.TurnRate.GetY() < -LARA_MED_TURN_MAX)
			lara->Control.TurnRate.SetY(-LARA_MED_TURN_MAX);
	}
	else if (TrInput & IN_RIGHT)
	{
		lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() + LARA_TURN_RATE * 1.25f);
		if (lara->Control.TurnRate.GetY() > LARA_MED_TURN_MAX)
			lara->Control.TurnRate.SetY(LARA_MED_TURN_MAX);
	}

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

			item->Animation.VerticalVelocity += LARA_SWIM_ACCELERATION;
			if (item->Animation.VerticalVelocity > LARA_TREAD_VELOCITY_MAX)
				item->Animation.VerticalVelocity = LARA_TREAD_VELOCITY_MAX;
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

	lara->Control.MoveAngle = item->Pose.Orientation.GetY();
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
		if (TrInput & IN_LEFT)
		{
			lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() - LARA_TURN_RATE * 1.25f);
			if (lara->Control.TurnRate.GetY() < -LARA_SLOW_MED_TURN_MAX)
				lara->Control.TurnRate.SetY(-LARA_SLOW_MED_TURN_MAX);
		}
		else if (TrInput & IN_RIGHT)
		{
			lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() + LARA_TURN_RATE * 1.25f);
			if (lara->Control.TurnRate.GetY() > LARA_SLOW_MED_TURN_MAX)
				lara->Control.TurnRate.SetY(LARA_SLOW_MED_TURN_MAX);
		}
	}

	if (DbInput & IN_JUMP)
		SetLaraSwimDiveAnimation(item);

	if (TrInput & IN_LSTEP || (TrInput & IN_WALK && TrInput & IN_LEFT))
	{
		item->Animation.TargetState = LS_ONWATER_LEFT;

		item->Animation.VerticalVelocity += LARA_SWIM_ACCELERATION;
		if (item->Animation.VerticalVelocity > LARA_TREAD_VELOCITY_MAX)
			item->Animation.VerticalVelocity = LARA_TREAD_VELOCITY_MAX;

		return;

	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_LEFT (48)
// Control:		lara_as_surface_swim_left()
void lara_col_surface_swim_left(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.GetY() - Angle::DegToRad(90.0f);
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
		if (TrInput & IN_LEFT)
		{
			lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() - LARA_TURN_RATE * 1.25f);
			if (lara->Control.TurnRate.GetY() < -LARA_SLOW_MED_TURN_MAX)
				lara->Control.TurnRate.SetY(-LARA_SLOW_MED_TURN_MAX);
		}
		else if (TrInput & IN_RIGHT)
		{
			lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() + LARA_TURN_RATE * 1.25f);
			if (lara->Control.TurnRate.GetY() > LARA_SLOW_MED_TURN_MAX)
				lara->Control.TurnRate.SetY(LARA_SLOW_MED_TURN_MAX);
		}
	}

	if (DbInput & IN_JUMP)
		SetLaraSwimDiveAnimation(item);

	if (TrInput & IN_RSTEP || (TrInput & IN_WALK && TrInput & IN_RIGHT))
	{
		item->Animation.TargetState = LS_ONWATER_RIGHT;

		item->Animation.VerticalVelocity += LARA_SWIM_ACCELERATION;
		if (item->Animation.VerticalVelocity > LARA_TREAD_VELOCITY_MAX)
			item->Animation.VerticalVelocity = LARA_TREAD_VELOCITY_MAX;

		return;
	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_RIGHT (49)
// Conrol:		lara_as_surface_swim_right()
void lara_col_surface_swim_right(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.GetY() + Angle::DegToRad(90.0f);
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

	if (TrInput & IN_LEFT)
	{
		lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() - LARA_TURN_RATE * 1.25f);
		if (lara->Control.TurnRate.GetY() < -LARA_SLOW_MED_TURN_MAX)
			lara->Control.TurnRate.SetY(-LARA_SLOW_MED_TURN_MAX);
	}
	else if (TrInput & IN_RIGHT)
	{
		lara->Control.TurnRate.SetY(lara->Control.TurnRate.GetY() + LARA_TURN_RATE * 1.25f);
		if (lara->Control.TurnRate.GetY() > LARA_SLOW_MED_TURN_MAX)
			lara->Control.TurnRate.SetY(LARA_SLOW_MED_TURN_MAX);
	}

	if (DbInput & IN_JUMP)
		SetLaraSwimDiveAnimation(item);

	if (TrInput & IN_BACK)
	{
		item->Animation.TargetState = LS_ONWATER_BACK;

		item->Animation.VerticalVelocity += LARA_SWIM_ACCELERATION;
		if (item->Animation.VerticalVelocity > LARA_TREAD_VELOCITY_MAX)
			item->Animation.VerticalVelocity = LARA_TREAD_VELOCITY_MAX;

		return;
	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_BACK (47)
// Control:		lara_as_surface_swim_back()
void lara_col_surface_swim_back(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.GetY() + Angle::DegToRad(180.0f);
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
