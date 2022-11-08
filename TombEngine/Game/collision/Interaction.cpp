#include "framework.h"
#include "Game/collision/Interaction.h"

#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Math/Math.h"

using namespace TEN::Math;

//namespace TEN::Collision
//{
	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const EulerAngles& orientOffset, const GameBoundingBox& bounds, const std::pair<EulerAngles, EulerAngles>& orientConstraint)
	{
		this->PosOffset = posOffset;
		this->OrientOffset = orientOffset;
		this->Bounds = bounds;
		this->OrientConstraint = orientConstraint;
	}
	
	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const GameBoundingBox& bounds, const std::pair<EulerAngles, EulerAngles>& orientConstraint)
	{
		this->PosOffset = posOffset;
		this->Bounds = bounds;
		this->OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const EulerAngles& orientOffset, const GameBoundingBox& bounds, const std::pair<EulerAngles, EulerAngles>& orientConstraint)
	{
		this->OrientOffset = orientOffset;
		this->Bounds = bounds;
		this->OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const GameBoundingBox& bounds, const std::pair<EulerAngles, EulerAngles>& orientConstraint)
	{
		this->Bounds = bounds;
		this->OrientConstraint = orientConstraint;
	};

	bool TestEntityInteraction(const ItemInfo& entityFrom, const ItemInfo& entityTo, const InteractionBasis& basis, const GameBoundingBox& boundsExtension)
	{
		// Avoid overriding active interactions. NOTE: For now, can only check offset blending status.
		if (entityFrom.OffsetBlend.IsActive)
			return false;

		auto orientConstraintAverage = (basis.OrientConstraint.first + basis.OrientConstraint.second) / 2;
		auto poseFrom = Pose(entityFrom.Pose.Position, entityFrom.Pose.Orientation + orientConstraintAverage);

		// TODO: May interfere with pickups?
		// Check whether entityFrom is aligned toward entityTo.
		if (!Geometry::IsPointInFront(poseFrom, entityTo.Pose.Position.ToVector3()))
			return false;

		// Check whether entityFrom's orientation is within interaction constraint.
		auto deltaOrient = entityFrom.Pose.Orientation - entityTo.Pose.Orientation;
		if (deltaOrient.x < basis.OrientConstraint.first.x || deltaOrient.x > basis.OrientConstraint.second.x ||
			deltaOrient.y < basis.OrientConstraint.first.y || deltaOrient.y > basis.OrientConstraint.second.y ||
			deltaOrient.z < basis.OrientConstraint.first.z || deltaOrient.z > basis.OrientConstraint.second.z)
		{
			return false;
		}

		auto direction = (entityFrom.Pose.Position - entityTo.Pose.Position).ToVector3();
		auto rotMatrix = entityTo.Pose.Orientation.ToRotationMatrix().Transpose(); // NOTE: Should be Invert(), but inverse/transpose of a rotation matrix are equal and transposing is faster.
		auto relPos = Vector3::Transform(direction, rotMatrix);

		// Check whether entityFrom is inside interaction bounds.
		auto bounds = basis.Bounds + boundsExtension; // TODO: Make this static to optimise?
		if (relPos.x < bounds.X1 || relPos.x > bounds.X2 ||
			relPos.y < bounds.Y1 || relPos.y > bounds.Y2 ||
			relPos.z < bounds.Z1 || relPos.z > bounds.Z2)
		{
			return false;
		}

		return true;
	}

	void SetEntityInteraction(ItemInfo& entityFrom, const ItemInfo& entityTo, const InteractionBasis& basis, const Vector3i& extraPosOffset, const EulerAngles& extraOrientOffset)
	{
		auto relPosOffset = basis.PosOffset + extraPosOffset;
		auto relOrientOffset = basis.OrientOffset + extraOrientOffset;

		auto targetPos = Geometry::TranslatePoint(entityTo.Pose.Position, entityTo.Pose.Orientation, relPosOffset);
		auto targetOrient = entityTo.Pose.Orientation + relOrientOffset;

		auto absPosOffset = (targetPos - entityFrom.Pose.Position).ToVector3();
		auto absOrientOffset = targetOrient - entityFrom.Pose.Orientation;
		entityFrom.OffsetBlend.SetLinear(absPosOffset, absOrientOffset, 0.4f);
	}

	void SetPlayerAlignAnimation(ItemInfo& playerEntity, const ItemInfo& entity)
	{
		auto& lara = *GetLaraInfo(&playerEntity);

		// Check whether already aligning.
		if (lara.Control.IsMoving)
			return;

		// Check water status.
		if (lara.Control.WaterStatus == WaterStatus::Underwater ||
			lara.Control.WaterStatus == WaterStatus::TreadWater)
		{
			return;
		}

		float distance = Vector3i::Distance(playerEntity.Pose.Position, entity.Pose.Position);
		bool doAlignAnim = ((distance - LARA_ALIGN_VELOCITY) > (LARA_ALIGN_VELOCITY * ANIMATED_ALIGNMENT_FRAME_COUNT_THRESHOLD));

		// Skip animating if very close to the object.
		if (!doAlignAnim)
			return;

		short headingAngle = Geometry::GetOrientToPoint(playerEntity.Pose.Position.ToVector3(), entity.Pose.Position.ToVector3()).y;
		int direction = GetQuadrant(headingAngle - playerEntity.Pose.Orientation.y);

		// Set appropriate animation.
		switch (direction)
		{
		default:
		case NORTH:
			SetAnimation(&playerEntity, LA_WALK);
			break;

		case SOUTH:
			SetAnimation(&playerEntity, LA_WALK_BACK);
			break;

		case EAST:
			SetAnimation(&playerEntity, LA_SIDESTEP_RIGHT);
			break;

		case WEST:
			SetAnimation(&playerEntity, LA_SIDESTEP_LEFT);
			break;
		}

		lara.Control.HandStatus = HandStatus::Busy;
		lara.Control.IsMoving = true;
		lara.Control.Count.PositionAdjust = 0;
	}
//}
