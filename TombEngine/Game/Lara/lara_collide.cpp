#include "framework.h"
#include "Game/Lara/lara_collide.h"

#include "Game/animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_swim.h"
#include "Game/Lara/lara_tests.h"
#include "Specific/input.h"
#include "Specific/level.h"
#include "Specific/setup.h"
#include "Flow/ScriptInterfaceFlowHandler.h"
#include "ScriptInterfaceLevel.h"

using namespace TEN::Input;

// -----------------------------
// COLLISION TEST FUNCTIONS
// For State Control & Collision
// -----------------------------

bool LaraDeflectEdge(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (coll->CollisionType == CT_FRONT || coll->CollisionType == CT_TOP_FRONT)
	{
		ShiftItem(item, coll);
		item->Animation.TargetState = LS_IDLE;
		item->Animation.Velocity.z = 0;
		return true;
	}

	if (TrInput & (IN_LEFT | IN_RIGHT))
	{
		ShiftItem(item, coll);
		return false;
	}

	if (coll->CollisionType == CT_LEFT)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y += Angle::DegToRad(coll->DiagonalStepAtLeft() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE);
	}
	else if (coll->CollisionType == CT_RIGHT)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y -= Angle::DegToRad(coll->DiagonalStepAtRight() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE);
	}

	return false;
}

bool LaraDeflectEdgeJump(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (coll->CollisionType != CT_NONE)
		ShiftItem(item, coll);

	if (coll->CollisionType == CT_FRONT || coll->CollisionType == CT_TOP_FRONT)
	{
		if (!lara->Control.CanClimbLadder || item->Animation.Velocity.z != 2)
		{
			if (coll->Middle.Floor <= CLICK(1))
			{
				SetAnimation(item, LA_LAND);
				LaraSnapToHeight(item, coll);
			}
			else if (abs(item->Animation.Velocity.z) > 47)
			{
				// TODO: Demagic. This is Lara's running velocity. Jumps have a minimum of 50.
				SetAnimation(item, LA_JUMP_WALL_SMASH_START, 1);
				Rumble(0.5f, 0.15f);
			}

			item->Animation.Velocity.z /= 4;
			lara->Control.MoveAngle += Angle::DegToRad(180.0f);

			if (item->Animation.Velocity.y <= 0)
				item->Animation.Velocity.y = 1;
		}

		return true;
	}

	switch (coll->CollisionType)
	{
	case CT_LEFT:
		item->Pose.Orientation.y += Angle::DegToRad(DEFLECT_STRAIGHT_ANGLE);
		break;

	case CT_RIGHT:
		item->Pose.Orientation.y -= Angle::DegToRad(DEFLECT_STRAIGHT_ANGLE);
		break;

	case CT_TOP:
	case CT_TOP_FRONT:
		if (item->Animation.Velocity.y <= 0)
			item->Animation.Velocity.y = 1;

		break;

	case CT_CLAMP:
		TranslateItem(item, item->Pose.Orientation.y + Angle::DegToRad(180.0f), CLICK(1.5f), 0, 0);
		item->Animation.Velocity.z = 0;
		coll->Middle.Floor = 0;

		if (item->Animation.Velocity.y <= 0)
			item->Animation.Velocity.y = 16;

		break;
	}

	return false;
}

void LaraSlideEdgeJump(ItemInfo* item, CollisionInfo* coll)
{
	ShiftItem(item, coll);

	switch (coll->CollisionType)
	{
	case CT_LEFT:
		item->Pose.Orientation.y += Angle::DegToRad(DEFLECT_STRAIGHT_ANGLE);
		break;

	case CT_RIGHT:
		item->Pose.Orientation.y -= Angle::DegToRad(DEFLECT_STRAIGHT_ANGLE);
		break;

	case CT_FRONT:
		item->Animation.Velocity.z = 0;
		break;

	case CT_TOP:
	case CT_TOP_FRONT:
		if (item->Animation.Velocity.y <= 0)
			item->Animation.Velocity.y = 1;

		break;

	case CT_CLAMP:
		TranslateItem(item, item->Pose.Orientation.y + Angle::DegToRad(180.0f), CLICK(1.5f), 0, 0);
		item->Animation.Velocity.z = 0;
		coll->Middle.Floor = 0;

		if (item->Animation.Velocity.y <= 0)
			item->Animation.Velocity.y = 16;

		break;
	}
}

