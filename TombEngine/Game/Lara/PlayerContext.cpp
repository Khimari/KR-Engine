#include "framework.h"
#include "Game/Lara/PlayerContext.h"

#include "Game/animation.h"
#include "Game/collision/AttractorCollision.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/control/los.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_tests.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision::Attractor;
using namespace TEN::Collision::Floordata;
using namespace TEN::Input;

namespace TEN::Entities::Player
{
	PlayerAttractorData::~PlayerAttractorData()
	{
		// TODO: Polymorphism to avoid global.
		Detach(*LaraItem);
	}

	void PlayerAttractorData::Attach(ItemInfo& playerItem, Attractor& attrac, float chainDist,
									 const Vector3& relPosOffset, const EulerAngles& relOrientOffset,
									 const Vector3& relDeltaPos, const EulerAngles& relDeltaOrient)
	{
		Detach(playerItem);

		Ptr = &attrac;
		ChainDistance = chainDist;
		RelPosOffset = relPosOffset;
		RelOrientOffset = relOrientOffset;
		RelDeltaPos = relDeltaPos;
		RelDeltaOrient = relDeltaOrient;

		Ptr->AttachPlayer(playerItem);
	}

	void PlayerAttractorData::Detach(ItemInfo& playerItem)
	{
		if (Ptr == nullptr)
			return;

		Ptr->DetachPlayer(playerItem);

		Ptr = nullptr;
		ChainDistance = 0.0f;
		RelPosOffset = Vector3::Zero;
		RelOrientOffset = EulerAngles::Zero;
		RelDeltaPos = Vector3::Zero;
		RelDeltaOrient = EulerAngles::Zero;
	};

	void PlayerAttractorData::Update(ItemInfo& playerItem, Attractor& attrac, float chainDist,
									 const Vector3& relPosOffset, const EulerAngles& relOrientOffset)
	{
		Attach(
			playerItem, attrac, chainDist,
			relPosOffset, relOrientOffset,
			RelDeltaPos, RelDeltaOrient);
	}

