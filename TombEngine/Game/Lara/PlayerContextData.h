#pragma once
#include "Math/Math.h"

namespace TEN::Collision::Attractor { class Attractor; }

using namespace TEN::Collision::Attractor;
using namespace TEN::Math;

namespace TEN::Entities::Player
{
	enum class ClimbContextAlignType
	{
		None,
		Snap,
		OffsetBlend,
		AttractorParent
	};

	struct GroundMovementSetupData
	{
		short HeadingAngle	  = 0;
		int	  LowerFloorBound = 0;
		int	  UpperFloorBound = 0;

		bool TestSlipperySlopeBelow = true;
		bool TestSlipperySlopeAbove = true;
		bool TestDeathFloor			= true;
	};

	struct MonkeySwingMovementSetupData
	{
		short HeadingAngle		= 0;
		int	  LowerCeilingBound = 0.0f;
		int	  UpperCeilingBound = 0.0f;
	};

	struct JumpSetupData
	{
		short HeadingAngle = 0;
		float Distance	   = 0.0f;

		bool TestWadeStatus = true;
	};

	struct EdgeVaultClimbSetupData
	{
		int LowerEdgeBound			 = 0;
		int UpperEdgeBound			 = 0;
		int LowerEdgeToCeilBound	 = 0;
		int DestFloorToCeilHeightMin = 0;
		int DestFloorToCeilHeightMax = 0;

		bool FindHighest		  = false;
		bool TestSwampDepth		  = false;
		bool TestEdgeFront		  = false;
		bool TestDestSpace		  = false;
		bool TestDestIllegalSlope = false;
	};

	struct EdgeHangDescentClimbSetupData
	{
		short RelHeadingAngle = 0;

		int LowerEdgeBound		 = 0;
		int UpperEdgeBound		 = 0;
		int LowerEdgeToCeilBound = 0;
	};

	struct LedgeClimbSetupData
	{
		int LowerEdgeToCeilBound	 = 0;
		int DestFloorToCeilHeightMin = 0;
		int DestFloorToCeilHeightMax = 0;

		bool TestIllegalSlope = false;
	};

	struct ClimbContextData
	{
		Attractor*	AttractorPtr	= nullptr;
		float		ChainDistance	= 0.0f;
		Vector3		RelPosOffset	= Vector3::Zero;
		EulerAngles RelOrientOffset = EulerAngles::Zero;
		int			TargetStateID	= 0;

		ClimbContextAlignType AlignType = ClimbContextAlignType::None;

		// TODO: These two can be unified into one as they coincide.
		//bool IsJump = false;
		bool SetBusyHands	 = false;
		bool SetJumpVelocity = false;
	};

	// TODO: Can probably use ClimbContextData, but keeping separate to make it easier to work with for now.
	struct EdgeHangContextData
	{
		Attractor* AttractorPtr	 = nullptr;
		float	   ChainDistance = 0.0f;
		int		   TargetStateID = 0;

		ClimbContextAlignType AlignType = ClimbContextAlignType::None;
	};

	struct WaterTreadStepOutContextData
	{
		int FloorHeight = 0;
		int AnimNumber	= 0;
	};
}
