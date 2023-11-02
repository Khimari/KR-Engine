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
									   const GameBoundingBox& bounds, const OrientConstraintPair& orientConstraint)
	{
		PosOffset = posOffset;
		OrientOffset = orientOffset;
		Bounds = bounds;
		OrientConstraint = orientConstraint;
	}
	
	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const GameBoundingBox& bounds, const OrientConstraintPair& orientConstraint)
	{
		PosOffset = posOffset;
		Bounds = bounds;
		OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const EulerAngles& orientOffset, const GameBoundingBox& bounds, const OrientConstraintPair& orientConstraint)
	{
		OrientOffset = orientOffset;
		Bounds = bounds;
		OrientConstraint = orientConstraint;
	}

	InteractionBasis::InteractionBasis(const GameBoundingBox& bounds, const OrientConstraintPair& orientConstraint)
	{
		Bounds = bounds;
		OrientConstraint = orientConstraint;
	};

	bool InteractionBasis::TestInteraction(const ItemInfo& entityFrom, const ItemInfo& entityTo,  const GameBoundingBox& boundsExtension) const
	{
		DrawDebug(entityTo);

		// 1) Avoid overriding active player interactions.
		if (entityFrom.IsLara())
		{
			const auto& player = GetLaraInfo(entityFrom);
			if (player.Context.InteractedItem != NO_ITEM)
				return false;
		}

		// 2) Avoid overriding active offset blend.
		if (entityFrom.OffsetBlend.IsActive)
			return false;

		// 3) Test if entityFrom's orientation is within interaction constraint.
		auto deltaOrient = entityFrom.Pose.Orientation - entityTo.Pose.Orientation;
		if (deltaOrient.x < OrientConstraint.first.x || deltaOrient.x > OrientConstraint.second.x ||
			deltaOrient.y < OrientConstraint.first.y || deltaOrient.y > OrientConstraint.second.y ||
			deltaOrient.z < OrientConstraint.first.z || deltaOrient.z > OrientConstraint.second.z)
		{
			return false;
		}

		auto deltaPos = (entityFrom.Pose.Position - entityTo.Pose.Position).ToVector3();
		auto invRotMatrix = entityTo.Pose.Orientation.ToRotationMatrix().Transpose(); // NOTE: Transpose() used as faster equivalent to Invert().
		auto relPos = Vector3::Transform(deltaPos, invRotMatrix);

		// 4) Test if entityFrom is inside interaction bounds.
		auto bounds = Bounds + boundsExtension;
		if (relPos.x < bounds.X1 || relPos.x > bounds.X2 ||
			relPos.y < bounds.Y1 || relPos.y > bounds.Y2 ||
			relPos.z < bounds.Z1 || relPos.z > bounds.Z2)
		{
			return false;
		}

		return true;
	}

	void InteractionBasis::DrawDebug(const ItemInfo& item) const
	{
		constexpr auto COLL_BOX_COLOR	  = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
		constexpr auto INTERACT_BOX_COLOR = Vector4(0.0f, 1.0f, 1.0f, 1.0f);

		// Draw collision box.
		auto bounds = GameBoundingBox(&item);
		auto box = bounds.ToBoundingOrientedBox(item.Pose);
		g_Renderer.AddDebugBox(box, COLL_BOX_COLOR, RendererDebugPage::None);

		// Draw interaction box.
		auto interactBox = Bounds.ToBoundingOrientedBox(item.Pose);
		g_Renderer.AddDebugBox(interactBox, INTERACT_BOX_COLOR, RendererDebugPage::None);
	}

	void SetEntityInteraction(ItemInfo& entityFrom, const ItemInfo& entityTo, const InteractionBasis& basis,
							  const Vector3i& extraPosOffset, const EulerAngles& extraOrientOffset)
	{
		constexpr auto OFFSET_BLEND_ALPHA = 0.3f;

		// Calculate relative offsets.
		auto relPosOffset = basis.PosOffset + extraPosOffset;
		auto relOrientOffset = basis.OrientOffset + extraOrientOffset;

		// Calculate targets.
		auto targetPos = Geometry::TranslatePoint(entityTo.Pose.Position, entityTo.Pose.Orientation, relPosOffset);
		auto targetOrient = entityTo.Pose.Orientation + relOrientOffset;

		// Calculate absolute offsets.
		auto absPosOffset = (targetPos - entityFrom.Pose.Position).ToVector3();
		auto absOrientOffset = targetOrient - entityFrom.Pose.Orientation;

		// Set offset blend.
		entityFrom.OffsetBlend.SetLogarithmic(absPosOffset, absOrientOffset, OFFSET_BLEND_ALPHA);

		// Set player parameters.
		if (entityFrom.IsLara())
		{
			auto& player = GetLaraInfo(entityFrom);

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

	void HandlePlayerInteraction(ItemInfo& playerEntity, ItemInfo& interactedEntity, const InteractionBasis& basis,
								 const PlayerInteractRoutine& interactRoutine)
	{
		auto& player = GetLaraInfo(playerEntity);
		
		// Shift.
		if (true)
		{
			if (basis.TestInteraction(playerEntity, interactedEntity))
			{
				// TODO: Currently unreliable because IteractedItem is frequently not reset.
				// Avoid overriding active interactions.
				/*if (player.Context.InteractedItem != NO_ITEM)
					return;*/

				SetEntityInteraction(playerEntity, interactedEntity, basis);
				interactRoutine(playerEntity, interactedEntity);
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
		}
	}
}
