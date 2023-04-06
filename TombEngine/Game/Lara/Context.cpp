#include "framework.h"
#include "Game/Lara/Context.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/los.h"
#include "Game/items.h"
#include "Game/Lara/ContextData.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_tests.h"
#include "Renderer/Renderer11.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"

using TEN::Renderer::g_Renderer;

namespace TEN::Entities::Player::Context
{
	static bool TestLedgeClimbSetup(ItemInfo& item, CollisionInfo& coll, const LedgeClimbSetupData& setupData)
	{
		constexpr auto ABS_FLOOR_BOUND = CLICK(0.8);

		// Get point collision.
		auto pointCollCenter = GetCollision(&item);
		auto pointCollFront = GetCollision(&item, setupData.HeadingAngle, OFFSET_RADIUS(coll.Setup.Radius), -(LARA_HEIGHT_STRETCH + CLICK(0.5f)));

		int vPosTop = item.Pose.Position.y - LARA_HEIGHT_STRETCH;
		int relFloorHeight = abs(pointCollFront.Position.Floor - vPosTop);
		int floorToCeilHeight = abs(pointCollFront.Position.Ceiling - pointCollFront.Position.Floor);
		int gapHeight = abs(pointCollCenter.Position.Ceiling - pointCollFront.Position.Floor);

		// 1) Test for slippery slope (if applicable).
		// TODO: This check fails for no reason.
		bool isSlipperySlope = setupData.TestSlipperySlope ? pointCollFront.Position.FloorSlope : false;
		if (isSlipperySlope)
			return false;

		// 2) Test for object blocking ledge.
		TestForObjectOnLedge(&item, &coll);
		if (coll.HitStatic)
			return false;

		// 3) Test for valid ledge.
		if (!TestValidLedge(&item, &coll))
			return false;

		// 4) Assess point collision.
		if (relFloorHeight <= ABS_FLOOR_BOUND &&				// Floor height is within lower/upper floor bounds.
			floorToCeilHeight > setupData.FloorToCeilingMin &&	// Floor-to-ceiling height isn't too narrow.
			floorToCeilHeight <= setupData.FloorToCeilingMax && // Floor-to-ceiling height isn't too wide.
			gapHeight >= setupData.GapHeightMin)				// Gap height is permissive.
		{
			return true;
		}

		return false;
	}

	bool CanSwingOnLedge(ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto UPPER_FLOOR_BOUND = 0;
		constexpr auto LOWER_CEIL_BOUND	 = CLICK(1.5f);

		auto& player = GetLaraInfo(item);

		// Get point collision.
		auto pointColl = GetCollision(&item, item.Pose.Orientation.y, BLOCK(0.25f));
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;
		int relCeilHeight = pointColl.Position.Ceiling - (item.Pose.Position.y - coll.Setup.Height);

		// 1) Test for wall.
		if (pointColl.Position.Floor == NO_HEIGHT)
			return false;

		// 2) Assess point collision.
		if (relFloorHeight >= UPPER_FLOOR_BOUND && // Floor height is below upper floor bound.
			relCeilHeight <= LOWER_CEIL_BOUND)	   // Ceiling height is above lower ceiling bound.
		{
			return true;
		}

		return false;
	}

	bool CanPerformLedgeJump(ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto LEDGE_HEIGHT_MIN = CLICK(2);

		// 1) Check if ledge jumps are enabled.
		if (!g_GameFlow->HasLedgeJumps())
			return false;

		// Ray collision setup at minimum ledge height.
		auto origin = GameVector(
			item.Pose.Position.x,
			(item.Pose.Position.y - LARA_HEIGHT_STRETCH) + LEDGE_HEIGHT_MIN,
			item.Pose.Position.z,
			item.RoomNumber);
		auto target = GameVector(
			Geometry::TranslatePoint(origin.ToVector3i(), item.Pose.Orientation.y, OFFSET_RADIUS(coll.Setup.Radius)),
			item.RoomNumber);

		// 2) Assess level geometry ray collision.
		if (LOS(&origin, &target))
			return false;

		// TODO: Assess static object geometry ray collision.

		// Get point collision.
		auto pointColl = GetCollision(&item);
		int relCeilHeight = pointColl.Position.Ceiling - (item.Pose.Position.y - LARA_HEIGHT_STRETCH);

		// 3) Assess point collision.
		if (relCeilHeight >= -coll.Setup.Height) // Ceiling height is below upper ceiling bound.
			return false;

		return true;
	}

