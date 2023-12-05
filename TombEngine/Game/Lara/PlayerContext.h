#pragma once
#include "Game/Lara/PlayerContextData.h"
#include "Math/Math.h"

//----debug
#include "Game/collision/Attractor.h"
using namespace TEN::Collision::Attractor;
//----

struct CollisionInfo;
struct ItemInfo;
struct LaraInfo;

using namespace TEN::Math;

namespace TEN::Entities::Player
{
	struct EdgeCatchContextData;
	struct MonkeySwingCatchContextData;
	struct ShimmyContextData;

	struct PlayerAttractorData
	{
		Attractor*	Ptr				= nullptr;
		float		ChainDistance   = 0.0f;
		Vector3		RelPosOffset	= Vector3::Zero;
		EulerAngles RelOrientOffset = EulerAngles::Zero;

		Vector3		RelDeltaPos	   = Vector3::Zero;
		EulerAngles RelDeltaOrient = EulerAngles::Zero;

		void Attach(ItemInfo& playerItem, Attractor& attrac, float chainDist,
					const Vector3& relPosOffset, const EulerAngles& relOrientOffset,
					const Vector3& relDeltaPos, const EulerAngles& relDeltaOrient);
		void Attach(ItemInfo& playerItem, Attractor& attrac, float chainDist);
		void Detach(ItemInfo& playerItem);
	};

	struct DebugAttractorData
	{
		Attractor Attrac0 = {};
		Attractor Attrac1 = {};
		Attractor Attrac2 = {};
	};

	// TODO: Savegame for attractors.
	struct PlayerContext
	{
	private:
		// TODO: Make LaraInfo aware of its parent ItemInfo.
		// Parent pointers
		/*const ItemInfo*		 ItemPtr = nullptr;
		const LaraInfo*		 PlayerPtr = nullptr;
		const CollisionInfo* CollPtr = nullptr;*/

	public:
		// Members
		PlayerAttractorData Attractor	 = {};
		DebugAttractorData	DebugAttracs = {};

		int			ProjectedFloorHeight = 0; // Used for primitive offset blend. TODO: Real offset blend feature + object parenting. -- Sezz 2023.09.27
		float		CalcJumpVelocity	 = 0;
		Pose		NextCornerPos		 = Pose::Zero;
		EulerAngles OrientOffset		 = EulerAngles::Zero;

		int		 WaterSurfaceDist	= 0;
		short	 WaterCurrentActive = 0; // Sink number? Often used as bool.
		Vector3i WaterCurrentPull	= Vector3i::Zero;

		int InteractedItem = 0; // InteractedItemNumber.
		int Vehicle		   = 0; // VehicleItemNumber.

		PlayerContext() {};
		PlayerContext(const ItemInfo& item, const CollisionInfo& coll);

		// TODO: Move all functions below into this class. Resulting syntax will be a neat player.Context.CanDoXYZ().
	};

	// Basic round movement contexts
	bool CanChangeElevation(const ItemInfo& item, const CollisionInfo& coll);
	bool CanStepUp(const ItemInfo& item, const CollisionInfo& coll);
	bool CanStepDown(const ItemInfo& item, const CollisionInfo& coll);
	bool CanStrikeAfkPose(const ItemInfo& item, const CollisionInfo& coll);
	bool CanTurn180(const ItemInfo& item, const CollisionInfo& coll);
	bool CanTurnFast(const ItemInfo& item, const CollisionInfo& coll, bool isGoingRight);
	bool CanRoll180Running(const ItemInfo& item);
	bool CanRunForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanRunBackward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanWalkForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanWalkBackward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanSidestepLeft(const ItemInfo& item, const CollisionInfo& coll);
	bool CanSidestepRight(const ItemInfo& item, const CollisionInfo& coll);
	bool CanWadeForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanWadeBackward(const ItemInfo& item, const CollisionInfo& coll);

	// Basic vault contexts
	bool CanVaultFromSprint(const ItemInfo& item, const CollisionInfo& coll);

