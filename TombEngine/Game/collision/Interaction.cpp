#include "framework.h"
#include "Game/collision/Interaction.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Renderer/Renderer11.h"

using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Collision
{
	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const EulerAngles& orientOffset,
									   const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint)
	{
		PosOffset = posOffset;
		OrientOffset = orientOffset;
		Box = box;
		OrientConstraint = orientConstraint;
	}
	
	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint)
	{
		PosOffset = posOffset;
		Box = box;
		OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const EulerAngles& orientOffset, const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint)
	{
		OrientOffset = orientOffset;
		Box = box;
		OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint)
	{
		Box = box;
		OrientConstraint = orientConstraint;
	};

	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const EulerAngles& orientOffset,
									   const GameBoundingBox& box, const OrientConstraintPair& orientConstraint)
	{
		PosOffset = posOffset;
		OrientOffset = orientOffset;
		Box = box.ToBoundingOrientedBox(Pose::Zero);
		OrientConstraint = orientConstraint;
	}
	
	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const GameBoundingBox& box, const OrientConstraintPair& orientConstraint)
	{
		PosOffset = posOffset;
		Box = box.ToBoundingOrientedBox(Pose::Zero);
		OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const EulerAngles& orientOffset, const GameBoundingBox& box, const OrientConstraintPair& orientConstraint)
	{
		OrientOffset = orientOffset;
		Box = box.ToBoundingOrientedBox(Pose::Zero);
		OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const GameBoundingBox& box, const OrientConstraintPair& orientConstraint)
	{
		Box = box.ToBoundingOrientedBox(Pose::Zero);
		OrientConstraint = orientConstraint;
	};

	bool InteractionBasis::TestInteraction(const ItemInfo& entityFrom, const ItemInfo& entityTo, std::optional<BoundingOrientedBox> expansionBox) const
	{
		DrawDebug(entityTo);

		// 1) Avoid overriding active offset blend.
		if (entityFrom.OffsetBlend.IsActive)
			return false;

		// TODO: Currently unreliable because IteractedItem is frequently not reset after completed interactions.
		// 2) Avoid overriding active player interactions.
		if (entityFrom.IsLara())
		{
			const auto& player = GetLaraInfo(entityFrom);
			if (player.Context.InteractedItem != NO_ITEM)
				return false;
		}

		// 3) Test if entityFrom's orientation is within interaction constraint.
		auto deltaOrient = entityFrom.Pose.Orientation - entityTo.Pose.Orientation;
		if (deltaOrient.x < OrientConstraint.first.x || deltaOrient.x > OrientConstraint.second.x ||
			deltaOrient.y < OrientConstraint.first.y || deltaOrient.y > OrientConstraint.second.y ||
			deltaOrient.z < OrientConstraint.first.z || deltaOrient.z > OrientConstraint.second.z)
		{
			return false;
		}

		// Calculate box-relative position.
		auto deltaPos = (entityFrom.Pose.Position - entityTo.Pose.Position).ToVector3();
		auto invRotMatrix = entityTo.Pose.Orientation.ToRotationMatrix().Invert();
		auto relPos = Vector3::Transform(deltaPos, invRotMatrix);

		// Calculate box min and max.
		auto box = expansionBox.has_value() ? Geometry::GetExpandedBoundingOrientedBox(Box, *expansionBox) : Box;
		auto max = box.Center + box.Extents;
		auto min = box.Center - box.Extents;

		// 4) Test if entityFrom is inside interaction box.
		if (relPos.x < min.x || relPos.x > max.x ||
			relPos.y < min.y || relPos.y > max.y ||
			relPos.z < min.z || relPos.z > max.z)
		{
			return false;
		}

		// TODO: Not working.
		/*box = expansionBox.has_value() ? Geometry::GetExpandedBoundingOrientedBox(Box, *expansionBox) : Box;
		box.Center = Vector3::Transform(box.Center, box.Orientation) + entityTo.Pose.Position.ToVector3();

		// 4) Test if entityFrom's position intersects interaction box.
		if (!Geometry::IsPointInBox(deltaPos, box))
			return false;*/

		return true;
	}

	void InteractionBasis::DrawDebug(const ItemInfo& entity) const
	{
		constexpr auto COLL_BOX_COLOR	  = Color(1.0f, 0.0f, 0.0f, 1.0f);
		constexpr auto INTERACT_BOX_COLOR = Color(0.0f, 1.0f, 1.0f, 1.0f);

		// Draw collision box.
		auto collBox = GameBoundingBox(&entity).ToBoundingOrientedBox(entity.Pose);
		g_Renderer.AddDebugBox(collBox, COLL_BOX_COLOR);

		auto rotMatrix = entity.Pose.Orientation.ToRotationMatrix();
		auto relCenter = Vector3::Transform(Box.Center, rotMatrix);
		auto orient = entity.Pose.Orientation.ToQuaternion();

		// Draw interaction box.
		auto interactBox = BoundingOrientedBox(entity.Pose.Position.ToVector3() + relCenter, Box.Extents, orient);
		g_Renderer.AddDebugBox(interactBox, INTERACT_BOX_COLOR);
	}

	void SetEntityInteraction(ItemInfo& entityFrom, const ItemInfo& entityTo, const InteractionBasis& basis,
							  const Vector3i& extraPosOffset, const EulerAngles& extraOrientOffset)
	{
		constexpr auto OFFSET_BLEND_LOG_ALPHA = 0.25f;

		// Calculate relative offsets.
		auto relPosOffset = basis.PosOffset + extraPosOffset;
		auto relOrientOffset = basis.OrientOffset + extraOrientOffset;

		// Calculate targets.
		auto targetPos = Geometry::TranslatePoint(entityTo.Pose.Position, entityTo.Pose.Orientation, relPosOffset);
		auto targetOrient = entityTo.Pose.Orientation + relOrientOffset;

		// Calculate absolute offsets.
		auto absPosOffset = (targetPos - entityFrom.Pose.Position).ToVector3();
		auto absOrientOffset = targetOrient - entityFrom.Pose.Orientation;

		// Set entity parameters.
		entityFrom.Animation.Velocity = Vector3::Zero;
		entityFrom.OffsetBlend.SetLogarithmic(absPosOffset, absOrientOffset, OFFSET_BLEND_LOG_ALPHA);

		// Set player parameters.
		if (entityFrom.IsLara())
		{
			auto& player = GetLaraInfo(entityFrom);

			player.Control.TurnRate = 0;
			player.Control.HandStatus = HandStatus::Busy;
			player.Context.InteractedItem = entityTo.Index;
		}
	}

	static int GetPlayerAlignAnim(const Pose& poseFrom, const Pose& poseTo)
	{
		short headingAngle = Geometry::GetOrientToPoint(poseFrom.Position.ToVector3(), poseTo.Position.ToVector3()).y;
		int cardinalDir = GetQuadrant(headingAngle - poseFrom.Orientation.y);

		// Return alignment anim number.
		switch (cardinalDir)
		{
		default:
		case NORTH:
			return LA_WALK;

		case SOUTH:
			return LA_WALK_BACK;

		case EAST:
			return LA_SIDESTEP_RIGHT;

		case WEST:
			return LA_SIDESTEP_LEFT;
		}
	}

	void SetPlayerAlignAnim(ItemInfo& playerEntity, const ItemInfo& interactedEntity)
	{
		constexpr auto ANIMATED_ALIGNMENT_FRAME_COUNT_THRESHOLD = 6;

		auto& player = GetLaraInfo(playerEntity);

		// Check if already aligning.
		if (player.Control.IsMoving)
			return;

		// Check water status.
		if (player.Control.WaterStatus == WaterStatus::Underwater ||
			player.Control.WaterStatus == WaterStatus::TreadWater)
		{
			return;
		}

		float dist = Vector3i::Distance(playerEntity.Pose.Position, interactedEntity.Pose.Position);
		bool doAlignAnim = ((dist - LARA_ALIGN_VELOCITY) > (LARA_ALIGN_VELOCITY * ANIMATED_ALIGNMENT_FRAME_COUNT_THRESHOLD));

		// Skip animating if very close to object.
		if (!doAlignAnim)
			return;

		SetAnimation(&playerEntity, GetPlayerAlignAnim(playerEntity.Pose, interactedEntity.Pose));
		player.Control.HandStatus = HandStatus::Busy;
		player.Control.IsMoving = true;
		player.Control.Count.PositionAdjust = 0;
	}

	// TODO: Don't return bool.
	bool HandlePlayerInteraction(ItemInfo& playerEntity, ItemInfo& interactedEntity, const InteractionBasis& basis,
								 const PlayerInteractRoutine& interactRoutine)
	{
		auto& player = GetLaraInfo(playerEntity);
		
		// Shift.
		if (true)
		{
			if (basis.TestInteraction(playerEntity, interactedEntity))
			{
				SetEntityInteraction(playerEntity, interactedEntity, basis);
				interactRoutine(playerEntity, interactedEntity);

				return true;
			}
		}
		// TODO
		// Walk over.
		else
		{
			if (basis.TestInteraction(playerEntity, interactedEntity))
			{
				if (MoveLaraPosition(basis.PosOffset, &interactedEntity, &playerEntity))
				{
					interactRoutine(playerEntity, interactedEntity);
					player.Control.IsMoving = false;
					player.Control.HandStatus = HandStatus::Busy;
				}
				else
				{
					player.Context.InteractedItem = interactedEntity.Index;
				}
			}
			else if (player.Control.IsMoving && player.Context.InteractedItem == interactedEntity.Index)
			{
				player.Control.IsMoving = false;
				player.Control.HandStatus = HandStatus::Free;
			}

			return true;
		}

		return false;
	}
}
