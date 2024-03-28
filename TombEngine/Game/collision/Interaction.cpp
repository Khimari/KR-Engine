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
#include "Renderer/Renderer.h"

using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Collision::Interaction
{
	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const EulerAngles& orientOffset, const BoundingOrientedBox& box, const OrientConstraintPair& orientConstraint)
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

	InteractionBasis::InteractionBasis(const Vector3i& posOffset, const EulerAngles& orientOffset, const GameBoundingBox& box, const OrientConstraintPair& orientConstraint)
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

	bool TestInteraction(const ItemInfo& interactor, const ItemInfo& interactable, const InteractionBasis& basis, std::optional<BoundingOrientedBox> expansionBox)
	{
		DrawDebug(interactable, basis);

		// 1) Avoid overriding active offset blend.
		if (interactor.OffsetBlend.IsActive)
			return false;

		// TODO: Currently unreliable because IteractedItemNumber is frequently not reset after completed interactions.
		// 2) Avoid overriding active player interactions.
		if (interactor.IsLara())
		{
			const auto& player = GetLaraInfo(interactor);
			if (player.Context.InteractedItem != NO_VALUE)
				return false;
		}

		// 3) Test if interactor's orientation is within interaction constraint.
		auto deltaOrient = interactor.Pose.Orientation - interactable.Pose.Orientation;
		if (deltaOrient.x < basis.OrientConstraint.first.x || deltaOrient.x > basis.OrientConstraint.second.x ||
			deltaOrient.y < basis.OrientConstraint.first.y || deltaOrient.y > basis.OrientConstraint.second.y ||
			deltaOrient.z < basis.OrientConstraint.first.z || deltaOrient.z > basis.OrientConstraint.second.z)
		{
			return false;
		}

		// Calculate box-relative position.
		auto deltaPos = (interactor.Pose.Position - interactable.Pose.Position).ToVector3();
		auto invRotMatrix = interactable.Pose.Orientation.ToRotationMatrix().Invert();
		auto relPos = Vector3::Transform(deltaPos, invRotMatrix);

		// Calculate box min and max.
		auto box = expansionBox.has_value() ? Geometry::GetExpandedBoundingOrientedBox(basis.Box, *expansionBox) : basis.Box;
		auto max = box.Center + box.Extents;
		auto min = box.Center - box.Extents;

		// 4) Test if interactor is inside interaction box.
		if (relPos.x < min.x || relPos.x > max.x ||
			relPos.y < min.y || relPos.y > max.y ||
			relPos.z < min.z || relPos.z > max.z)
		{
			return false;
		}

		// TODO: Not working.
		/*box = expansionBox.has_value() ? Geometry::GetExpandedBoundingOrientedBox(basis.Box, *expansionBox) : basis.Box;
		box.Center = Vector3::Transform(box.Center, box.Orientation) + interactible.Pose.Position.ToVector3();

		// 4) Test if interactor's position intersects interaction box.
		if (!Geometry::IsPointInBox(deltaPos, box))
			return false;*/

		return true;
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

	void SetPlayerAlignAnim(ItemInfo& playerItem, const ItemInfo& interactable)
	{
		constexpr auto ANIMATED_ALIGNMENT_FRAME_COUNT_THRESHOLD = 6;

		auto& player = GetLaraInfo(playerItem);

		// Check if already aligning.
		if (player.Control.IsMoving)
			return;

		// Check water status.
		if (player.Control.WaterStatus == WaterStatus::Underwater ||
			player.Control.WaterStatus == WaterStatus::TreadWater)
		{
			return;
		}

		float dist = Vector3i::Distance(playerItem.Pose.Position, interactable.Pose.Position);
		bool doAlignAnim = ((dist - LARA_ALIGN_VELOCITY) > (LARA_ALIGN_VELOCITY * ANIMATED_ALIGNMENT_FRAME_COUNT_THRESHOLD));

		// Skip animating if very close to object.
		if (!doAlignAnim)
			return;

		SetAnimation(&playerItem, GetPlayerAlignAnim(playerItem.Pose, interactable.Pose));
		player.Control.HandStatus = HandStatus::Busy;
		player.Control.IsMoving = true;
		player.Control.Count.PositionAdjust = 0;
	}
	
	static void SetEaseInteraction(ItemInfo& interactor, ItemInfo& interactable, const InteractionBasis& basis, const InteractionRoutine& routine)
	{
		constexpr auto OFFSET_BLEND_ALPHA = 0.25f;

		// Calculate relative offsets.
		auto relPosOffset = basis.PosOffset;// + extraPosOffset;
		auto relOrientOffset = basis.OrientOffset;// + extraOrientOffset;

		// Calculate targets.
		auto targetPos = Geometry::TranslatePoint(interactable.Pose.Position, interactable.Pose.Orientation, relPosOffset);
		auto targetOrient = interactable.Pose.Orientation + relOrientOffset;

		// Calculate absolute offsets.
		auto absPosOffset = (targetPos - interactor.Pose.Position).ToVector3();
		auto absOrientOffset = targetOrient - interactor.Pose.Orientation;

		// Set interactor parameters.
		interactor.Animation.Velocity = Vector3::Zero;
		interactor.OffsetBlend.SetLogarithmic(absPosOffset, absOrientOffset, OFFSET_BLEND_ALPHA);

		// Set player parameters.
		if (interactor.IsLara())
		{
			auto& player = GetLaraInfo(interactor);

			player.Control.TurnRate = 0;
			player.Control.HandStatus = HandStatus::Busy;
			player.Context.InteractedItem = interactable.Index;
		}

		// Call interaction routine.
		routine(interactor, interactable);
	}
	
	static void SetWalkInteraction(ItemInfo& interactor, ItemInfo& interactable, const InteractionBasis& basis, const InteractionRoutine& routine)
	{
		// Set player parameters.
		if (interactor.IsLara())
		{
			auto& player = GetLaraInfo(interactor);

			player.Control.TurnRate = 0;
			player.Control.HandStatus = HandStatus::Busy;
			player.Context.InteractedItem = interactable.Index;
			player.Context.WalkInteraction.Basis = basis;
			player.Context.WalkInteraction.Routine = routine;
		}
		else
		{
			TENLog("SetWalkInteraction(): non-player passed as interactor.", LogLevel::Warning);
		}
	}

	// TODO: Move to some player file.
	void HandlePlayerWalkInteraction(ItemInfo& playerItem, ItemInfo& interactable, const InteractionBasis& basis, const InteractionRoutine& routine)
	{
		auto& player = GetLaraInfo(playerItem);

		if (TestInteraction(playerItem, interactable, basis))
		{
			if (MoveLaraPosition(basis.PosOffset, &interactable, &playerItem))
			{
				routine(playerItem, interactable);
				player.Control.IsMoving = false;
				player.Control.HandStatus = HandStatus::Busy;
			}
			else
			{
				player.Context.InteractedItem = interactable.Index;
			}
		}
		else if (player.Control.IsMoving && player.Context.InteractedItem == interactable.Index)
		{
			player.Control.IsMoving = false;
			player.Control.HandStatus = HandStatus::Free;
		}
	}

	void SetInteraction(ItemInfo& interactor, ItemInfo& interactable, const InteractionBasis& basis, const InteractionRoutine& routine, InteractionType type)
	{
		switch (type)
		{
		default:
		case InteractionType::Latch:
			SetEaseInteraction(interactor, interactable, basis, routine);
			break;

		case InteractionType::Walk:
			SetWalkInteraction(interactor, interactable, basis, routine);
			break;
		}
	}

	void DrawDebug(const ItemInfo& item, const InteractionBasis& basis)
	{
		constexpr auto COLL_BOX_COLOR	  = Color(1.0f, 0.0f, 0.0f, 1.0f);
		constexpr auto INTERACT_BOX_COLOR = Color(0.0f, 1.0f, 1.0f, 1.0f);

		// Draw collision box.
		auto collBox = GameBoundingBox(&item).ToBoundingOrientedBox(item.Pose);
		g_Renderer.AddDebugBox(collBox, COLL_BOX_COLOR);

		auto rotMatrix = item.Pose.Orientation.ToRotationMatrix();
		auto relCenter = Vector3::Transform(basis.Box.Center, rotMatrix);
		auto orient = item.Pose.Orientation.ToQuaternion();

		// Draw interaction box.
		auto interactBox = BoundingOrientedBox(item.Pose.Position.ToVector3() + relCenter, basis.Box.Extents, orient);
		g_Renderer.AddDebugBox(interactBox, INTERACT_BOX_COLOR);
	}
}