	// Slide contexts
	bool CanSlide(const ItemInfo& item, const CollisionInfo& coll);
	bool CanSteerOnSlide(const ItemInfo& item, const CollisionInfo& coll);

	// Crouch and crawl contexts
	bool IsInLowSpace(const ItemInfo& item, const CollisionInfo& coll);
	bool CanCrouch(const ItemInfo& item, const CollisionInfo& coll);
	bool CanCrouchToCrawl(const ItemInfo& item, const CollisionInfo& coll);
	bool CanCrouchRoll(const ItemInfo& item, const CollisionInfo& coll);
	bool CanCrawlForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanCrawlBackward(const ItemInfo& item, const CollisionInfo& coll);

	// Monkey swing contexts
	bool CanPerformMonkeySwingStep(const ItemInfo& item, const CollisionInfo& coll);
	bool CanFallFromMonkeySwing(const ItemInfo& item, const CollisionInfo& coll);
	bool CanGrabMonkeySwing(const ItemInfo& item, const CollisionInfo& coll);
	bool CanMonkeySwingForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanMonkeySwingBackward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanMonkeySwingShimmyLeft(const ItemInfo& item, const CollisionInfo& coll);
	bool CanMonkeySwingShimmyRight(const ItemInfo& item, const CollisionInfo& coll);

	// Jump contexts
	bool CanFall(const ItemInfo& item, const CollisionInfo& coll);
	bool CanLand(const ItemInfo& item, const CollisionInfo& coll);
	bool CanPerformJump(const ItemInfo& item, const CollisionInfo& coll);
	bool CanJumpUp(const ItemInfo& item, const CollisionInfo& coll);
	bool CanJumpForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanJumpBackward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanJumpLeft(const ItemInfo& item, const CollisionInfo& coll);
	bool CanJumpRight(const ItemInfo& item, const CollisionInfo& coll);
	bool CanQueueRunningJump(const ItemInfo& item, const CollisionInfo& coll);
	bool CanRunJumpForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanSprintJumpForward(const ItemInfo& item, const CollisionInfo& coll);
	bool CanPerformSlideJump(const ItemInfo& item, const CollisionInfo& coll);
	bool CanCrawlspaceDive(const ItemInfo& item, const CollisionInfo& coll);

	// Edge contexts
	bool CanSwingOnLedge(const ItemInfo& item, const CollisionInfo& coll);
	bool CanPerformLedgeJump(const ItemInfo& item, const CollisionInfo& coll);
	bool CanPerformLedgeHandstand(const ItemInfo& item, CollisionInfo& coll);
	bool CanClimbLedgeToCrouch(const ItemInfo& item, CollisionInfo& coll);
	bool CanClimbLedgeToStand(const ItemInfo& item, CollisionInfo& coll);
	bool CanShimmyUp(const ItemInfo& item, const CollisionInfo& coll);
	bool CanShimmyDown(const ItemInfo& item, const CollisionInfo& coll);
	bool CanShimmyLeft(ItemInfo& item, CollisionInfo& coll);
	bool CanShimmyRight(ItemInfo& item, CollisionInfo& coll);

	// Object interaction contexts
	bool CanDismountTightrope(const ItemInfo& item, const CollisionInfo& coll);

	// Context data getters
	std::optional<ClimbContextData>			   GetStandClimbContext(const ItemInfo& item, const CollisionInfo& coll);
	std::optional<ClimbContextData>			   GetCrawlClimbContext(const ItemInfo& item, const CollisionInfo& coll);
	std::optional<ClimbContextData>			   GetCrawlToHangClimbContext(ItemInfo& item, const CollisionInfo& coll);
	std::optional<ClimbContextData>			   GetWaterTreadClimbContext(ItemInfo& item, const CollisionInfo& coll);
	std::optional<EdgeCatchContextData>		   GetEdgeCatchContext(ItemInfo& item, CollisionInfo& coll);
	std::optional<MonkeySwingCatchContextData> GetMonkeySwingCatchContext(const ItemInfo& item, const CollisionInfo& coll);
}