	bool CanPerformLedgeHandstand(ItemInfo& item, CollisionInfo& coll)
	{
		auto setupData = LedgeClimbSetupData
		{
			item.Pose.Orientation.y,
			LARA_HEIGHT, -MAX_HEIGHT,
			CLICK(3),
			false
		};

		return TestLedgeClimbSetup(item, coll, setupData);
	}

	bool CanClimbLedgeToCrouch(ItemInfo& item, CollisionInfo& coll)
	{
		auto setupData = LedgeClimbSetupData
		{
			item.Pose.Orientation.y,
			LARA_HEIGHT_CRAWL, LARA_HEIGHT,
			(int)CLICK(0.6f),
			true
		};

		return TestLedgeClimbSetup(item, coll, setupData);
	}

	bool CanClimbLedgeToStand(ItemInfo& item, CollisionInfo& coll)
	{
		auto setupData = LedgeClimbSetupData
		{
			item.Pose.Orientation.y,
			LARA_HEIGHT, -MAX_HEIGHT,
			CLICK(1),
			false
		};

		return TestLedgeClimbSetup(item, coll, setupData);
	}

	bool CanLedgeShimmyLeft(ItemInfo& item, CollisionInfo& coll)
	{
		return TestLaraHangSideways(&item, &coll, ANGLE(-90.0f));
	}

	bool CanLedgeShimmyRight(ItemInfo& item, CollisionInfo& coll)
	{
		return TestLaraHangSideways(&item, &coll, ANGLE(90.0f));
	}

	bool CanWallShimmyUp(ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto WALL_STEP_HEIGHT = -CLICK(1);

		auto& player = GetLaraInfo(item);

		// 1) Test if wall is climbable.
		if (!player.Control.CanClimbLadder)
			return false;

		// Get point collision.
		auto pointCollCenter = GetCollision(&item);
		auto pointCollLeft = GetCollision(&item, item.Pose.Orientation.y - ANGLE(90.0f), OFFSET_RADIUS(coll.Setup.Radius));
		auto pointCollRight = GetCollision(&item, item.Pose.Orientation.y + ANGLE(90.0f), OFFSET_RADIUS(coll.Setup.Radius));

		int vPosTop = item.Pose.Position.y - LARA_HEIGHT_STRETCH;
		int relCeilHeightCenter = pointCollCenter.Position.Ceiling - vPosTop;
		int relCeilHeightLeft = pointCollCenter.Position.Ceiling - vPosTop;
		int relCeilHeightRight = pointCollCenter.Position.Ceiling - vPosTop;

		// 2) Assess point collision.
		if (relCeilHeightCenter <= WALL_STEP_HEIGHT &&
			relCeilHeightLeft <= WALL_STEP_HEIGHT &&
			relCeilHeightRight <= WALL_STEP_HEIGHT)
		{
			return true;
		}

		return false;
	}

	// TODO!!
	bool CanWallShimmyDown(ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto WALL_STEP_HEIGHT = CLICK(1);

		auto& player = GetLaraInfo(item);

		auto pointCollCenter = GetCollision(&item);

		int relFloorHeight = pointCollCenter.Position.Floor - item.Pose.Position.y;
		// Left and right.

		// 1) Check if wall is climbable.
		if (!player.Control.CanClimbLadder)
			return false;

		// 2) Assess point collision.
		if (relFloorHeight >= WALL_STEP_HEIGHT)
			return true;

		return false;
	}

	std::optional<MonkeySwingCatchData> GetMonkeySwingCatchData(ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto ABS_CEIL_BOUND			= CLICK(0.5f);
		constexpr auto FLOOR_TO_CEIL_HEIGHT_MAX = LARA_HEIGHT_MONKEY;

		const auto& player = GetLaraInfo(item);

		// 1) Check for monkey swing ceiling.
		if (!player.Control.CanMonkeySwing)
			return std::nullopt;

		// Get point collision.
		auto pointColl = GetCollision(&item);

		// Calculate key heights.
		int vPos = item.Pose.Position.y - coll.Setup.Height;
		int relCeilHeight = pointColl.Position.Ceiling - vPos;
		int floorToCeilHeight = abs(pointColl.Position.Ceiling - pointColl.Position.Floor);

		// 2) Check collision type.
		if (coll.CollisionType != CollisionType::CT_TOP &&
			coll.CollisionType != CollisionType::CT_TOP_FRONT)
		{
			return std::nullopt;
		}
		
		// 3) Assess point collision.
		if (abs(relCeilHeight) <= ABS_CEIL_BOUND &&		  // Ceiling height is within lower/upper ceiling bounds.
			floorToCeilHeight > FLOOR_TO_CEIL_HEIGHT_MAX) // Floor-to-ceiling height isn't too narrow.
		{
			int animNumber = (item.Animation.ActiveState == LS_JUMP_UP) ? LA_JUMP_UP_TO_MONKEY : LA_REACH_TO_MONKEY;
			return MonkeySwingCatchData{ animNumber, pointColl.Position.Ceiling };
		}

		return std::nullopt;
	}