bool LaraDeflectEdgeCrawl(ItemInfo* item, CollisionInfo* coll)
{
	// Useless in the best case; Lara does not have to embed in order to perform climbing actions in crawl states. Keeping for security. @Sezz 2021.11.26
	if (coll->CollisionType == CT_FRONT || coll->CollisionType == CT_TOP_FRONT)
	{
		ShiftItem(item, coll);

		item->Animation.Velocity.z = 0;
		item->Animation.IsAirborne = false;
		return true;
	}

	if (TrInput & (IN_LEFT | IN_RIGHT))
	{
		ShiftItem(item, coll);
		return false;
	}

	if (coll->CollisionType == CT_LEFT)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y += Angle::DegToRad(coll->DiagonalStepAtLeft() ? DEFLECT_DIAGONAL_ANGLE_CRAWL : DEFLECT_STRAIGHT_ANGLE_CRAWL);
	}
	else if (coll->CollisionType == CT_RIGHT)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y -= Angle::DegToRad(coll->DiagonalStepAtRight() ? DEFLECT_DIAGONAL_ANGLE_CRAWL : DEFLECT_STRAIGHT_ANGLE_CRAWL);
	}

	return false;
}

bool LaraDeflectEdgeMonkey(ItemInfo* item, CollisionInfo* coll)
{
	// HACK
	if (coll->Shift.y >= 0 && coll->Shift.y <= CLICK(1.25f))
		coll->Shift.y = 0;

	if (coll->CollisionType == CT_FRONT || coll->CollisionType == CT_TOP_FRONT ||
		coll->HitTallObject)
	{
		ShiftItem(item, coll);

		item->Animation.TargetState = LS_MONKEY_IDLE;
		item->Animation.Velocity.z = 0;
		item->Animation.IsAirborne = false;
		return true;
	}

	if (TrInput & (IN_LEFT | IN_RIGHT))
	{
		ShiftItem(item, coll);
		return false;
	}

	if (coll->CollisionType == CT_LEFT)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y += Angle::DegToRad(coll->DiagonalStepAtLeft() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE);
	}
	else if (coll->CollisionType == CT_RIGHT)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y -= Angle::DegToRad(coll->DiagonalStepAtRight() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE);
	}

	return false;
}

void LaraCollideStop(ItemInfo* item, CollisionInfo* coll)
{
	switch (coll->Setup.OldState)
	{
	case LS_IDLE:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_FAST:
	case LS_TURN_LEFT_FAST:
		item->Animation.ActiveState = coll->Setup.OldState;
		item->Animation.AnimNumber = coll->Setup.OldAnimNumber;
		item->Animation.FrameNumber = coll->Setup.OldFrameNumber;

		if (TrInput & IN_LEFT)
		{
			// Prevent turn lock against walls.
			if (item->Animation.ActiveState == LS_TURN_RIGHT_SLOW ||
				item->Animation.ActiveState == LS_TURN_RIGHT_FAST)
			{
				item->Animation.TargetState = LS_IDLE;
			}
			else
				item->Animation.TargetState = LS_TURN_LEFT_SLOW;
		}
		else if (TrInput & IN_RIGHT)
		{
			if (item->Animation.ActiveState == LS_TURN_LEFT_SLOW ||
				item->Animation.ActiveState == LS_TURN_LEFT_FAST)
			{
				item->Animation.TargetState = LS_IDLE;
			}
			else
				item->Animation.TargetState = LS_TURN_RIGHT_SLOW;
		}
		else
			item->Animation.TargetState = LS_IDLE;

		AnimateLara(item);
		break;

	default:
		item->Animation.TargetState = LS_IDLE;

		if (item->Animation.AnimNumber != LA_STAND_SOLID)
			SetAnimation(item, LA_STAND_SOLID);

		break;
	}
}