	PlayerContext::PlayerContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		/*ItemPtr = &item;
		PlayerPtr = &player;
		CollPtr = &coll;*/
	}

	bool CanChangeElevation(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto LOWER_FLOOR_BOUND = STEPUP_HEIGHT;
		constexpr auto UPPER_FLOOR_BOUND = -STEPUP_HEIGHT;

		const auto& player = GetLaraInfo(item);

		// Get point collision.
		auto pointColl = GetCollision(&item, 0, 0, -coll.Setup.Height / 2); // NOTE: Height offset required for correct bridge collision.
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;

		// 1) Test if player is already aligned with floor.
		if (relFloorHeight == 0)
			return false;

		// 2) Assess point collision and player status.
		if ((relFloorHeight <= LOWER_FLOOR_BOUND ||					// Floor height is above lower floor bound.
				player.Control.WaterStatus == WaterStatus::Wade) && // OR player is wading.
			relFloorHeight >= UPPER_FLOOR_BOUND)					// Floor height is below upper floor bound.
		{
			return true;
		}

		return false;
	}

	static bool CanPerformStep(const ItemInfo& item, const CollisionInfo& coll, bool isGoingUp)
	{
		constexpr auto LOWER_FLOOR_BOUND_UP	  = -CLICK(0.75f);
		constexpr auto UPPER_FLOOR_BOUND_UP	  = -STEPUP_HEIGHT;
		constexpr auto LOWER_FLOOR_BOUND_DOWN = STEPUP_HEIGHT;
		constexpr auto UPPER_FLOOR_BOUND_DOWN = CLICK(0.75f);

		// Get point collision.
		auto pointColl = GetCollision(&item, 0, 0, -coll.Setup.Height / 2); // NOTE: Height offset required for correct bridge collision.
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;

		// Determine appropriate floor bounds.
		int lowerFloorBound = isGoingUp ? LOWER_FLOOR_BOUND_UP : LOWER_FLOOR_BOUND_DOWN;
		int upperFloorBound = isGoingUp ? UPPER_FLOOR_BOUND_UP : UPPER_FLOOR_BOUND_DOWN;

		// Assess point collision.
		if (relFloorHeight <= lowerFloorBound && // Floor height is above lower floor bound.
			relFloorHeight >= upperFloorBound)	 // Floor height is below upper floor bound.
		{
			return true;
		}

		return false;
	}

	bool CanStepUp(const ItemInfo& item, const CollisionInfo& coll)
	{
		return CanPerformStep(item, coll, true);
	}

	bool CanStepDown(const ItemInfo& item, const CollisionInfo& coll)
	{
		return CanPerformStep(item, coll, false);
	}

	bool CanStrikeAfkPose(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// 1) Check if AFK posing is enabled.
		if (!g_GameFlow->HasAFKPose())
			return false;

		// 2) Test AFK pose timer.
		if (player.Control.Count.Pose < PLAYER_POSE_TIME)
			return false;

		// 3) Test player hand and water status.
		if (player.Control.HandStatus != HandStatus::Free ||
			player.Control.WaterStatus == WaterStatus::Wade)
		{
			return false;
		}

		// 4) Assess player status.
		if (!(IsHeld(In::Flare) || IsHeld(In::Draw)) &&				   // Avoid unsightly concurrent actions.
			(player.Control.Weapon.GunType != LaraWeaponType::Flare || // Not handling flare.
				player.Flare.Life) &&								   // OR flare is still active.
			player.Context.Vehicle == NO_ITEM)						   // Not in a vehicle.
		{
			return true;
		}

		return false;
	}

	bool CanTurn180(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// Assess player status.
		if (player.Control.WaterStatus == WaterStatus::Wade || // Is wading.
			TestEnvironment(ENV_FLAG_SWAMP, &item))			   // OR is in swamp.
		{
			return true;
		}

		return false;
	}

	bool CanTurnFast(const ItemInfo& item, const CollisionInfo& coll, bool isGoingRight)
	{
		const auto& player = GetLaraInfo(item);

		// 1) Test player turn rate.
		if (isGoingRight)
		{
			if (player.Control.TurnRate/*.y*/ >= LARA_SLOW_TURN_RATE_MAX)
				return true;
		}
		else
		{
			if (player.Control.TurnRate/*.y*/ <= -LARA_SLOW_TURN_RATE_MAX)
				return true;
		}

		// 2) Assess player status.
		if (player.Control.WaterStatus == WaterStatus::Dry &&
			((player.Control.HandStatus == HandStatus::WeaponReady && player.Control.Weapon.GunType != LaraWeaponType::Torch) ||
				(player.Control.HandStatus == HandStatus::WeaponDraw && player.Control.Weapon.GunType != LaraWeaponType::Flare)))
		{
			return true;
		}

		return false;
	}

	static bool TestGroundMovementSetup(const ItemInfo& item, const CollisionInfo& coll, const GroundMovementSetupData& setup, bool isCrawling = false)
	{
		constexpr auto SLOPE_ASPECT_ANGLE_DELTA_MAX = ANGLE(90.0f);

		// HACK: coll.Setup.Radius and coll.Setup.Height are set only in lara_col functions and then reset by LaraAboveWater() to defaults.
		// This means they will store the wrong values for any context assessment functions called in crouch/crawl lara_as routines.
		// If states become objects, a dedicated state init function should eliminate the need for the isCrawling parameter. -- Sezz 2022.03.16
		int playerRadius = isCrawling ? LARA_RADIUS_CRAWL : coll.Setup.Radius;
		int playerHeight = isCrawling ? LARA_HEIGHT_CRAWL : coll.Setup.Height;

		// Get point collision.
		auto pointColl = GetCollision(&item, setup.HeadingAngle, OFFSET_RADIUS(playerRadius), -playerHeight);
		int vPos = item.Pose.Position.y;
		int vPosTop = vPos - playerHeight;

		// Calculate slope aspect delta angle.
		short aspectAngle = Geometry::GetSurfaceAspectAngle(pointColl.FloorNormal);
		short aspectAngleDelta = Geometry::GetShortestAngle(setup.HeadingAngle, aspectAngle);

		// 1) Check for slippery slope below floor (if applicable).
		if (setup.TestSlipperySlopeBelow &&
			(pointColl.Position.FloorSlope && abs(aspectAngleDelta) <= SLOPE_ASPECT_ANGLE_DELTA_MAX))
		{
			return false;
		}
		
		// 1) Check for slippery slope above floor (if applicable).
		if (setup.TestSlipperySlopeAbove &&
			(pointColl.Position.FloorSlope && abs(aspectAngleDelta) >= SLOPE_ASPECT_ANGLE_DELTA_MAX))
		{
			return false;
		}

		// 3) Check for death floor (if applicable).
		if (setup.TestDeathFloor && pointColl.Block->Flags.Death)
			return false;

		// LOS setup at upper floor bound.
		auto origin0 = GameVector(
			item.Pose.Position.x,
			(vPos + setup.UpperFloorBound) - 1,
			item.Pose.Position.z,
			item.RoomNumber);
		auto target0 = GameVector(
			pointColl.Coordinates.x,
			(vPos + setup.UpperFloorBound) - 1,
			pointColl.Coordinates.z,
			item.RoomNumber);

		// LOS setup at lowest ceiling bound (player height).
		auto origin1 = GameVector(
			item.Pose.Position.x,
			vPosTop + 1,
			item.Pose.Position.z,
			item.RoomNumber);
		auto target1 = GameVector(
			pointColl.Coordinates.x,
			vPosTop + 1,
			pointColl.Coordinates.z,
			item.RoomNumber);

		// Calculate LOS direction.
		auto origin = target0.ToVector3();
		auto target = target1.ToVector3();
		auto dir = target - origin;
		dir.Normalize();

		// 4) Assess static LOS.
		auto staticLos = GetStaticObjectLos(origin, item.RoomNumber, dir, Vector3::Distance(origin, target), false);
		if (staticLos.has_value())
			return false;

		// 5) Assess room LOS.
		if (!LOS(&origin0, &target0) || !LOS(&origin1, &target1))
			return false;

		int relFloorHeight = pointColl.Position.Floor - vPos;
		int relCeilHeight = pointColl.Position.Ceiling - vPos;
		int floorToCeilHeight = abs(pointColl.Position.Ceiling - pointColl.Position.Floor);

		// 6) Assess point collision.
		if (relFloorHeight <= setup.LowerFloorBound && // Floor height is above lower floor bound.
			relFloorHeight >= setup.UpperFloorBound && // Floor height is below upper floor bound.
			relCeilHeight < -playerHeight &&		   // Ceiling height is above player height.
			floorToCeilHeight > playerHeight)		   // Floor-to-ceiling height isn't too narrow.
		{
			return true;
		}

		return false;
	}

	bool CanRoll180Running(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);

		if (player.Control.WaterStatus != WaterStatus::Wade &&
			!player.Control.IsRunJumpQueued) // NOTE: Queued running jump blocks 180 roll by design.
		{
			return true;
		}

		return false;
	}

	bool CanRunForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = GroundMovementSetupData
		{
			item.Pose.Orientation.y,
			-MAX_HEIGHT, -STEPUP_HEIGHT, // NOTE: Bounds defined by run forward state.
			false, true, false
		};

		return TestGroundMovementSetup(item, coll, setup);
	}

	bool CanRunBackward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = GroundMovementSetupData
		{
			short(item.Pose.Orientation.y + ANGLE(180.0f)),
			-MAX_HEIGHT, -STEPUP_HEIGHT, // NOTE: Bounds defined by run backward state.
			false, false, false
		};

		return TestGroundMovementSetup(item, coll, setup);
	}

	bool CanWalkForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = GroundMovementSetupData
		{
			item.Pose.Orientation.y,
			STEPUP_HEIGHT, -STEPUP_HEIGHT, // NOTE: Bounds defined by walk forward state.
		};

		return TestGroundMovementSetup(item, coll, setup);
	}

	bool CanWalkBackward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = GroundMovementSetupData
		{
			short(item.Pose.Orientation.y + ANGLE(180.0f)),
			STEPUP_HEIGHT, -STEPUP_HEIGHT // NOTE: Bounds defined by walk backward state.
		};

		return TestGroundMovementSetup(item, coll, setup);
	}

	static bool TestSidestep(const ItemInfo& item, const CollisionInfo& coll, bool isGoingRight)
	{
		const auto& player = GetLaraInfo(item);

		auto setup = GroundMovementSetupData{};

		// Wade case.
		if (player.Control.WaterStatus == WaterStatus::Wade)
		{
			setup = GroundMovementSetupData
			{
				short(item.Pose.Orientation.y + (isGoingRight ? ANGLE(90.0f) : ANGLE(-90.0f))),
				-MAX_HEIGHT, -(int)CLICK(1.25f), // NOTE: Upper bound defined by sidestep left/right states.
				false, false, false
			};
		}
		// Regular case.
		else
		{
			setup = GroundMovementSetupData
			{
				short(item.Pose.Orientation.y + (isGoingRight ? ANGLE(90.0f) : ANGLE(-90.0f))),
				(int)CLICK(0.8f), -(int)CLICK(0.8f) // NOTE: Bounds defined by sidestep left/right states.
			};
		}

		return TestGroundMovementSetup(item, coll, setup);
	}

	bool CanSidestepLeft(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestSidestep(item, coll, false);
	}

	bool CanSidestepRight(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestSidestep(item, coll, true);
	}

	bool CanWadeForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// 1) Test player water status.
		if (player.Control.WaterStatus != WaterStatus::Wade)
			return false;

		auto setup = GroundMovementSetupData
		{
			item.Pose.Orientation.y,
			-MAX_HEIGHT, -STEPUP_HEIGHT, // NOTE: Bounds defined by wade forward state.
			false, false, false
		};

		// 2) Assess context.
		return TestGroundMovementSetup(item, coll, setup);
	}

	bool CanWadeBackward(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// 1) Test player water status.
		if (player.Control.WaterStatus != WaterStatus::Wade)
			return false;

		auto setup = GroundMovementSetupData
		{
			short(item.Pose.Orientation.y + ANGLE(180.0f)),
			-MAX_HEIGHT, -STEPUP_HEIGHT, // NOTE: Bounds defined by walk backward state.
			false, false, false
		};

		// 2) Assess context.
		return TestGroundMovementSetup(item, coll, setup);
	}

	bool CanVaultFromSprint(const ItemInfo& item, const CollisionInfo& coll)
	{
		return !TestLaraWall(&item, OFFSET_RADIUS(coll.Setup.Radius), -BLOCK(5 / 8.0f));
	}

	bool CanSlide(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ABS_FLOOR_BOUND = STEPUP_HEIGHT;

		const auto& player = GetLaraInfo(item);

		// 1) Test player water status.
		if (player.Control.WaterStatus == WaterStatus::Wade)
			return false;

		// Get point collision.
		auto pointColl = GetCollision(&item, 0, 0, -coll.Setup.Height / 2); // NOTE: Offset required for correct bridge collision.
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;

		// 2) Assess point collision.
		if (abs(relFloorHeight) <= ABS_FLOOR_BOUND && // Floor height is within upper/lower floor bounds.
			pointColl.Position.FloorSlope)			  // Floor is a slippery slope.
		{
			return true;
		}

		return false;
	}

	bool CanSteerOnSlide(const ItemInfo& item, const CollisionInfo& coll)
	{
		return g_GameFlow->HasSlideExtended();
	}

	bool IsInLowSpace(const ItemInfo& item, const CollisionInfo& coll)
	{
		static const auto CROUCH_STATES = std::vector<int>
		{
			LS_CROUCH_IDLE,
			LS_CROUCH_TURN_LEFT,
			LS_CROUCH_TURN_RIGHT
		};

		// HACK: coll.Setup.Radius is only set to LARA_RADIUS_CRAWL in lara_col functions, then reset by LaraAboveWater(),
		// meaning that for tests called in lara_as functions it will store the wrong radius. -- Sezz 2021.11.05
		float radius = TestState(item.Animation.ActiveState, CROUCH_STATES) ? LARA_RADIUS_CRAWL : LARA_RADIUS;

		// Get center point collision.
		auto pointCollCenter = GetCollision(&item, 0, 0.0f, -LARA_HEIGHT / 2);
		int floorToCeilHeightCenter = abs(pointCollCenter.Position.Ceiling - pointCollCenter.Position.Floor);

		// Assess center point collision.
		if (floorToCeilHeightCenter < LARA_HEIGHT ||					// Floor-to-ceiling height isn't too wide.
			abs(coll.Middle.Ceiling - LARA_HEIGHT_CRAWL) < LARA_HEIGHT) // Consider statics overhead detected by GetCollisionInfo().
		{
			return true;
		}

		// TODO: Check whether < or <= and > or >=.

		// Get front point collision.
		auto pointCollFront = GetCollision(&item, item.Pose.Orientation.y, radius, -coll.Setup.Height);
		int floorToCeilHeightFront = abs(pointCollFront.Position.Ceiling - pointCollFront.Position.Floor);
		int relFloorHeightFront = abs(pointCollFront.Position.Floor - pointCollCenter.Position.Floor);

		// Assess front point collision.
		if (relFloorHeightFront <= CRAWL_STEPUP_HEIGHT && // Floor is within upper/lower floor bounds.
			floorToCeilHeightFront < LARA_HEIGHT &&		  // Floor-to-ceiling height isn't too wide.
			floorToCeilHeightFront > LARA_HEIGHT_CRAWL)	  // Floor-to-ceiling height isn't too narrow.
		{
			return true;
		}

		// Get back point collision.
		auto pointCollBack = GetCollision(&item, item.Pose.Orientation.y, -radius, -coll.Setup.Height);
		int floorToCeilHeightBack = abs(pointCollBack.Position.Ceiling - pointCollBack.Position.Floor);
		int relFloorHeightBack = abs(pointCollBack.Position.Floor - pointCollCenter.Position.Floor);

		// Assess back point collision.
		if (relFloorHeightBack <= CRAWL_STEPUP_HEIGHT && // Floor is within upper/lower floor bounds.
			floorToCeilHeightBack < LARA_HEIGHT &&		 // Floor-to-ceiling height isn't too wide.
			floorToCeilHeightBack > LARA_HEIGHT_CRAWL)	 // Floor-to-ceiling height isn't too narrow.
		{
			return true;
		}

		return false;
	}

	bool CanCrouch(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// Assess player status.
		if (player.Control.WaterStatus != WaterStatus::Wade && // Player is wading.
			!((player.Control.HandStatus == HandStatus::WeaponReady ||
				player.Control.HandStatus == HandStatus::WeaponDraw ||
				player.Control.HandStatus == HandStatus::WeaponUndraw) &&
				IsStandingWeapon(&item, player.Control.Weapon.GunType))) // OR player is wielding a non-standing weapon.
		{
			return true;
		}

		return false;
	}

	bool CanCrouchToCrawl(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// Assess player status.
		if (!(IsHeld(In::Flare) || IsHeld(In::Draw)) &&				   // Avoid unsightly concurrent actions.
			player.Control.HandStatus == HandStatus::Free &&		   // Hands are free.
			(player.Control.Weapon.GunType != LaraWeaponType::Flare || // Not handling flare. TODO: Should be allowed, but flare animation bugs out. -- Sezz 2022.03.18
				player.Flare.Life))
		{
			return true;
		}

		return false;
	}

	bool CanCrouchRoll(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto FLOOR_BOUND				= CRAWL_STEPUP_HEIGHT;
		constexpr auto FLOOR_TO_CEIL_HEIGHT_MAX = LARA_HEIGHT_CRAWL;
		constexpr auto WATER_HEIGHT_MAX			= -CLICK(1);
		constexpr auto PROBE_DIST_MAX			= BLOCK(1);
		constexpr auto STEP_DIST				= BLOCK(0.25f);

		const auto& player = GetLaraInfo(item);

		// 1) Check if crouch roll is enabled.
		if (!g_GameFlow->HasCrouchRoll())
			return false;

		// 2) Test water depth.
		if (player.Context.WaterSurfaceDist < WATER_HEIGHT_MAX)
			return false;

		// TODO: Extend point collision struct to also find water depths.
		float dist = 0.0f;
		auto pointColl0 = GetCollision(&item);

		// 3) Test continuity of path.
		while (dist < PROBE_DIST_MAX)
		{
			// Get point collision.
			dist += STEP_DIST;
			auto pointColl1 = GetCollision(&item, item.Pose.Orientation.y, dist, -LARA_HEIGHT_CRAWL);

			int floorHeightDelta = abs(pointColl0.Position.Floor - pointColl1.Position.Floor);
			int floorToCeilHeight = abs(pointColl1.Position.Ceiling - pointColl1.Position.Floor);

			// Assess point collision.
			if (floorHeightDelta > FLOOR_BOUND ||				 // Avoid floor height delta beyond crawl stepup threshold.
				floorToCeilHeight <= FLOOR_TO_CEIL_HEIGHT_MAX || // Avoid narrow spaces.
				pointColl1.Position.FloorSlope)					 // Avoid slippery floor slopes.
			{
				return false;
			}

			pointColl0 = std::move(pointColl1);
		}

		return true;
	}

	bool CanCrawlForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = GroundMovementSetupData
		{
			item.Pose.Orientation.y,
			CRAWL_STEPUP_HEIGHT, -CRAWL_STEPUP_HEIGHT // NOTE: Bounds defined by crawl forward state.
		};

		return TestGroundMovementSetup(item, coll, setup, true);
	}

	bool CanCrawlBackward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = GroundMovementSetupData
		{
			short(item.Pose.Orientation.y + ANGLE(180.0f)),
			CRAWL_STEPUP_HEIGHT, -CRAWL_STEPUP_HEIGHT // NOTE: Bounds defined by crawl backward state.
		};

		return TestGroundMovementSetup(item, coll, setup, true);
	}

	bool CanPerformMonkeySwingStep(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto LOWER_CEIL_BOUND = MONKEY_STEPUP_HEIGHT;
		constexpr auto UPPER_CEIL_BOUND = -MONKEY_STEPUP_HEIGHT;

		// Get point collision.
		auto pointColl = GetCollision(&item);
		int relCeilHeight = pointColl.Position.Ceiling - (item.Pose.Position.y - LARA_HEIGHT_MONKEY);

		// Assess point collision.
		if (relCeilHeight <= LOWER_CEIL_BOUND && // Ceiling height is above lower ceiling bound.
			relCeilHeight >= UPPER_CEIL_BOUND)	 // Ceiling height is below upper ceiling bound.
		{
			return true;
		}

		return false;
	}

	bool CanFallFromMonkeySwing(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ABS_CEIL_BOUND = CLICK(1.25f);

		auto& player = GetLaraInfo(item);

		// 1) Check for monkey swing ceiling.
		if (!player.Control.CanMonkeySwing)
			return true;

		// Get point collision.
		auto pointColl = GetCollision(&item);

		// 2) Test for slippery ceiling slope and check if overhang climb is disabled.
		if (pointColl.Position.CeilingSlope && !g_GameFlow->HasOverhangClimb())
			return true;

		// 3) Assess point collision.
		int relCeilHeight = pointColl.Position.Ceiling - (item.Pose.Position.y - LARA_HEIGHT_MONKEY);
		if (abs(relCeilHeight) > ABS_CEIL_BOUND) // Ceiling height is within lower/upper ceiling bound.
			return true;

		return false;
	}

	bool CanGrabMonkeySwing(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto LOWER_CEIL_BOUND			= CLICK(0.5f);
		constexpr auto FLOOR_TO_CEIL_HEIGHT_MAX = LARA_HEIGHT_MONKEY;

		const auto& player = GetLaraInfo(item);

		// 1) Check for monkey swing ceiling.
		if (!player.Control.CanMonkeySwing)
			return false;

		// Get point collision.
		auto pointColl = GetCollision(&item);
		int relCeilHeight = pointColl.Position.Ceiling - (item.Pose.Position.y - LARA_HEIGHT_MONKEY);
		int floorToCeilHeight = abs(pointColl.Position.Ceiling - pointColl.Position.Floor);

		// 2) Assess collision with ceiling.
		if (relCeilHeight < 0 &&
			coll.CollisionType != CollisionType::CT_TOP &&
			coll.CollisionType != CollisionType::CT_TOP_FRONT)
		{
			return false;
		}

		// 3) Assess point collision.
		if (relCeilHeight <= LOWER_CEIL_BOUND &&		  // Ceiling height is above lower ceiling bound.
			floorToCeilHeight > FLOOR_TO_CEIL_HEIGHT_MAX) // Floor-to-ceiling space isn't too narrow.
		{
			return true;
		}

		return false;
	}

	static bool TestMonkeySwingSetup(const ItemInfo& item, const CollisionInfo& coll, const MonkeySwingMovementSetupData& setup)
	{
		// HACK: Have to make the height explicit for now. -- Sezz 2022.07.28
		constexpr auto PLAYER_HEIGHT = LARA_HEIGHT_MONKEY;

		// Get point collision.
		auto pointColl = GetCollision(&item, setup.HeadingAngle, OFFSET_RADIUS(coll.Setup.Radius));

		// 1) Test if ceiling is monkey swing.
		if (!pointColl.BottomBlock->Flags.Monkeyswing)
			return false;

		// 2) Test for ceiling slippery slope.
		if (pointColl.Position.CeilingSlope)
			return false;

		int vPos = item.Pose.Position.y;
		int vPosTop = vPos - coll.Setup.Height;

		// Ray collision setup at highest floor bound (player base).
		auto origin0 = GameVector(
			item.Pose.Position.x,
			vPos - 1,
			item.Pose.Position.z,
			item.RoomNumber);
		auto target0 = GameVector(
			pointColl.Coordinates.x,
			vPos - 1,
			pointColl.Coordinates.z,
			item.RoomNumber);

		// Raycast setup at lower ceiling bound.
		auto origin1 = GameVector(
			item.Pose.Position.x,
			(vPosTop + setup.LowerCeilingBound) + 1,
			item.Pose.Position.z,
			item.RoomNumber);
		auto target1 = GameVector(
			pointColl.Coordinates.x,
			(vPosTop + setup.LowerCeilingBound) + 1,
			pointColl.Coordinates.z,
			item.RoomNumber);

		// Prepare data for static object LOS.
		auto origin = target0.ToVector3();
		auto target = target1.ToVector3();
		auto dir = target - origin;
		dir.Normalize();

		// 3) Assess ray-static collision.
		auto staticLos = GetStaticObjectLos(origin, item.RoomNumber, dir, Vector3::Distance(origin, target), false);
		if (staticLos.has_value())
			return false;

		// 3) Assess level geometry ray collision.
		if (!LOS(&origin0, &target0) || !LOS(&origin1, &target1))
			return false;

		// TODO: Assess static object geometry ray collision.

		int relFloorHeight = pointColl.Position.Floor - vPos;
		int relCeilHeight = pointColl.Position.Ceiling - vPosTop;
		int floorToCeilHeight = abs(pointColl.Position.Ceiling - pointColl.Position.Floor);

		// 4) Assess point collision.
		if (relFloorHeight > 0 &&						// Floor is within highest floor bound (player base).
			relCeilHeight <= setup.LowerCeilingBound && // Ceiling is within lower ceiling bound.
			relCeilHeight >= setup.UpperCeilingBound && // Ceiling is within upper ceiling bound.
			floorToCeilHeight > PLAYER_HEIGHT)			// Space is not too narrow.
		{
			return true;
		}

		return false;
	}

	bool CanMonkeySwingForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = MonkeySwingMovementSetupData
		{
			item.Pose.Orientation.y,
			MONKEY_STEPUP_HEIGHT, -MONKEY_STEPUP_HEIGHT // NOTE: Bounds defined by monkey forward state.
		};

		return TestMonkeySwingSetup(item, coll, setup);
	}

	bool CanMonkeySwingBackward(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto setup = MonkeySwingMovementSetupData
		{
			short(item.Pose.Orientation.y + ANGLE(180.0f)),
			MONKEY_STEPUP_HEIGHT, -MONKEY_STEPUP_HEIGHT // NOTE: Bounds defined by monkey backward state.
		};

		return TestMonkeySwingSetup(item, coll, setup);
	}

	static bool TestMonkeyShimmy(const ItemInfo& item, const CollisionInfo& coll, bool isGoingRight)
	{
		auto setup = MonkeySwingMovementSetupData
		{
			short(item.Pose.Orientation.y + (isGoingRight ? ANGLE(90.0f) : ANGLE(-90.0f))),
			(int)CLICK(0.5f), -(int)CLICK(0.5f) // NOTE: Bounds defined by monkey shimmy left/right states.
		};

		return TestMonkeySwingSetup(item, coll, setup);
	}

	bool CanMonkeySwingShimmyLeft(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestMonkeyShimmy(item, coll, false);
	}

	bool CanMonkeySwingShimmyRight(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestMonkeyShimmy(item, coll, true);
	}

	bool CanFall(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto UPPER_FLOOR_BOUND = STEPUP_HEIGHT;

		const auto& player = GetLaraInfo(item);

		// 1) Test player water status.
		if (player.Control.WaterStatus == WaterStatus::Wade)
			return false;

		// Get point collision.
		auto pointColl = GetCollision(&item, 0, 0, -coll.Setup.Height / 2);
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;

		// 2) Assess point collision.
		if (relFloorHeight > UPPER_FLOOR_BOUND) // Floor height is below upper floor bound.
			return true;

		return false;
	}

	bool CanLand(const ItemInfo& item, const CollisionInfo& coll)
	{
		float projVerticalVel = item.Animation.Velocity.y + GetEffectiveGravity(item.Animation.Velocity.y);

		// 1) Check airborne status and vertical velocity.
		if (!item.Animation.IsAirborne || projVerticalVel < 0.0f)
			return false;

		// 2) Check for swamp.
		if (TestEnvironment(ENV_FLAG_SWAMP, &item))
			return true;

		// Get point collision.
		auto pointColl = GetCollision(&item);
		int vPos = item.Pose.Position.y;

		// 3) Assess point collision.
		if ((pointColl.Position.Floor - vPos) <= projVerticalVel) // Floor height is above projected vertical position.
			return true;

		return false;
	}

	bool CanPerformJump(const ItemInfo& item, const CollisionInfo& coll)
	{
		return !TestEnvironment(ENV_FLAG_SWAMP, &item);
	}

	static bool TestJumpSetup(const ItemInfo& item, const CollisionInfo& coll, const JumpSetupData& setup)
	{
		const auto& player = GetLaraInfo(item);

		bool isWading = setup.TestWadeStatus ? (player.Control.WaterStatus == WaterStatus::Wade) : false;
		bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, &item);

		// 1) Check for swamp or wade status (if applicable).
		if (isWading || isInSwamp)
			return false;

		// 2) Check for corner.
		if (TestLaraFacingCorner(&item, setup.HeadingAngle, setup.Distance))
			return false;

		// Prepare data for static object LOS.
		// TODO: Lock-up occurs.
		/*auto origin = Geometry::TranslatePoint(item.Pose.Position.ToVector3(), item.Pose.Orientation.y, OFFSET_RADIUS(coll.Setup.Radius));
		auto target = origin + Vector3(0.0f,-STEPUP_HEIGHT, 0.0f);
		auto dir = target - origin;
		dir.Normalize();

		// 3) Assess ray-static collision.
		auto staticLos = GetStaticObjectLos(origin, item.RoomNumber, dir, Vector3::Distance(origin, target), false);
		if (staticLos.has_value())
			return false;*/

		// Get point collision.
		auto pointColl = GetCollision(&item, setup.HeadingAngle, setup.Distance, -coll.Setup.Height);
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;
		int relCeilHeight = pointColl.Position.Ceiling - item.Pose.Position.y;

		// 4) Assess point collision.
		if (relFloorHeight >= -STEPUP_HEIGHT &&								  // Floor is within highest floor bound.
			(relCeilHeight < -(coll.Setup.Height + (LARA_HEADROOM * 0.8f)) || // Ceiling is within lowest ceiling bound.
				(relCeilHeight < -coll.Setup.Height &&						  // OR ceiling is level with Lara's head.
					relFloorHeight >= CLICK(0.5f))))						  // AND there is a drop below.
		{
			return true;
		}

		return false;
	}

	bool CanJumpUp(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto setup = JumpSetupData
		{
			0,
			0.0f,
			false
		};

		return TestJumpSetup(item, coll, setup);
	}

	static bool TestDirectionalStandingJump(const ItemInfo& item, const CollisionInfo& coll, short relHeadingAngle)
	{
		// Check for swamp.
		if (TestEnvironment(ENV_FLAG_SWAMP, &item))
			return false;

		auto setup = JumpSetupData
		{
			short(item.Pose.Orientation.y + relHeadingAngle),
			CLICK(0.85f)
		};

		return TestJumpSetup(item, coll, setup);
	}

	bool CanJumpForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestDirectionalStandingJump(item, coll, ANGLE(0.0f));
	}

	bool CanJumpBackward(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestDirectionalStandingJump(item, coll, ANGLE(180.0f));
	}

	bool CanJumpLeft(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestDirectionalStandingJump(item, coll, ANGLE(-90.0f));
	}

	bool CanJumpRight(const ItemInfo& item, const CollisionInfo& coll)
	{
		return TestDirectionalStandingJump(item, coll, ANGLE(90.0f));
	}

	bool CanQueueRunningJump(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto UPPER_FLOOR_BOUND	 = CLICK(0.5f);
		constexpr auto LOWER_CEIL_BOUND_BASE = -LARA_HEADROOM * 0.8f;

		auto& player = GetLaraInfo(item);

		// 1) Test if running jump is immediately possible.
		if (CanRunJumpForward(item, coll))
			return IsRunJumpQueueableState(item.Animation.TargetState);

		// Get point collision.
		auto pointColl = GetCollision(&item, item.Pose.Orientation.y, BLOCK(1), -coll.Setup.Height);

		int lowerCeilingBound = (LOWER_CEIL_BOUND_BASE - coll.Setup.Height);
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;
		int relCeilHeight = pointColl.Position.Ceiling - item.Pose.Position.y;

		// 2) Assess point collision for possible running jump ahead.
		if (relCeilHeight < lowerCeilingBound || // Ceiling height is above lower ceiling bound.
			relFloorHeight >= UPPER_FLOOR_BOUND) // OR floor height ahead is below upper floor bound.
		{
			return IsRunJumpQueueableState(item.Animation.TargetState);
		}

		return false;
	}

	bool CanRunJumpForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// Check running jump timer.
		if (player.Control.Count.Run < PLAYER_RUN_JUMP_TIME)
			return false;

		auto setup = JumpSetupData
		{
			item.Pose.Orientation.y,
			CLICK(3 / 2.0f)
		};

		return TestJumpSetup(item, coll, setup);
	}

	bool CanSprintJumpForward(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		// 1) Check if sprint jump is enabled.
		if (!g_GameFlow->HasSprintJump())
			return false;

		// 2) Check for jump state dispatch.
		if (!HasStateDispatch(&item, LS_JUMP_FORWARD))
			return false;

		// 3) Check running jump timer.
		if (player.Control.Count.Run < PLAYER_SPRINT_JUMP_TIME)
			return false;

		auto setup = JumpSetupData
		{
			item.Pose.Orientation.y,
			CLICK(1.8f)
		};

		// 4) Assess context.
		return TestJumpSetup(item, coll, setup);
	}

	bool CanPerformSlideJump(const ItemInfo& item, const CollisionInfo& coll)
	{
		// TODO: Get back to this project. -- Sezz 2022.11.11
		return true;

		// Check whether extended slide mechanics are enabled.
		if (!g_GameFlow->HasSlideExtended())
			return true;

		// TODO: Broken on diagonal slides?

		auto pointColl = GetCollision(&item);

		//short aspectAngle = GetLaraSlideHeadingAngle(item, coll);
		//short slopeAngle = Geometry::GetSurfaceSlopeAngle(GetSurfaceNormal(pointColl.FloorTilt, true));
		//return (abs(short(coll.Setup.ForwardAngle - aspectAngle)) <= abs(slopeAngle));
	}

	bool CanCrawlspaceDive(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto pointColl = GetCollision(&item, coll.Setup.ForwardAngle, coll.Setup.Radius, -coll.Setup.Height);
		return (abs(pointColl.Position.Ceiling - pointColl.Position.Floor) < LARA_HEIGHT || IsInLowSpace(item, coll));
	}

	bool CanSwingOnLedge(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto UPPER_FLOOR_BOUND = 0;
		constexpr auto LOWER_CEIL_BOUND	 = CLICK(1.5f);

		auto& player = GetLaraInfo(item);

		// Get point collision.
		auto pointColl = GetCollision(&item, item.Pose.Orientation.y, OFFSET_RADIUS(coll.Setup.Radius));
		int relFloorHeight = pointColl.Position.Floor - item.Pose.Position.y;
		int relCeilHeight = pointColl.Position.Ceiling - (item.Pose.Position.y - coll.Setup.Height);

		// Assess point collision.
		if (relFloorHeight >= UPPER_FLOOR_BOUND && // Floor height is below upper floor bound.
			relCeilHeight <= LOWER_CEIL_BOUND)	   // Ceiling height is above lower ceiling bound.
		{
			return true;
		}

		return false;
	}

	bool CanPerformLedgeJump(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto WALL_HEIGHT_MIN = CLICK(2);

		// 1) Check if ledge jumps are enabled.
		if (!g_GameFlow->HasLedgeJumps())
			return false;

		// Ray collision setup at minimum ledge height.
		auto origin = GameVector(
			item.Pose.Position.x,
			(item.Pose.Position.y - LARA_HEIGHT_STRETCH) + WALL_HEIGHT_MIN,
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

	static bool TestLedgeClimbSetup(const ItemInfo& item, CollisionInfo& coll, const LedgeClimbSetupData& setup)
	{
		constexpr auto REL_FLOOR_HEIGHT_THRESHOLD = CLICK(0.5f);

		const auto& player = GetLaraInfo(item);

		// 1) Check for attractor parent.
		if (player.Context.Attractor.Ptr == nullptr)
			return false;

		// Get attractor collision.
		auto attracColl = GetAttractorCollision(*player.Context.Attractor.Ptr, player.Context.Attractor.ChainDistance, item.Pose.Orientation.y);

		// TODO: Probe from player.
		// Get point collision in front of edge. NOTE: Height offset required for correct bridge collision.
		auto pointCollFront = GetCollision(
			attracColl.Proximity.Intersection, attracColl.AttractorPtr->GetRoomNumber(),
			attracColl.HeadingAngle, coll.Setup.Radius, -CLICK(1));

		// TODO: This check fails for no reason.
		// 2) Test for illegal slope (if applicable).
		if (setup.TestIllegalSlope)
		{
			if (pointCollFront.Position.FloorSlope)
				return false;
		}

		// 3) Test for object blocking ledge.
		if (TestForObjectOnLedge(item, coll))
			return false;

		// 4) Test ledge floor-to-edge height.
		int ledgeFloorToEdgeHeight = abs(attracColl.Proximity.Intersection.y - pointCollFront.Position.Floor);
		if (ledgeFloorToEdgeHeight > REL_FLOOR_HEIGHT_THRESHOLD)
			return false;
		
		// 5) Test ledge floor-to-ceiling height.
		int ledgeFloorToCeilHeight = abs(pointCollFront.Position.Ceiling - pointCollFront.Position.Floor);
		if (ledgeFloorToCeilHeight <= setup.LedgeFloorToCeilHeightMin ||
			ledgeFloorToCeilHeight > setup.LedgeFloorToCeilHeightMax)
		{
			return false;
		}

		// Get point collision behind edge.
		auto pointCollBack = GetCollision(
			attracColl.Proximity.Intersection, attracColl.AttractorPtr->GetRoomNumber(),
			attracColl.HeadingAngle, -coll.Setup.Radius);

		// 6) Test if ceiling behind is adequately higher than edge.
		int edgeToCeilHeight = pointCollBack.Position.Ceiling - pointCollFront.Position.Floor;
		if (edgeToCeilHeight > setup.LowerEdgeToCeilBound)
			return false;

		return true;
	}	

	bool CanPerformLedgeHandstand(const ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto SETUP = LedgeClimbSetupData
		{
			LARA_HEIGHT, -MAX_HEIGHT,
			CLICK(3),
			false
		};

		return TestLedgeClimbSetup(item, coll, SETUP);
	}

	bool CanClimbLedgeToCrouch(const ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto SETUP = LedgeClimbSetupData
		{
			LARA_HEIGHT_CRAWL, LARA_HEIGHT,
			(int)CLICK(0.6f),
			true
		};

		return TestLedgeClimbSetup(item, coll, SETUP);
	}

	bool CanClimbLedgeToStand(const ItemInfo& item, CollisionInfo& coll)
	{
		constexpr auto SETUP = LedgeClimbSetupData
		{
			LARA_HEIGHT, -MAX_HEIGHT,
			CLICK(1),
			false
		};

		return TestLedgeClimbSetup(item, coll, SETUP);
	}

	bool CanShimmyUp(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto WALL_STEP_HEIGHT = -CLICK(1);

		auto& player = GetLaraInfo(item);

		// 1) Check for climbable wall flag.
		if (!player.Control.CanClimbLadder)
			return false;

		// Get point collision.
		auto pointCollCenter = GetCollision(&item);
		auto pointCollLeft = GetCollision(&item, item.Pose.Orientation.y - ANGLE(90.0f), OFFSET_RADIUS(coll.Setup.Radius));
		auto pointCollRight = GetCollision(&item, item.Pose.Orientation.y + ANGLE(90.0f), OFFSET_RADIUS(coll.Setup.Radius));

		int vPos = item.Pose.Position.y - LARA_HEIGHT_STRETCH;
		int relCeilHeightCenter = pointCollCenter.Position.Ceiling - vPos;
		int relCeilHeightLeft = pointCollCenter.Position.Ceiling - vPos;
		int relCeilHeightRight = pointCollCenter.Position.Ceiling - vPos;

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
	bool CanShimmyDown(const ItemInfo& item, const CollisionInfo& coll)
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

	bool CanShimmyLeft(ItemInfo& item, CollisionInfo& coll)
	{
		return true;

		//return TestLaraHangSideways(&item, &coll, ANGLE(-90.0f));
	}

	bool CanShimmyRight(ItemInfo& item, CollisionInfo& coll)
	{
		return true;

		//return TestLaraHangSideways(&item, &coll, ANGLE(90.0f));
	}

	// TODO
	void GetClimbableWallShimmyUpContext(const ItemInfo& item, const CollisionInfo& coll)
	{

	}
	
	void GetHangShimmyLeftContext(const ItemInfo& item, const CollisionInfo& coll)
	{

	}
	
	void GetHangShimmyRightContext(const ItemInfo& item, const CollisionInfo& coll)
	{

	}

	bool CanDismountTightrope(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		auto pointColl = GetCollision(&item);

		if (player.Control.Tightrope.CanDismount &&			  // Dismount is allowed.
			pointColl.Position.Floor == item.Pose.Position.y) // Floor is level with player.
		{
			return true;
		}

		return false;
	}

	static std::optional<AttractorCollisionData> GetEdgeVaultClimbAttractorCollision(const ItemInfo& item, const CollisionInfo& coll,
																					 const EdgeVaultClimbSetupData& setup,
																					 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SWAMP_DEPTH_MAX			  = -CLICK(3);
		constexpr auto REL_FLOOR_HEIGHT_THRESHOLD = CLICK(0.5f);

		// HACK: Offset required for proper bridge surface height detection. Floordata should be revised for proper handling.
		constexpr auto PROBE_POINT_OFFSET = Vector3(0.0f, -CLICK(1), 0.0f);
		
		const auto& player = GetLaraInfo(item);

		// 1) Test swamp depth (if applicable).
		if (setup.TestSwampDepth)
		{
			if (TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber) && player.Context.WaterSurfaceDist < SWAMP_DEPTH_MAX)
				return std::nullopt;
		}

		const AttractorCollisionData* highestAttracCollPtr = nullptr;

		// 2) Assess attractor collision.
		float range2D = OFFSET_RADIUS(coll.Setup.Radius);
		for (const auto& attracColl : attracColls)
		{
			// 2.1) Check if attractor is edge or wall edge type.
			if (attracColl.AttractorPtr->GetType() != AttractorType::Edge &&
				attracColl.AttractorPtr->GetType() != AttractorType::WallEdge)
			{
				continue;
			}

			// 2.2) Test if edge is within 2D range.
			if (attracColl.Proximity.Distance2D > range2D)
				continue;

			// 2.3) Test if edge slope is illegal.
			if (abs(attracColl.SlopeAngle) >= ILLEGAL_FLOOR_SLOPE_ANGLE)
				continue;

			// 2.4) Test relation to edge intersection.
			if (!attracColl.IsInFront ||
				!TestPlayerInteractAngle(item.Pose.Orientation.y, attracColl.HeadingAngle + (setup.TestEdgeFront ? ANGLE(0.0f) : ANGLE(180.0f))))
			{
				continue;
			}

			// TODO: Point collision probing may traverse rooms correctly if bridges cross rooms.
			// Solution is to probe from player's position and room. Combine player/intersect RelDeltaPos and RelPosOffset.

			// Get point collision at edge.
			auto pointCollCenter = GetCollision(
				Vector3i(attracColl.Proximity.Intersection.x, attracColl.Proximity.Intersection.y - 1, attracColl.Proximity.Intersection.z),
				attracColl.AttractorPtr->GetRoomNumber(),
				attracColl.HeadingAngle, -coll.Setup.Radius, PROBE_POINT_OFFSET.y);

			// 2.5) Test if intersection is blocked by ceiling.
			if (attracColl.Proximity.Intersection.y <= pointCollCenter.Position.Ceiling)
				continue;

			// Get point collision behind edge.
			auto pointCollBack = GetCollision(
				Vector3i(attracColl.Proximity.Intersection), attracColl.AttractorPtr->GetRoomNumber(),
				attracColl.HeadingAngle, -coll.Setup.Radius, PROBE_POINT_OFFSET.y);

			// TODO: Simplify! Trash! Bad!
			bool isTreadingWater = (player.Control.WaterStatus == WaterStatus::TreadWater);
			int relEdgeHeight = attracColl.Proximity.Intersection.y - (isTreadingWater ? item.Pose.Position.y : pointCollBack.Position.Floor);
			int relPlayerFloorHeight = abs(item.Pose.Position.y - (setup.TestEdgeFront ? pointCollBack.Position.Floor : attracColl.Proximity.Intersection.y));

			// 2.6) Test if relative edge height is within edge intersection bounds.
			if (relEdgeHeight >= setup.LowerEdgeBound ||
				relEdgeHeight < setup.UpperEdgeBound ||
				(relPlayerFloorHeight > REL_FLOOR_HEIGHT_THRESHOLD && !isTreadingWater))
			{
				continue;
			}

			// 2.7) Test if ceiling behind is adequately higher than edge.
			int edgeToCeilHeight = pointCollBack.Position.Ceiling - attracColl.Proximity.Intersection.y;
			if (edgeToCeilHeight > setup.LowerEdgeToCeilBound)
				continue;

			// Get point collision in front of edge.
			auto pointCollFront = GetCollision(
				Vector3i(attracColl.Proximity.Intersection), attracColl.AttractorPtr->GetRoomNumber(),
				attracColl.HeadingAngle, coll.Setup.Radius, PROBE_POINT_OFFSET.y);

			// Test destination space (if applicable).
			if (setup.TestDestSpace)
			{
				const auto& destPointColl = setup.TestEdgeFront ? pointCollFront : pointCollBack;

				// 2.8) Test destination floor-to-ceiling height.
				int destFloorToCeilHeight = abs(destPointColl.Position.Ceiling - destPointColl.Position.Floor);
				if (destFloorToCeilHeight <= setup.LedgeFloorToCeilHeightMin ||
					destFloorToCeilHeight > setup.LedgeFloorToCeilHeightMax)
				{
					continue;
				}

				// 2.9) Test destination floor-to-edge height if approaching from front.
				if (setup.TestEdgeFront)
				{
					int destFloorToEdgeHeight = abs(attracColl.Proximity.Intersection.y - destPointColl.Position.Floor);
					if (destFloorToEdgeHeight > REL_FLOOR_HEIGHT_THRESHOLD)
						continue;
				}

				const auto& staticPointColl = setup.TestEdgeFront ? pointCollFront : pointCollBack;
				auto origin = Vector3(staticPointColl.Coordinates.x, staticPointColl.Position.Floor, staticPointColl.Coordinates.z);
				auto dir = setup.TestEdgeFront ? -Vector3::UnitY : Vector3::UnitY;

				// 2.10) Test for static object.
				auto staticLos = GetStaticObjectLos(origin, attracColl.AttractorPtr->GetRoomNumber(), dir, coll.Setup.Height, false);
				if (staticLos.has_value())
					continue;
			}

			// 2.11) Test for illegal slope at destination (if applicable).
			if (setup.TestDestIllegalSlope)
			{
				if (setup.TestEdgeFront ? pointCollFront.Position.FloorSlope : pointCollBack.Position.FloorSlope)
					continue;
			}

			// 2.12) Track highest or return lowest attractor collision.
			if (setup.FindHighest)
			{
				if (highestAttracCollPtr == nullptr)
				{
					highestAttracCollPtr = &attracColl;
					continue;
				}

				if (attracColl.Proximity.Intersection.y < highestAttracCollPtr->Proximity.Intersection.y)
				{
					// Ensure attractors are stacked exactly.
					auto highest2DIntersect = Vector2(highestAttracCollPtr->Proximity.Intersection.x, highestAttracCollPtr->Proximity.Intersection.z);
					auto current2DIntersect = Vector2(attracColl.Proximity.Intersection.x, attracColl.Proximity.Intersection.z);
					if (Vector2::DistanceSquared(highest2DIntersect, current2DIntersect) > EPSILON)
						continue;

					highestAttracCollPtr = &attracColl;
				}
			}
			else
			{
				return attracColl;
			}
		}

		// Return highest attractor collision (if applicable).
		if (highestAttracCollPtr != nullptr)
			return *highestAttracCollPtr;

		// No valid edge attractor collision; return nullopt.
		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetStandVault2StepsUpClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																			 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-STEPUP_HEIGHT, -(int)CLICK(2.5f), // Edge height bounds.
			LARA_HEIGHT, -MAX_HEIGHT,		   // Ledge floor-to-ceil range.
			-CLICK(1),						   // Edge-to-ceil height lower bound.
			false,							   // Find highest.
			false,							   // Test swamp depth.
			true,							   // Test edge front.
			true,							   // Test ledge heights.
			true							   // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(2);

		// Get standing vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_STAND_VAULT_2_STEPS_UP;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetStandVault3StepsUpClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																			 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-(int)CLICK(2.5f), -(int)CLICK(3.5f), // Edge height bounds.
			LARA_HEIGHT, -MAX_HEIGHT,			  // Ledge floor-to-ceil range.
			-CLICK(1),							  // Edge-to-ceil height lower bound.
			false,								  // Find highest.
			false,								  // Test swamp depth.
			true,								  // Test edge front.
			true,								  // Test ledge heights.
			true								  // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(3);

		// Get standing vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_STAND_VAULT_3_STEPS_UP;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetStandVault1StepUpToCrouchClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																					const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			0, -STEPUP_HEIGHT,				// Edge height bounds.
			LARA_HEIGHT_CRAWL, LARA_HEIGHT, // Ledge floor-to-ceil range.
			-CLICK(1),						// Edge-to-ceil height lower bound.
			false,							// Find highest.
			false,							// Test swamp depth.
			true,							// Test edge front.
			true,							// Test ledge heights.
			true							// Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(1);

		// Get standing vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_STAND_VAULT_1_STEP_UP_TO_CROUCH;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetStandVault2StepsUpToCrouchClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																					 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-STEPUP_HEIGHT, -(int)CLICK(2.5f), // Edge height bounds.
			LARA_HEIGHT_CRAWL, LARA_HEIGHT,	   // Ledge floor-to-ceil range.
			-CLICK(1),						   // Edge-to-ceil height lower bound.
			false,							   // Find highest.
			false,							   // Test swamp depth.
			true,							   // Test edge front.
			true,							   // Test ledge heights.
			true							   // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(2);

		// Get standing vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_STAND_VAULT_2_STEPS_UP_TO_CROUCH;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetStandVault3StepsUpToCrouchClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																					 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-(int)CLICK(2.5f), -(int)CLICK(3.5f), // Edge height bounds.
			LARA_HEIGHT_CRAWL, LARA_HEIGHT,		  // Ledge floor-to-ceil range.
			-CLICK(1),							  // Edge-to-ceil height lower bound.
			false,								  // Find highest.
			false,								  // Test swamp depth.
			true,								  // Test edge front.
			true,								  // Test ledge heights.
			true								  // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(3);

		// Get standing vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_STAND_VAULT_3_STEPS_UP_TO_CROUCH;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetAutoJumpClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																   const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto LOWER_CEIL_BOUND = -LARA_HEIGHT_MONKEY;
		constexpr auto UPPER_CEIL_BOUND = -CLICK(7);

		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-(int)CLICK(3.5f), -(int)CLICK(7.5f), // Edge height bounds.
			0, -MAX_HEIGHT,						  // Ledge floor-to-ceil range.
			-(int)CLICK(1 / 256.0f),			  // Edge-to-ceil height minumum.
			true,								  // Find highest.
			false,								  // Test swamp depth.
			true,								  // Test edge front.
			false,								  // Test ledge heights.
			false								  // Test ledge illegal slope.
		};

		const auto& player = GetLaraInfo(item);

		// 1) Get edge auto jump climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			int relEdgeHeight = attracColl->Proximity.Intersection.y - item.Pose.Position.y;

			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, -relEdgeHeight, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_AUTO_JUMP;
			context.AlignType = ClimbContextAlignType::OffsetBlend;
			context.SetBusyHands = false;
			context.SetJumpVelocity = true;

			return context;
		}

		// Auto jump to monkey swing disabled; return early.
		if (!g_GameFlow->HasMonkeyAutoJump())
			return std::nullopt;

		// Get point collision.
		auto pointColl = GetCollision(item);
		int relCeilHeight = pointColl.Position.Ceiling - item.Pose.Position.y;

		// 2) Get auto jump to monkey swing climb context.
		if (player.Control.CanMonkeySwing &&	// Player is standing below monkey swing.
			relCeilHeight < LOWER_CEIL_BOUND && // Ceiling height is within lower ceiling bound.
			relCeilHeight >= UPPER_CEIL_BOUND)	// Ceiling height is within upper ceiling bound.
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = nullptr;
			context.ChainDistance = 0.0f;
			context.RelPosOffset = Vector3::Zero;
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_AUTO_JUMP;
			context.AlignType = ClimbContextAlignType::None;
			context.SetBusyHands = false;
			context.SetJumpVelocity = true;

			return context;
		}

		return std::nullopt;
	}

	// TODO: Currently still using legacy climbable wall flags.
	static std::optional<ClimbContextData> GetClimbableWallMountClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																			 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SWAMP_DEPTH_MAX = -CLICK(3);

		const auto& player = GetLaraInfo(item);

		float range2D = OFFSET_RADIUS(coll.Setup.Radius);
		//int vPos = round(item.Pose.Position.y / CLICK(1)) * CLICK(1);

		// TODO: Find 4 stacked WallEdge attractors.
		for (auto& attrac : attracColls)
		{
			break;
		}

		// 1) Check if wall is climbable.
		if (!player.Control.CanClimbLadder)
			return std::nullopt;

		// 2) Test swamp depth.
		if (TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber) && player.Context.WaterSurfaceDist < SWAMP_DEPTH_MAX)
			return std::nullopt;

		// 3) Test relation to wall.
		short wallHeadingAngle = GetQuadrant(item.Pose.Orientation.y) * ANGLE(90.0f);
		if (!TestPlayerInteractAngle(item.Pose.Orientation.y, wallHeadingAngle))
			return std::nullopt;

		// Get point collision.
		auto pointCollCenter = GetCollision(item);
		auto pointCollFront = GetCollision(&item, wallHeadingAngle, OFFSET_RADIUS(coll.Setup.Radius), -coll.Setup.Height);
		int vPos = item.Pose.Position.y;

		// 4) Assess point collision.
		if ((pointCollCenter.Position.Ceiling - vPos) > -CLICK(4.5f) ||	 // Ceiling height is within lower center ceiling bound.
			(pointCollCenter.Position.Ceiling - vPos) <= -CLICK(6.5f) || // Ceiling height is within upper center ceiling bound.
			(pointCollCenter.Position.Floor - vPos) <= -CLICK(6.5f) ||	 // Floor height is within upper center floor bound.
			(pointCollFront.Position.Ceiling - vPos) > -CLICK(4.5f))	 // Ceiling height is within lowest front ceiling bound.
		{
			return std::nullopt;
		}

		auto& nonConstPlayerItem = *LaraItem;
		auto& nonConstPlayerColl = LaraCollision;

		// 5) Test and set climbable wall mount.
		if (!TestLaraClimbIdle(&nonConstPlayerItem, &nonConstPlayerColl))
			return std::nullopt;

		SnapEntityToGrid(nonConstPlayerItem, coll);
		nonConstPlayerItem.Pose.Orientation.y = wallHeadingAngle;

		auto context = ClimbContextData{};
		context.AttractorPtr = nullptr;
		context.ChainDistance = 0.0f;
		context.RelPosOffset = Vector3(0.0f, 0.0f, -coll.Setup.Radius);
		context.RelOrientOffset = EulerAngles::Zero;
		context.TargetStateID = LS_LADDER_IDLE;
		context.AlignType = ClimbContextAlignType::None;
		context.SetBusyHands = true;
		context.SetJumpVelocity = false;

		return context;
	}

	std::optional<ClimbContextData> GetStandVaultClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS = BLOCK(2);

		const auto& player = GetLaraInfo(item);

		// Check hand status.
		if (player.Control.HandStatus != HandStatus::Free)
			return std::nullopt;

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(item.Pose.Position.ToVector3(), item.RoomNumber, item.Pose.Orientation.y, ATTRAC_DETECT_RADIUS);

		auto context = std::optional<ClimbContextData>();

		// 1) Stand vault 1 step up to crouch.
		context = GetStandVault1StepUpToCrouchClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 2) Stand vault 2 steps up to crouch.
		context = GetStandVault2StepsUpClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 3) Stand vault 2 steps up.
		context = GetStandVault2StepsUpToCrouchClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 4) Stand vault 3 steps up to crouch.
		context = GetStandVault3StepsUpToCrouchClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 5) Stand vault 3 steps up.
		context = GetStandVault3StepsUpClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 6) Mount climbable wall.
		context = GetClimbableWallMountClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 7) Auto jump.
		context = GetAutoJumpClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetCrawlVault1StepDownClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																			  const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			STEPUP_HEIGHT, CRAWL_STEPUP_HEIGHT, // Edge height bounds.
			LARA_HEIGHT_CRAWL, -MAX_HEIGHT,		// Ledge floor-to-ceil range.
			-(int)CLICK(0.6f),					// Edge-to-ceil height lower bound.
			false,								// Find highest.
			false,								// Test swamp depth.
			false,								// Test edge front.
			true,								// Test ledge heights.
			true								// Test ledge illegal slope.
		};

		// Get crawling vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, 0.0f, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles(0, ANGLE(180.0f), 0);
			context.TargetStateID = LS_CRAWL_VAULT_1_STEP_DOWN;
			context.AlignType = ClimbContextAlignType::OffsetBlend;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetCrawlVault1StepDownToStandClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																					 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			STEPUP_HEIGHT, -CRAWL_STEPUP_HEIGHT, // Edge height bounds.
			LARA_HEIGHT, -MAX_HEIGHT,			 // Ledge floor-to-ceil range.
			-(int)CLICK(1.25f),					 // Edge-to-ceil height lower bound.
			false,								 // Find highest.
			false,								 // Test swamp depth.
			false,								 // Test edge front.
			true,								 // Test ledge heights.
			false								 // Test ledge illegal slope.
		};

		// Crouch action held; return nullopt.
		if (IsHeld(In::Crouch))
			return std::nullopt;

		// Get crawling vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.RelPosOffset = Vector3(0.0f, 0.0f, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles(0, ANGLE(180.0f), 0);
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.TargetStateID = LS_CRAWL_VAULT_1_STEP_DOWN_TO_STAND;
			context.AlignType = ClimbContextAlignType::OffsetBlend;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetCrawlVault1StepUpClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																			const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-CRAWL_STEPUP_HEIGHT, -STEPUP_HEIGHT, // Edge height bounds.
			LARA_HEIGHT_CRAWL, -MAX_HEIGHT,		  // Ledge floor-to-ceil range.
			-(int)CLICK(0.6f),					  // Edge-to-ceil height lower bound.
			false,								  // Find highest.
			false,								  // Test swamp depth.
			true,								  // Test edge front.
			true,								  // Test ledge heights.
			true								  // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(1);

		// Get crawling vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())

		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_CRAWL_VAULT_1_STEP_UP;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetCrawlVaultJumpClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																		 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			NO_LOWER_BOUND, STEPUP_HEIGHT, // Edge height bounds.
			LARA_HEIGHT, -MAX_HEIGHT,	   // Ledge floor-to-ceil range.
			-(int)CLICK(1.25f),			   // Edge-to-ceil height lower bound.
			false,						   // Find highest.
			false,						   // Test swamp depth.
			false,						   // Test edge front.
			true,						   // Test ledge heights.
			false						   // Test ledge illegal slope.
		};

		// 1) Get edge crawl vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.RelPosOffset = Vector3(0.0f, 0.0f, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles(0, ANGLE(180.0f), 0);
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.TargetStateID = IsHeld(In::Walk) ? LS_CRAWL_VAULT_JUMP_FLIP : LS_CRAWL_VAULT_JUMP;
			context.AlignType = ClimbContextAlignType::OffsetBlend;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		auto pointCollCenter = GetCollision(item);
		auto pointCollFront = GetCollision(&item, item.Pose.Orientation.y, BLOCK(0.25f), -coll.Setup.Height);
		int relFloorHeight = pointCollFront.Position.Floor - item.Pose.Position.y;

		// TODO
		// 2) Get illegal slope crawl vault climb context (special case).
		if (pointCollFront.Position.FloorSlope &&
			true
			/*relFloorHeight <= SETUP.LowerEdgeBound &&							// Within lower floor bound.
			relFloorHeight >= SETUP.UpperEdgeBound &&							// Within upper floor bound.

			abs(pointCollFront.Position.Ceiling - pointCollFront.Position.Floor) > testSetup.ClampMin &&		// Crossing clamp limit.
			abs(pointCollCenter.Position.Ceiling - probeB.Position.Floor) > testSetup.ClampMin &&		// Destination clamp limit.
			abs(probeMiddle.Position.Ceiling - pointCollFront.Position.Floor) >= testSetup.GapMin &&	// Gap is optically permissive (going up).
			abs(probeA.Position.Ceiling - probeMiddle.Position.Floor) >= testSetup.GapMin &&	// Gap is optically permissive (going down).
			abs(probeA.Position.Floor - probeB.Position.Floor) <= testSetup.FloorBound &&		// Crossing/destination floor height difference suggests continuous crawl surface.
			(probeA.Position.Ceiling - y) < -testSetup.GapMin*/)									// Ceiling height is permissive.
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = nullptr;
			context.ChainDistance = 0.0f;
			context.RelPosOffset = Vector3::Zero;
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_CRAWL_VAULT_JUMP;
			context.AlignType = ClimbContextAlignType::None;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	std::optional<ClimbContextData> GetCrawlVaultClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS = BLOCK(0.5f);

		const auto& player = GetLaraInfo(item);

		// Extended crawl moveset disabled; return nullopt.
		if (!g_GameFlow->HasCrawlExtended())
			return std::nullopt;

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(item, ATTRAC_DETECT_RADIUS);

		auto context = std::optional<ClimbContextData>();

		// 1) Crawl vault down 1 step to stand.
		context = GetCrawlVault1StepDownToStandClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 2) Crawl vault down 1 step.
		context = GetCrawlVault1StepDownClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 3) Crawl vault jump.
		context = GetCrawlVaultJumpClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 4) Crawl vault up 1 step.
		context = GetCrawlVault1StepUpClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetTreadWaterVault1StepDownToStandClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																						  const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			STEPUP_HEIGHT, (int)CLICK(0.5f), // Edge height bounds.
			LARA_HEIGHT, -MAX_HEIGHT,		 // Ledge floor-to-ceil range.
			-CLICK(1),						 // Edge-to-ceil height lower bound.
			false,							 // Find highest.
			false,							 // Test swamp depth.
			true,							 // Test edge front.
			true,							 // Test ledge heights.
			true							 // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = -CLICK(1);

		// Crouch action held and extended crawl moveset enabled; return nullopt.
		if (IsHeld(In::Crouch) && g_GameFlow->HasCrawlExtended())
			return std::nullopt;

		// Get tread water vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_TREAD_WATER_VAULT_1_STEP_DOWN_TO_STAND;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetTreadWaterVault0StepsToStandClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																					   const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			(int)CLICK(0.5f), -(int)CLICK(0.5f), // Edge height bounds.
			LARA_HEIGHT, -MAX_HEIGHT,			 // Ledge floor-to-ceil range.
			-CLICK(1),							 // Edge-to-ceil height lower bound.
			false,								 // Find highest.
			false,								 // Test swamp depth.
			true,								 // Test edge front.
			true,								 // Test ledge heights.
			true								 // Test ledge illegal slope.
		};

		// Crouch action held and extended crawl moveset enabled; return nullopt.
		if (IsHeld(In::Crouch) && g_GameFlow->HasCrawlExtended())
			return std::nullopt;

		// Get tread water vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, 0.0f, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_TREAD_WATER_VAULT_0_STEPS_TO_STAND;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}
	
	static std::optional<ClimbContextData> GetTreadWaterVault1StepUpToStandClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																						const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-(int)CLICK(0.5f), -STEPUP_HEIGHT, // Edge height bounds.
			LARA_HEIGHT, -MAX_HEIGHT,		   // Ledge floor-to-ceil range.
			-CLICK(1),						   // Edge-to-ceil height lower bound.
			false,							   // Find highest.
			false,							   // Test swamp depth.
			true,							   // Test edge front.
			true,							   // Test ledge heights.
			true							   // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(1);

		// Crouch action held and extended crawl moveset enabled; return nullopt.
		if (IsHeld(In::Crouch) && g_GameFlow->HasCrawlExtended())
			return std::nullopt;

		// Get tread water vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_TREAD_WATER_VAULT_1_STEP_UP_TO_STAND;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetTreadWaterVault1StepDownToCrouchClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																						   const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			STEPUP_HEIGHT, (int)CLICK(0.5f), // Edge height bounds.
			LARA_HEIGHT_CRAWL, -MAX_HEIGHT,	 // Ledge floor-to-ceil range.
			-(int)CLICK(0.6f),				 // Edge-to-ceil height lower bound.
			false,							 // Find highest.
			false,							 // Test swamp depth.
			true,							 // Test edge front.
			true,							 // Test ledge heights.
			true							 // Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = -CLICK(1);

		// Extended crawl moveset disabled; return nullopt.
		if (!g_GameFlow->HasCrawlExtended())
			return std::nullopt;

		// Get tread water vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_TREAD_WATER_VAULT_1_STEP_DOWN_TO_CROUCH;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetTreadWaterVault0StepsToCrouchClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																						const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			(int)CLICK(0.5f), -(int)CLICK(0.5f), // Edge height bounds.
			LARA_HEIGHT_CRAWL, -MAX_HEIGHT,		 // Ledge floor-to-ceil range.
			-(int)CLICK(0.6f),					 // Edge-to-ceil height lower bound.
			false,								 // Find highest.
			false,								 // Test swamp depth.
			true,								 // Test edge front.
			true,								 // Test ledge heights.
			true								 // Test ledge illegal slope.
		};

		// Extended crawl moveset disabled; return nullopt.
		if (!g_GameFlow->HasCrawlExtended())
			return std::nullopt;

		// Get tread water vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, 0.0f, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_TREAD_WATER_VAULT_0_STEPS_TO_CROUCH;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetTreadWaterVault1StepUpToCrouchClimbContext(const ItemInfo& item, const CollisionInfo& coll,
																						 const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto SETUP = EdgeVaultClimbSetupData
		{
			-CLICK(1), -STEPUP_HEIGHT,		// Edge height bounds.
			LARA_HEIGHT_CRAWL, -MAX_HEIGHT, // Ledge floor-to-ceil range.
			-(int)CLICK(0.6f),				// Edge-to-ceil height lower bound.
			false,							// Find highest.
			false,							// Test swamp depth.
			true,							// Test edge front.
			true,							// Test ledge heights.
			true							// Test ledge illegal slope.
		};
		constexpr auto VERTICAL_OFFSET = CLICK(1);

		// Extended crawl moveset disabled; return nullopt.
		if (!g_GameFlow->HasCrawlExtended())
			return std::nullopt;

		// Get tread water vault climb context.
		auto attracColl = GetEdgeVaultClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_TREAD_WATER_VAULT_1_STEP_UP_TO_CROUCH;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	std::optional<ClimbContextData> GetTreadWaterVaultClimbContext(ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS = BLOCK(0.5f);

		const auto& player = GetLaraInfo(item);

		// Check hand status.
		if (player.Control.HandStatus != HandStatus::Free)
			return std::nullopt;

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(item, ATTRAC_DETECT_RADIUS);

		auto context = std::optional<ClimbContextData>();

		// 1) Water tread vault 1 step down to stand.
		context = GetTreadWaterVault1StepDownToStandClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 2) Water tread vault 1 step down to crouch.
		context = GetTreadWaterVault1StepDownToCrouchClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 3) Water tread vault 0 steps to stand.
		context = GetTreadWaterVault0StepsToStandClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 4) Water tread vault 0 steps to crouch.
		context = GetTreadWaterVault0StepsToCrouchClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 5) Water tread vault up 1 step up to stand.
		context = GetTreadWaterVault1StepUpToStandClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}

		// 6) Water tread vault up 1 step up to crouch.
		context = GetTreadWaterVault1StepUpToCrouchClimbContext(item, coll, attracColls);
		if (context.has_value())
		{
			if (HasStateDispatch(&item, context->TargetStateID))
				return context;
		}
		
		// No valid edge attractor collision; return nullopt.
		return std::nullopt;
	}

	std::optional<WaterTreadStepOutContextData> GetTreadWaterStepOutContext(const ItemInfo& item)
	{
		auto& player = GetLaraInfo(item);

		// Get point collision.
		auto pointColl = GetCollision(item);
		int vPos = item.Pose.Position.y;

		// Assess water height.
		if ((pointColl.Position.Floor - item.Pose.Position.y) >= SWIM_WATER_DEPTH)
			return std::nullopt;

		if ((pointColl.Position.Floor - vPos) <= 0)
			return std::nullopt;

		if ((pointColl.Position.Floor - vPos) >= CLICK(1))
		{
			return WaterTreadStepOutContextData{ pointColl.Position.Floor, LA_STAND_IDLE };
		}
		else
		{
			return WaterTreadStepOutContextData{ pointColl.Position.Floor, LA_ONWATER_TO_WADE_1_STEP };
		}
	}

	static std::optional<AttractorCollisionData> GetEdgeHangClimbAttractorCollision(const ItemInfo& item, const CollisionInfo& coll,
																					const EdgeHangClimbSetupData& setup,
																					const std::vector<AttractorCollisionData>& attracColls)
	{
		constexpr auto ABS_EDGE_BOUND = CLICK(0.5f);

		float range2D = OFFSET_RADIUS(coll.Setup.Radius);

		// Assess attractor collision.
		for (const auto& attracColl : attracColls)
		{
			// 1) Check if attractor is edge or wall edge type.
			if (attracColl.AttractorPtr->GetType() != AttractorType::Edge &&
				attracColl.AttractorPtr->GetType() != AttractorType::WallEdge)
			{
				continue;
			}

			// 2) Test if edge is within 2D range.
			if (attracColl.Proximity.Distance2D > range2D)
				continue;

			// 3) Test if edge slope is illegal.
			if (abs(attracColl.SlopeAngle) >= ILLEGAL_FLOOR_SLOPE_ANGLE)
				continue;

			// 4) Test relation to edge intersection.
			if (!attracColl.IsInFront ||
				!TestPlayerInteractAngle(item.Pose.Orientation.y + setup.RelHeadingAngle, attracColl.HeadingAngle + ANGLE(180.0f)))
			{
				continue;
			}

			// Get point collision behind edge.
			auto pointCollBack = GetCollision(
				attracColl.Proximity.Intersection, attracColl.AttractorPtr->GetRoomNumber(),
				attracColl.HeadingAngle, -coll.Setup.Radius);

			// TODO: Add to other functions.
			// 5) Test if player vertical position is adequately close to edge.
			if (abs(attracColl.Proximity.Intersection.y - item.Pose.Position.y) > ABS_EDGE_BOUND)
				continue;

			// 6) Test if relative edge height is within edge intersection bounds.
			int relEdgeHeight = pointCollBack.Position.Floor - item.Pose.Position.y;
			if (relEdgeHeight >= setup.LowerEdgeBound || // Floor-to-edge height is within lower edge bound.
				relEdgeHeight < setup.UpperEdgeBound)	 // Floor-to-edge height is within upper edge bound.
			{
				continue;
			}

			// 7) Test if ceiling behind is adequately higher than edge.
			int edgeToCeilHeight = pointCollBack.Position.Ceiling - attracColl.Proximity.Intersection.y;
			if (edgeToCeilHeight > setup.LowerEdgeToCeilBound)
				continue;

			return attracColl;
		}

		// No valid edge attractor collision; return nullopt.
		return std::nullopt;
	}

	std::optional<ClimbContextData> GetStandToHangFrontClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS = BLOCK(0.5f);
		constexpr auto SETUP = EdgeHangClimbSetupData
		{
			ANGLE(0.0f),					  // Relative heading angle.
			-MAX_HEIGHT, LARA_HEIGHT_STRETCH, // Edge height bounds.
			-CLICK(1)						  // Edge-to-ceil height lower bound.
		};

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(
			item.Pose.Position.ToVector3(), item.RoomNumber, item.Pose.Orientation.y + SETUP.RelHeadingAngle,
			ATTRAC_DETECT_RADIUS);

		// Get standing front edge hang climb context.
		auto attracColl = GetEdgeHangClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, 0.0f, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles(0, ANGLE(180.0f), 0);
			context.TargetStateID = LS_STAND_EDGE_DESCENT_FRONT;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	std::optional<ClimbContextData> GetStandToHangBackClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS = BLOCK(0.5f);
		constexpr auto SETUP = EdgeHangClimbSetupData
		{
			ANGLE(180.0f),					  // Relative heading angle.
			-MAX_HEIGHT, LARA_HEIGHT_STRETCH, // Edge height bounds.
			-CLICK(1)						  // Edge-to-ceil height lower bound.
		};

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(
			item.Pose.Position.ToVector3(), item.RoomNumber, item.Pose.Orientation.y + SETUP.RelHeadingAngle,
			ATTRAC_DETECT_RADIUS);

		// Get standing back edge hang climb context.
		auto attracColl = GetEdgeHangClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, 0.0f, coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = IsHeld(In::Sprint) ? LS_STAND_EDGE_DESCENT_BACK_FLIP : LS_STAND_EDGE_DESCENT_BACK;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	// TODO: Needs animation.
	std::optional<ClimbContextData> GetCrawlToHangFrontClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS = BLOCK(0.5f);
		constexpr auto SETUP = EdgeHangClimbSetupData
		{
			ANGLE(0.0f),					  // Relative heading angle.
			-MAX_HEIGHT, LARA_HEIGHT_STRETCH, // Edge height bounds.
			-(int)CLICK(0.6f)				  // Edge-to-ceil height lower bound.
		};

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(
			item.Pose.Position.ToVector3(), item.RoomNumber, item.Pose.Orientation.y + SETUP.RelHeadingAngle,
			ATTRAC_DETECT_RADIUS);

		// Get standing front edge hang climb context.
		auto attracColl = GetEdgeHangClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, 0.0f, coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_STAND_EDGE_DESCENT_FRONT;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	std::optional<ClimbContextData> GetCrawlToHangBackClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS = BLOCK(0.5f);
		constexpr auto SETUP = EdgeHangClimbSetupData
		{
			ANGLE(180.0f),					  // Relative heading angle.
			-MAX_HEIGHT, LARA_HEIGHT_STRETCH, // Edge height bounds.
			-(int)CLICK(0.6f)				  // Edge-to-ceil height lower bound.
		};

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(
			item.Pose.Position.ToVector3(), item.RoomNumber, item.Pose.Orientation.y + SETUP.RelHeadingAngle,
			ATTRAC_DETECT_RADIUS);

		// Get crawl back edge hang climb context.
		auto attracColl = GetEdgeHangClimbAttractorCollision(item, coll, SETUP, attracColls);
		if (attracColl.has_value())
		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, 0.0f, coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_CRAWL_TO_HANG;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	static std::optional<AttractorCollisionData> GetEdgeCatchAttractorCollision(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ATTRAC_DETECT_RADIUS		  = BLOCK(0.5f);
		constexpr auto POINT_COLL_VERTICAL_OFFSET = -CLICK(1);
		constexpr auto FLOOR_TO_EDGE_HEIGHT_MIN	  = LARA_HEIGHT_STRETCH;

		// Get attractor collisions.
		auto attracColls = GetAttractorCollisions(item, 0.0f, -coll.Setup.Height, 0.0f, ATTRAC_DETECT_RADIUS);

		float range2D = OFFSET_RADIUS(std::max((float)coll.Setup.Radius, item.Animation.Velocity.Length()));

		// Assess attractor collision.
		for (const auto& attracColl : attracColls)
		{
			// 1) Check if attractor is edge or wall edge type.
			if (attracColl.AttractorPtr->GetType() != AttractorType::Edge &&
				attracColl.AttractorPtr->GetType() != AttractorType::WallEdge)
			{
				continue;
			}

			// 2) Test if edge is within 2D range.
			if (attracColl.Proximity.Distance2D > range2D)
				continue;

			// 3) Test if edge slope is illegal.
			if (abs(attracColl.SlopeAngle) >= ILLEGAL_FLOOR_SLOPE_ANGLE)
				continue;

			// 4) Test relation to edge intersection.
			if (!attracColl.IsInFront || !TestPlayerInteractAngle(item.Pose.Orientation.y, attracColl.HeadingAngle))
				continue;

			// Get point collision in front of edge.
			auto pointColl = GetCollision(
				attracColl.Proximity.Intersection, attracColl.AttractorPtr->GetRoomNumber(),
				attracColl.HeadingAngle, -coll.Setup.Radius, 0.0f, POINT_COLL_VERTICAL_OFFSET);

			// 5) Test if edge is high enough from floor.
			int floorToEdgeHeight = pointColl.Position.Floor - attracColl.Proximity.Intersection.y;
			if (floorToEdgeHeight <= FLOOR_TO_EDGE_HEIGHT_MIN)
				continue;

			// 6) Test if ceiling behind is adequately higher than edge.
			int edgeToCeilHeight = pointColl.Position.Ceiling - attracColl.Proximity.Intersection.y;
			if (edgeToCeilHeight >= 0)
				continue;

			// Get water heights.
			int waterHeight = GetWaterHeight(pointColl.Coordinates.x, pointColl.Coordinates.y, pointColl.Coordinates.z, pointColl.RoomNumber);
			int waterDepth = GetWaterDepth(pointColl.Coordinates.x, pointColl.Coordinates.y, pointColl.Coordinates.z, pointColl.RoomNumber);

			// 7) Test if edge is high enough from water surface (if applicable).
			if (waterHeight != NO_HEIGHT && waterDepth != NO_HEIGHT)
			{
				int waterToEdgeHeight = waterHeight - attracColl.Proximity.Intersection.y;
				if (waterToEdgeHeight <= FLOOR_TO_EDGE_HEIGHT_MIN &&
					waterDepth >= attracColl.Proximity.Intersection.y)
				{
					continue;
				}
			}

			int vPos = item.Pose.Position.y - coll.Setup.Height;
			int relEdgeHeight = attracColl.Proximity.Intersection.y - vPos;

			float projVerticalVel = item.Animation.Velocity.y + GetEffectiveGravity(item.Animation.Velocity.y);
			bool isFalling = (projVerticalVel >= 0.0f);
			int lowerBound = isFalling ? (int)ceil(projVerticalVel) : 0;
			int upperBound = isFalling ? 0 : (int)floor(projVerticalVel);

			// 8) SPECIAL CASE: Test if player is falling if attractor is wall edge.
			if (attracColl.AttractorPtr->GetType() == AttractorType::WallEdge &&
				!isFalling)
			{
				return std::nullopt;
			}

			// 9) Test catch trajectory.
			if (relEdgeHeight <= lowerBound && // Edge height is above lower height bound.
				relEdgeHeight >= upperBound)   // Edge height is below upper height bound.
			{
				return attracColl;
			}
		}

		// No valid edge attractor collision; return nullopt.
		return std::nullopt;
	}

	static std::optional<ClimbContextData> GetEdgeJumpCatchClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto VERTICAL_OFFSET = LARA_HEIGHT_STRETCH;

		// Return edge catch climb context.
		auto attracColl = GetEdgeCatchAttractorCollision(item, coll);
		if (attracColl.has_value())

		{
			auto context = ClimbContextData{};
			context.AttractorPtr = attracColl->AttractorPtr;
			context.ChainDistance = attracColl->Proximity.ChainDistance;
			context.RelPosOffset = Vector3(0.0f, VERTICAL_OFFSET, -coll.Setup.Radius);
			context.RelOrientOffset = EulerAngles::Zero;
			context.TargetStateID = LS_HANG_IDLE;
			context.AlignType = ClimbContextAlignType::AttractorParent;
			context.SetBusyHands = true;
			context.SetJumpVelocity = false;

			return context;
		}

		return std::nullopt;
	}

	static std::optional<MonkeySwingJumpCatchClimbContextData> GetMonkeySwingJumpCatchContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ABS_CEIL_BOUND			= CLICK(0.5f);
		constexpr auto FLOOR_TO_CEIL_HEIGHT_MAX = LARA_HEIGHT_MONKEY;

		const auto& player = GetLaraInfo(item);

		// 1) Check for monkey swing flag.
		if (!player.Control.CanMonkeySwing)
			return std::nullopt;

		// 2) Check collision type.
		if (coll.CollisionType != CollisionType::CT_TOP &&
			coll.CollisionType != CollisionType::CT_TOP_FRONT)
		{
			return std::nullopt;
		}

		// Get point collision.
		auto pointColl = GetCollision(&item);

		int vPos = item.Pose.Position.y - coll.Setup.Height;
		int relCeilHeight = pointColl.Position.Ceiling - vPos;
		int floorToCeilHeight = abs(pointColl.Position.Ceiling - pointColl.Position.Floor);

		// 3) Assess point collision.
		if (abs(relCeilHeight) <= ABS_CEIL_BOUND &&		  // Ceiling height is within lower/upper ceiling bounds.
			floorToCeilHeight > FLOOR_TO_CEIL_HEIGHT_MAX) // Floor-to-ceiling height is wide enough.
		{
			auto monkeyCatchContext = MonkeySwingJumpCatchClimbContextData{};
			monkeyCatchContext.CeilingHeight = pointColl.Position.Ceiling;
			monkeyCatchContext.TargetStateID = LS_MONKEY_IDLE;

			return monkeyCatchContext;
		}

		return std::nullopt;
	}

	std::optional<JumpCatchClimbContextData> GetJumpCatchClimbContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		auto edgeCatchClimbContext = GetEdgeJumpCatchClimbContext(item, coll);
		if (edgeCatchClimbContext.has_value())
			return edgeCatchClimbContext;

		auto monkeyCatchClimbContext = GetMonkeySwingJumpCatchContext(item, coll);
		if (monkeyCatchClimbContext.has_value())
			return monkeyCatchClimbContext;

		return std::nullopt;
	}
}
