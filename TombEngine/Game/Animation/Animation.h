#pragma once

#include "Game/Animation/Commands.h"
#include "Math/Math.h"

using namespace TEN::Math;

enum GAME_OBJECT_ID : short;
class EulerAngles;
class Pose;
class Vector3i;
struct CreatureBiteInfo;
struct ItemInfo;
struct ObjectInfo;

namespace TEN::Animation
{
	enum class AnimFlags
	{
		None = 0,

		RootMotionTranslationX = 1 << 0,
		RootMotionTranslationY = 1 << 1,
		RootMotionTranslationZ = 1 << 2,
		RootMotionRotationX	   = 1 << 3,
		RootMotionRotationY	   = 1 << 4,
		RootMotionRotationZ	   = 1 << 5,
		RootMotionCycle		   = 1 << 6
	};

	struct KeyframeData
	{
		Vector3					RootOffset		 = Vector3::Zero;
		std::vector<Quaternion> BoneOrientations = {};
		BoundingBox				Aabb			 = DirectX::BoundingBox();

		// Deprecated.
		GameBoundingBox BoundingBox = GameBoundingBox::Zero;
	};

	struct StateDispatchData
	{
		int StateID			= 0;
		int FrameNumberLow	= 0;
		int FrameNumberHigh = 0;

		int			  NextAnimNumber	  = 0;
		int			  NextFrameNumberLow  = 0;
		int			  NextFrameNumberHigh = 0;
		int			  BlendFrameCount	  = 0;
		BezierCurve2D BlendCurve		  = {};
	};

	struct FrameInterpData
	{
		const KeyframeData& Keyframe0;
		const KeyframeData& Keyframe1;
		float Alpha = 0.0f;

		FrameInterpData(const KeyframeData& keyframe0, const KeyframeData& keyframe1, float alpha);
	};

	struct RootMotionData
	{
		Vector3		Translation = Vector3::Zero;
		EulerAngles Rotation	= EulerAngles::Identity;
	};

	struct AnimData
	{
		using AnimCommandPtr = std::unique_ptr<AnimCommand>;
		
		int			  StateID		  = 0;
		int			  Interpolation	  = 0;
		int			  EndFrameNumber  = 0;
		int			  NextAnimNumber  = 0;
		int			  NextFrameNumber = 0;
		int			  BlendFrameCount = 0;
		BezierCurve2D BlendCurve	  = {};

		Vector3 VelocityStart = Vector3::Zero; // CONVENTION: +X = Right, +Y = Down, +Z = Forward.
		Vector3 VelocityEnd	  = Vector3::Zero; // CONVENTION: +X = Right, +Y = Down, +Z = Forward.

		std::vector<KeyframeData>	   Keyframes  = {};
		std::vector<StateDispatchData> Dispatches = {};
		std::vector<AnimCommandPtr>	   Commands	  = {};

		int Flags = (int)AnimFlags::None;

		FrameInterpData		GetFrameInterpolation(int frameNumber) const;
		const KeyframeData& GetClosestKeyframe(int frameNumber) const;
		RootMotionData		GetRootMotion(int frameNumber) const;
		RootMotionData		GetRootMotionCounteraction(int frameNumber) const;
	};

	struct BoneMutator
	{
		Vector3		Offset	 = Vector3::Zero;
		EulerAngles Rotation = EulerAngles::Identity;
		Vector3		Scale	 = Vector3::One;

		bool IsEmpty() const;
	};

	// Animation controller

	void AnimateItem(ItemInfo& item);

	// Inquirers

	bool TestStateDispatch(const ItemInfo& item, int targetStateID = NO_VALUE);
	bool TestLastFrame(const ItemInfo& item, int animNumber = NO_VALUE);
	bool TestAnimFrameRange(const ItemInfo& item, int frameNumberLow, int frameNumberHigh);

	// Getters

	const AnimData& GetAnimData(const ObjectInfo& object, int animNumber);
	const AnimData& GetAnimData(GAME_OBJECT_ID objectID, int animNumber);
	const AnimData& GetAnimData(const ItemInfo& item, int animNumber = NO_VALUE);

	FrameInterpData	GetFrameInterpolation(const ItemInfo& item);
	const KeyframeData& GetKeyframe(GAME_OBJECT_ID objectID, int animNumber, int frameNumber = 0);
	const KeyframeData& GetKeyframe(const ItemInfo& item, int animNumber, int frameNumber = 0);
	const KeyframeData& GetFirstKeyframe(GAME_OBJECT_ID objectID, int animNumber);
	const KeyframeData& GetLastKeyframe(GAME_OBJECT_ID objectID, int animNumber);
	const KeyframeData& GetClosestKeyframe(const ItemInfo& item);

	const StateDispatchData* GetStateDispatch(const ItemInfo& item, int targetStateID = NO_VALUE);

	int	  GetNextAnimState(const ItemInfo& item);
	int	  GetNextAnimState(GAME_OBJECT_ID objectID, int animNumber);
	int	  GetFrameCount(GAME_OBJECT_ID objectID, int animNumber); // TODO: Not needed? Not the "real" frame count anyway since 0 isn't counted.
	int	  GetFrameCount(const ItemInfo& item);
	float GetEffectiveGravity(float verticalVel);

	Vector3i   GetJointPosition(const ItemInfo& item, int boneID, const Vector3i& relOffset = Vector3i::Zero);
	Vector3i   GetJointPosition(ItemInfo* item, int boneID, const Vector3i& relOffset = Vector3i::Zero);
	Vector3i   GetJointPosition(ItemInfo* item, const CreatureBiteInfo& bite);
	Vector3i   GetJointPosition(const ItemInfo& item, const CreatureBiteInfo& bite);
	Vector3	   GetJointOffset(GAME_OBJECT_ID objectID, int boneID);
	Quaternion GetBoneOrientation(const ItemInfo& item, int boneID);

	// Setters

	void SetAnimation(ItemInfo& item, GAME_OBJECT_ID animObjectID, int animNumber, int frameNumber = 0, int blendFrameCount = 0, const BezierCurve2D& blendCurve = BezierCurve2D::Linear);
	void SetAnimation(ItemInfo& item, int animNumber, int frameNumber = 0, int blendFrameCount = 0, const BezierCurve2D& blendCurve = BezierCurve2D::Linear);
	void SetStateDispatch(ItemInfo& item, const StateDispatchData& dispatch);

	// Utilities

	void ClampRotation(Pose& outPose, short angle, short rot);
	void DrawAnimatingItem(ItemInfo* item);
}