void LaraCollideStopCrawl(ItemInfo* item, CollisionInfo* coll)
{
	switch (coll->Setup.OldState)
	{
	case LS_CRAWL_IDLE:
	case LS_CRAWL_TURN_LEFT:
	case LS_CRAWL_TURN_RIGHT:
		item->Animation.ActiveState = coll->Setup.OldState;
		item->Animation.AnimNumber = coll->Setup.OldAnimNumber;
		item->Animation.FrameNumber = coll->Setup.OldFrameNumber;

		if (TrInput & IN_LEFT)
			item->Animation.TargetState = LS_CRAWL_TURN_LEFT;
		else if (TrInput & IN_RIGHT)
			item->Animation.TargetState = LS_CRAWL_TURN_RIGHT;
		else
			item->Animation.TargetState = LS_CRAWL_IDLE;

		AnimateLara(item);
		break;

	default:
		item->Animation.ActiveState = LS_CRAWL_IDLE;
		item->Animation.TargetState = LS_CRAWL_IDLE;

		if (item->Animation.AnimNumber != LA_CRAWL_IDLE)
		{
			item->Animation.AnimNumber = LA_CRAWL_IDLE;
			item->Animation.FrameNumber = GetFrameNumber(item, 0);
		}

		break;
	}
}

void LaraCollideStopMonkey(ItemInfo* item, CollisionInfo* coll)
{
	switch (coll->Setup.OldState)
	{
	case LS_MONKEY_IDLE:
	case LS_MONKEY_TURN_LEFT:
	case LS_MONKEY_TURN_RIGHT:
		item->Animation.ActiveState = coll->Setup.OldState;
		item->Animation.AnimNumber = coll->Setup.OldAnimNumber;
		item->Animation.FrameNumber = coll->Setup.OldFrameNumber;

		if (TrInput & IN_LEFT)
			item->Animation.TargetState = LS_MONKEY_TURN_LEFT;
		else if (TrInput & IN_RIGHT)
			item->Animation.TargetState = LS_MONKEY_TURN_RIGHT;
		else
			item->Animation.TargetState = LS_MONKEY_IDLE;

		AnimateLara(item);
		break;

	default:
		item->Animation.ActiveState = LS_MONKEY_IDLE;
		item->Animation.TargetState = LS_MONKEY_IDLE;

		if (item->Animation.AnimNumber != LA_MONKEY_IDLE)
		{
			item->Animation.AnimNumber = LA_MONKEY_IDLE;
			item->Animation.FrameNumber = GetFrameNumber(item, 0);
		}

		break;
	}
}

void LaraSnapToEdgeOfBlock(ItemInfo* item, CollisionInfo* coll, float angle)
{
	// Snapping distance of Lara's radius + 12 units is seemingly empirical value from Core tests.
	int snapDistance = coll->Setup.Radius + 12;

	if (item->Animation.ActiveState == LS_SHIMMY_RIGHT)
	{
		switch (Angle::RadToShrt(angle)) // TODO
		{
		case NORTH:
			item->Pose.Position.x = (coll->Setup.OldPosition.x & ~(WALL_SIZE - 1)) | (WALL_SIZE - snapDistance);
			return;

		case EAST:
			item->Pose.Position.z = (coll->Setup.OldPosition.z & ~(WALL_SIZE - 1)) | snapDistance;
			return;

		case SOUTH:
			item->Pose.Position.x = (coll->Setup.OldPosition.x & ~(WALL_SIZE - 1)) | snapDistance;
			return;

		case WEST:
		default:
			item->Pose.Position.z = (coll->Setup.OldPosition.z & ~(WALL_SIZE - 1)) | (WALL_SIZE - snapDistance);
			return;
		}
	}

	if (item->Animation.ActiveState == LS_SHIMMY_LEFT)
	{
		switch (Angle::RadToShrt(angle))
		{
		case NORTH:
			item->Pose.Position.x = (coll->Setup.OldPosition.x & ~(WALL_SIZE - 1)) | snapDistance;
			return;

		case EAST:
			item->Pose.Position.z = (coll->Setup.OldPosition.z & ~(WALL_SIZE - 1)) | (WALL_SIZE - snapDistance);
			return;

		case SOUTH:
			item->Pose.Position.x = (coll->Setup.OldPosition.x & ~(WALL_SIZE - 1)) | (WALL_SIZE - snapDistance);
			return;

		case WEST:
		default:
			item->Pose.Position.z = (coll->Setup.OldPosition.z & ~(WALL_SIZE - 1)) | snapDistance;
			return;
		}
	}
}