	static bool TestLedgeCatch(int verticalPos, float verticalVel, int edgeHeight)
	{
		bool isMovingUp = (verticalVel < 0.0f);

		int relEdgeHeight = edgeHeight - verticalPos;
		int lowerBound = isMovingUp ? 0 : verticalVel;
		int upperBound = isMovingUp ? verticalVel : 0;

		// Assess point collision to ledge moving up.
		if (relEdgeHeight <= lowerBound && // Edge height is above lower height bound.
			relEdgeHeight >= upperBound)   // Edge height is below upper height bound.
		{
			return true;
		}

		return false;
	}

	static bool TestClimbableWallCatch(int verticalPos, float verticalVel, int edgeHeight, int floorHeight, int ceilHeight)
	{
		// 1) Check movement direction.
		bool isMovingUp = (verticalVel < 0.0f);
		if (isMovingUp)
			return false;

		// 2) Test for wall.
		if (edgeHeight < floorHeight && // Edge is above floor.
			edgeHeight > ceilHeight)	// Edge is below ceiling.
		{
			return false;
		}

		// 3) Assess point collision to climbable wall edge.
		int relEdgeHeight = edgeHeight - verticalPos;
		if (relEdgeHeight <= verticalVel && // Edge height is above lower height bound.
			relEdgeHeight >= 0)				// Edge height is below upper height bound.
		{
			return true;
		}

		return false;
	}

	std::optional<EdgeCatchData> GetEdgeCatchData(ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto WALL_STEP_HEIGHT = CLICK(1);

		const auto& player = GetLaraInfo(item);

		// 1) Test for valid ledge.
		if (!TestValidLedge(&item, &coll, true))
			return std::nullopt;

		// Get point collision.
		float probeHeight = -(coll.Setup.Height + abs(item.Animation.Velocity.y));
		auto pointCollCenter = GetCollision(&item);
		auto pointCollFront = GetCollision(&item, item.Pose.Orientation.y, OFFSET_RADIUS(coll.Setup.Radius), probeHeight);

		int vPos = item.Pose.Position.y - coll.Setup.Height;
		int relFloorHeightCenter = pointCollCenter.Position.Floor - vPos;
		int relFloorHeightFront = pointCollFront.Position.Floor - vPos;

		// 2) Test if ledge height is too low to the ground.
		int relLedgeHeight = abs(pointCollFront.Position.Floor - pointCollCenter.Position.Floor);
		if (relLedgeHeight <= LARA_HEIGHT_STRETCH)
			return std::nullopt;

		// 3) Test ledge catch.
		int ledgeHeight = pointCollFront.Position.Floor;
		if (TestLedgeCatch(vPos, item.Animation.Velocity.y, ledgeHeight))
			return EdgeCatchData{ EdgeType::Ledge, ledgeHeight };

		// TODO: Can't catch walls formed by ceilings?
		// 4) Test for climbable wall edge.
		if (player.Control.CanClimbLadder)
		{
			// 4.1) Ensure climbable wall is valid.
			bool isClimbableWall = TestLaraHangOnClimbableWall(&item, &coll);
			if (!isClimbableWall && !TestValidLedge(&item, &coll, true, true))
				return std::nullopt;

			int wallEdgeHeight = (int)floor((vPos + item.Animation.Velocity.y) / WALL_STEP_HEIGHT) * WALL_STEP_HEIGHT;

			// 4.2) Test if wall eedge height is too low to the ground.
			int relWallEdgeHeight = abs(wallEdgeHeight - pointCollCenter.Position.Floor);
			if (relWallEdgeHeight <= LARA_HEIGHT_STRETCH)
				return std::nullopt;

			// 4.3) Test wall edge catch.
			if (TestClimbableWallCatch(
				vPos, item.Animation.Velocity.y, wallEdgeHeight,
				pointCollFront.Position.Floor, pointCollFront.Position.Ceiling))
			{
				return EdgeCatchData{ EdgeType::ClimbableWall, wallEdgeHeight };
			}
		}

		return std::nullopt;
	}
}