void LaraResetGravityStatus(ItemInfo* item, CollisionInfo* coll)
{
	// This routine cleans gravity status flag and VerticalVelocity, making it
	// impossible to perform bugs such as QWOP and flare jump. Found by Troye -- Lwmte, 25.09.2021

	if (coll->Middle.Floor <= STEPUP_HEIGHT)
	{
		item->Animation.IsAirborne = false;
		item->Animation.Velocity.y = 0;
	}
}

void LaraSnapToHeight(ItemInfo* item, CollisionInfo* coll)
{
	if (TestEnvironment(ENV_FLAG_SWAMP, item) && coll->Middle.Floor > 0)
		item->Pose.Position.y += SWAMP_GRAVITY;
	else if (coll->Middle.Floor != NO_HEIGHT)
		item->Pose.Position.y += coll->Middle.Floor;
}

void GetLaraDeadlyBounds()
{
	BOUNDING_BOX tBounds;
	auto* bounds = GetBoundsAccurate(LaraItem);
	phd_RotBoundingBoxNoPersp(&LaraItem->Pose, bounds, &tBounds);

	DeadlyBounds[0] = LaraItem->Pose.Position.x + tBounds.X1;
	DeadlyBounds[1] = LaraItem->Pose.Position.x + tBounds.X2;
	DeadlyBounds[2] = LaraItem->Pose.Position.y + tBounds.Y1;
	DeadlyBounds[3] = LaraItem->Pose.Position.y + tBounds.Y2;
	DeadlyBounds[4] = LaraItem->Pose.Position.z + tBounds.Z1;
	DeadlyBounds[5] = LaraItem->Pose.Position.z + tBounds.Z2;
}

void LaraJumpCollision(ItemInfo* item, CollisionInfo* coll, float moveAngle)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = moveAngle;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = lara->Control.MoveAngle;
	GetCollisionInfo(coll, item);

	LaraDeflectEdgeJump(item, coll);
}

void LaraSurfaceCollision(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	coll->Setup.ForwardAngle = lara->Control.MoveAngle;

	GetCollisionInfo(coll, item, Vector3Int(0, LARA_HEIGHT_TREAD, 0));
	ShiftItem(item, coll);

	if (coll->CollisionType & (CT_FRONT | CT_TOP | CT_TOP_FRONT | CT_CLAMP) ||
		coll->Middle.Floor < 0 && coll->Middle.FloorSlope)
	{
		item->Animation.Velocity.y = 0;
		item->Pose.Position = coll->Setup.OldPosition;
	}
	else if (coll->CollisionType == CT_LEFT)
		item->Pose.Orientation.y += Angle::DegToRad(5.0f);
	else if (coll->CollisionType == CT_RIGHT)
		item->Pose.Orientation.y -= Angle::DegToRad(5.0f);

	if ((GetWaterHeight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, item->RoomNumber) - item->Pose.Position.y) > -100)
		TestLaraWaterStepOut(item, coll);
	else
		SetLaraSwimDiveAnimation(item);
}

void LaraSwimCollision(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	auto oldPose = item->Pose;

	if (item->Pose.Orientation.x < Angle::DegToRad(-90.0f) ||
		item->Pose.Orientation.x > Angle::DegToRad(90.0f))
	{
		lara->Control.MoveAngle = item->Pose.Orientation.y + Angle::DegToRad(180.0f);
		coll->Setup.ForwardAngle = item->Pose.Orientation.y - Angle::DegToRad(180.0f);
	}
	else
	{
		lara->Control.MoveAngle = item->Pose.Orientation.y;
		coll->Setup.ForwardAngle = item->Pose.Orientation.y;
	}

	int height = LARA_HEIGHT * sin(item->Pose.Orientation.x);
	height = abs(height);

	auto level = g_GameFlow->GetLevel(CurrentLevel);
	if (height < ((level->GetLaraType() == LaraType::Divesuit) << 6) + 200)
		height = ((level->GetLaraType() == LaraType::Divesuit) << 6) + 200;

	coll->Setup.UpperFloorBound = -CLICK(0.25f);
	coll->Setup.Height = height;

	GetCollisionInfo(coll, item, Vector3Int(0, height / 2, 0));

	auto c1 = *coll;
	c1.Setup.ForwardAngle = c1.Setup.ForwardAngle + Angle::DegToRad(45.0f);
	GetCollisionInfo(&c1, item, Vector3Int(0, height / 2, 0));

	auto c2 = *coll;
	c2.Setup.ForwardAngle = c2.Setup.ForwardAngle - Angle::DegToRad(45.0f);
	GetCollisionInfo(&c2, item, Vector3Int(0, height / 2, 0));

	ShiftItem(item, coll);

	int flag = 0;

	switch (coll->CollisionType)
	{
	case CT_FRONT:
		if (item->Pose.Orientation.x <= Angle::DegToRad(25.0f))
		{
			if (item->Pose.Orientation.x >= Angle::DegToRad(-25.0f))
			{
				if (item->Pose.Orientation.x > Angle::DegToRad(5.0f))
					item->Pose.Orientation.x += Angle::DegToRad(0.5f);
				else if (item->Pose.Orientation.x < Angle::DegToRad(-5.0f))
					item->Pose.Orientation.x -= Angle::DegToRad(0.5f);
				else if (item->Pose.Orientation.x > 0.0f)
					item->Pose.Orientation.x += Angle::DegToRad(0.25f);
				else if (item->Pose.Orientation.x < 0.0f)
					item->Pose.Orientation.x -= Angle::DegToRad(0.25);
				else
				{
					item->Animation.Velocity.y = 0;
					flag = 1;
				}
			}
			else
			{
				item->Pose.Orientation.x = item->Pose.Orientation.x - Angle::DegToRad(1.0f);
				flag = 1;
			}
		}
		else
		{
			item->Pose.Orientation.x = item->Pose.Orientation.x + Angle::DegToRad(1.0f);
			flag = 1;
		}

		if (c1.CollisionType == CT_LEFT)
			item->Pose.Orientation.y += Angle::DegToRad(2.0f);
		else if (c1.CollisionType == CT_RIGHT)
			item->Pose.Orientation.y -= Angle::DegToRad(2.0f);
		else if (c2.CollisionType == CT_LEFT)
			item->Pose.Orientation.y += Angle::DegToRad(2.0f);
		else if (c2.CollisionType == CT_RIGHT)
			item->Pose.Orientation.y -= Angle::DegToRad(2.0f);

		break;

	case CT_TOP:
		if (item->Pose.Orientation.x >= Angle::DegToRad(-45.0f))
		{
			item->Pose.Orientation.x -= Angle::DegToRad(1.0f);
			flag = 1;
		}

		break;

	case CT_TOP_FRONT:
		item->Animation.Velocity.y = 0;
		flag = 1;
		break;

	case CT_LEFT:
		item->Pose.Orientation.y += Angle::DegToRad(2.0f);
		flag = 1;
		break;

	case CT_RIGHT:
		item->Pose.Orientation.y -= Angle::DegToRad(2.0f);
		flag = 1;
		break;

	case CT_CLAMP:
		item->Pose.Position = coll->Setup.OldPosition;
		item->Animation.Velocity.y = 0;
		flag = 2;
		break;
	}

	if (coll->Middle.Floor < 0 && coll->Middle.Floor != NO_HEIGHT)
	{
		flag = 1;
		item->Pose.Orientation.x += Angle::DegToRad(1.0f);
		item->Pose.Position.y += coll->Middle.Floor;
	}

	if ((oldPose.Position == item->Pose.Position &&
		oldPose.Orientation.x == item->Pose.Orientation.x &&
		oldPose.Orientation.y == item->Pose.Orientation.y) ||
		flag != 1)
	{
		if (flag == 2)
			return;
	}

	if (lara->Control.WaterStatus != WaterStatus::FlyCheat && lara->ExtraAnim == NO_ITEM)
		TestLaraWaterDepth(item, coll);
}

void LaraWaterCurrent(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (lara->WaterCurrentActive)
	{
		auto* sink = &g_Level.Sinks[lara->WaterCurrentActive - 1];

		float angle = atan2(item->Pose.Position.x - sink->x, item->Pose.Position.z - sink->z);
		lara->WaterCurrentPull.x += (sink->strength * SECTOR(1) * sin(angle - Angle::DegToRad(90.0f)) - lara->WaterCurrentPull.x) / 16;
		lara->WaterCurrentPull.z += (sink->strength * SECTOR(1) * cos(angle - Angle::DegToRad(90.0f)) - lara->WaterCurrentPull.z) / 16;

		item->Pose.Position.y += (sink->y - item->Pose.Position.y) / 16;
	}
	else
	{
		int shift = 0;

		if (abs(lara->WaterCurrentPull.x) <= 16)
			shift = (abs(lara->WaterCurrentPull.x) > 8) + 2;
		else
			shift = 4;
		lara->WaterCurrentPull.x -= lara->WaterCurrentPull.x >> shift;

		if (abs(lara->WaterCurrentPull.x) < 4)
			lara->WaterCurrentPull.x = 0;

		if (abs(lara->WaterCurrentPull.z) <= 16)
			shift = (abs(lara->WaterCurrentPull.z) > 8) + 2;
		else
			shift = 4;
		lara->WaterCurrentPull.z -= lara->WaterCurrentPull.z >> shift;

		if (abs(lara->WaterCurrentPull.z) < 4)
			lara->WaterCurrentPull.z = 0;

		if (!lara->WaterCurrentPull.x && !lara->WaterCurrentPull.z)
			return;
	}

	item->Pose.Position.x += lara->WaterCurrentPull.x / CLICK(1);
	item->Pose.Position.z += lara->WaterCurrentPull.z / CLICK(1);
	lara->WaterCurrentActive = 0;

	coll->Setup.ForwardAngle = atan2(item->Pose.Position.z - coll->Setup.OldPosition.z, item->Pose.Position.x - coll->Setup.OldPosition.x);
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	GetCollisionInfo(coll, item, Vector3Int(0, 200, 0));

	if (coll->CollisionType == CT_FRONT)
	{
		if (item->Pose.Orientation.x > Angle::DegToRad(35.0f))
			item->Pose.Orientation.x = item->Pose.Orientation.x + Angle::DegToRad(1.0f);
		else if (item->Pose.Orientation.x < Angle::DegToRad(-35.0f))
			item->Pose.Orientation.x = item->Pose.Orientation.x - Angle::DegToRad(1.0f);
		else
			item->Animation.Velocity.y = 0;
	}
	else if (coll->CollisionType == CT_TOP)
		item->Pose.Orientation.x -= Angle::DegToRad(1.0f);
	else if (coll->CollisionType == CT_TOP_FRONT)
		item->Animation.Velocity.y = 0;
	else if (coll->CollisionType == CT_LEFT)
		item->Pose.Orientation.y += Angle::DegToRad(5.0f);
	else if (coll->CollisionType == CT_RIGHT)
		item->Pose.Orientation.y -= Angle::DegToRad(5.0f);

	if (coll->Middle.Floor < 0 && coll->Middle.Floor != NO_HEIGHT)
		item->Pose.Position.y += coll->Middle.Floor;

	ShiftItem(item, coll);
	coll->Setup.OldPosition = item->Pose.Position;
}

bool TestLaraHitCeiling(CollisionInfo* coll)
{
	if (coll->CollisionType == CT_TOP ||
		coll->CollisionType == CT_CLAMP)
	{
		return true;
	}

	return false;
}

void SetLaraHitCeiling(ItemInfo* item, CollisionInfo* coll)
{
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.z = 0;
	item->Animation.Velocity.y = 0;
	item->Pose.Position = coll->Setup.OldPosition;
}

bool TestLaraObjectCollision(ItemInfo* item, float angle, int distance, int height, int side)
{
	auto oldPose = item->Pose;
	int sideSign = copysign(1, side);

	item->Pose.Position.x += sin(item->Pose.Orientation.y + angle) * distance + cos(angle + Angle::DegToRad(90.0f) * sideSign) * abs(side);
	item->Pose.Position.y += height;
	item->Pose.Position.z += cos(item->Pose.Orientation.y + angle) * distance + sin(angle + Angle::DegToRad(90.0f) * sideSign) * abs(side);

	auto result = GetCollidedObjects(item, LARA_RADIUS, true, CollidedItems, CollidedMeshes, 0);

	item->Pose = oldPose;
	return result;
}
