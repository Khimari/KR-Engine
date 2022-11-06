#include "framework.h"
#include "Objects/Generic/Object/polerope.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/sphere.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_tests.h"
#include "Math/Math.h"
#include "Renderer/Renderer11.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Entities::Generic
{
	constexpr auto LADDER_STEP_HEIGHT = BLOCK(1.0f / 8);

	enum class LadderMountType
	{
		None,
		TopFront,
		TopBack,
		Front,
		Back,
		Left,
		Right,
		JumpReach,
		JumpUp
	};

	const std::vector<int> LadderMountedStates =
	{
		LS_LADDER_IDLE,
		LS_LADDER_UP,
		LS_LADDER_DOWN
	};
	const std::vector<int> LadderGroundedMountStates =
	{
		LS_IDLE,
		LS_TURN_LEFT_SLOW,
		LS_TURN_RIGHT_SLOW,
		LS_TURN_LEFT_FAST,
		LS_TURN_RIGHT_FAST,
		LS_WALK_FORWARD,
		LS_RUN_FORWARD
	};
	const std::vector<int> LadderAirborneMountStates =
	{
		LS_REACH,
		LS_JUMP_UP
	};

	const auto LadderMountedOffset2D = Vector3i(0, 0, -BLOCK(1.0f / 7));
	const auto LadderInteractBounds2D = GameBoundingBox(
		-BLOCK(1.0f / 4), BLOCK(1.0f / 4),
		0, 0,
		-BLOCK(3.0f / 8), BLOCK(3.0f / 8)
	);

	const auto LadderMountTopFrontBasis = InteractionBasis(
		LadderMountedOffset2D, // TODO
		EulerAngles(0, ANGLE(180.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(180.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(180.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))
		)
	);
	const auto LadderMountTopBackBasis = InteractionBasis(
		LadderMountedOffset2D, // TODO
		EulerAngles::Zero,
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD, ANGLE(10.0f))
		)
	);
	const auto LadderMountFrontBasis = InteractionBasis(
		LadderMountedOffset2D,
		EulerAngles::Zero,
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD, ANGLE(10.0f))
		)
	);
	const auto LadderMountBackBasis = InteractionBasis(
		LadderMountedOffset2D,
		EulerAngles(0, ANGLE(180.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(180.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(180.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))
		)
	);
	const auto LadderMountLeftBasis = InteractionBasis(
		LadderMountedOffset2D + Vector3i(-BLOCK(1.0f / 4), 0, 0),
		EulerAngles(0, ANGLE(90.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(90.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(90.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))
		)
	);
	const auto LadderMountRightBasis = InteractionBasis(
		LadderMountedOffset2D + Vector3i(BLOCK(1.0f / 4), 0, 0),
		EulerAngles(0, ANGLE(-90.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-90.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(-90.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))
		)
	);

	void DisplayLadderDebug(ItemInfo& ladderItem)
	{
		auto ladderBounds = GameBoundingBox(&ladderItem);
		auto ladderInteractBounds = LadderInteractBounds2D + GameBoundingBox(0, 0, ladderBounds.Y1, ladderBounds.Y2, 0, 0);

		// Render interaction bounds.
		auto box = ladderInteractBounds.ToBoundingOrientedBox(ladderItem.Pose);
		g_Renderer.AddDebugBox(box, Vector4(0, 1, 1, 1), RENDERER_DEBUG_PAGE::NO_PAGE);
	}

	void LadderCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& ladderItem = g_Level.Items[itemNumber];
		auto& lara = *GetLaraInfo(laraItem);

		DisplayLadderDebug(ladderItem);

		// TODO!! This will be MUCH cleaner.
		/*auto mountType = GetLadderMountType(ladderItem, *laraItem);
		if (mountType != LadderMountType::None)
		{
			DoLadderMount(ladderItem, *laraItem, mountType);
			return;
		}*/

		bool isFacingLadder = Geometry::IsPointInFront(laraItem->Pose, ladderItem.Pose.Position.ToVector3());

		// Mount while grounded.
		if ((IsHeld(In::Action) && isFacingLadder &&
			TestState(laraItem->Animation.ActiveState, LadderGroundedMountStates) &&
			lara.Control.HandStatus == HandStatus::Free) ||
			(lara.Control.IsMoving && lara.InteractedItem == itemNumber))
		{
			auto ladderBounds = GameBoundingBox(&ladderItem);
			auto boundsExtension = GameBoundingBox(0, 0, ladderBounds.Y1, ladderBounds.Y2 + LADDER_STEP_HEIGHT, 0, 0);

			// Mount from front.
			if (TestEntityInteraction(*laraItem, ladderItem, LadderMountFrontBasis, boundsExtension))
			{
				if (!laraItem->OffsetBlend.IsActive)
				{
					auto boundsOffset = Vector3i(0, 0, GameBoundingBox(&ladderItem).Z1);
					SetEntityInteraction(*laraItem, ladderItem, LadderMountFrontBasis, boundsOffset);

					SetAnimation(laraItem, LA_LADDER_MOUNT_FRONT);
					lara.Control.IsMoving = false;
					lara.Control.HandStatus = HandStatus::Busy;
				}
				else
					lara.InteractedItem = itemNumber;

				return;
			}

			// Mount from back.
			if (TestEntityInteraction(*laraItem, ladderItem, LadderMountBackBasis, boundsExtension))
			{
				if (!laraItem->OffsetBlend.IsActive)
				{
					auto mountOffset = LadderMountBackBasis.PosOffset + Vector3i(0, 0, GameBoundingBox(&ladderItem).Z2);

					auto targetPos = Geometry::TranslatePoint(ladderItem.Pose.Position, ladderItem.Pose.Orientation, mountOffset);
					auto posOffset = (targetPos - laraItem->Pose.Position).ToVector3();
					auto orientOffset = (ladderItem.Pose.Orientation + LadderMountBackBasis.OrientOffset) - laraItem->Pose.Orientation;
					laraItem->SetOffsetBlend(posOffset, orientOffset);

					SetAnimation(laraItem, LA_LADDER_MOUNT_FRONT);
					lara.Control.IsMoving = false;
					lara.Control.HandStatus = HandStatus::Busy;
				}
				else
					lara.InteractedItem = itemNumber;

				return;
			}

			// Mount from right.
			if (TestEntityInteraction(*laraItem, ladderItem, LadderMountRightBasis, boundsExtension))
			{
				auto mountOffset = LadderMountRightBasis.PosOffset + Vector3i(0, 0, GameBoundingBox(&ladderItem).Z1);

				//if (AlignPlayerToEntity(&ladderItem, laraItem, mountOffset, LadderMountRightOrient))
				if (!laraItem->OffsetBlend.IsActive)
				{
					auto heightOffset = Vector3i(
						0,
						round(laraItem->Pose.Position.y / ladderItem.Pose.Position.y) * LADDER_STEP_HEIGHT, // Target closest step on ladder.
						0
					);

					auto targetPos = Geometry::TranslatePoint(ladderItem.Pose.Position + heightOffset, ladderItem.Pose.Orientation, mountOffset);
					auto posOffset = (targetPos - laraItem->Pose.Position).ToVector3();
					auto orientOffset = (ladderItem.Pose.Orientation + LadderMountRightBasis.OrientOffset) - laraItem->Pose.Orientation;
					laraItem->SetOffsetBlend(posOffset, orientOffset);

					SetAnimation(laraItem, LA_LADDER_MOUNT_RIGHT);
					lara.Control.IsMoving = false;
					lara.Control.HandStatus = HandStatus::Busy;
				}
				else
					lara.InteractedItem = itemNumber;

				return;
			}

			if (lara.Control.IsMoving && lara.InteractedItem == itemNumber)
			{
				lara.Control.HandStatus = HandStatus::Free;
				lara.Control.IsMoving = false;
			}

			return;
		}

		// Mount while airborne.
		if (TrInput & IN_ACTION && isFacingLadder &&
			TestState(laraItem->Animation.ActiveState, LadderAirborneMountStates) &&
			laraItem->Animation.IsAirborne &&
			laraItem->Animation.Velocity.y > 0.0f &&
			lara.Control.HandStatus == HandStatus::Free)
		{
			// Test bounds collision.
			if (TestBoundsCollide(&ladderItem, laraItem, coll->Setup.Radius))//LARA_RADIUS + (int)round(abs(laraItem->Animation.Velocity.z))))
			{
				if (TestCollision(&ladderItem, laraItem))
				{
					auto ladderBounds = GameBoundingBox(&ladderItem);
					int ladderVPos = ladderItem.Pose.Position.y + ladderBounds.Y1;
					int playerVPos = laraItem->Pose.Position.y - LARA_HEIGHT;

					int vOffset = -abs(playerVPos - ladderVPos) / LADDER_STEP_HEIGHT;
					auto mountOffset = Vector3i(0, vOffset, ladderBounds.Z1) + LadderMountedOffset2D;

					if (AlignPlayerToEntity(&ladderItem, laraItem, mountOffset, LadderMountFrontBasis.OrientOffset, true))
					{
						// Reaching.
						if (laraItem->Animation.ActiveState == LS_REACH)
							SetAnimation(laraItem, LA_LADDER_IDLE);// LA_LADDER_MOUNT_JUMP_REACH);
						// Jumping up.
						else
							SetAnimation(laraItem, LA_LADDER_IDLE);// LA_LADDER_MOUNT_JUMP_UP);

						laraItem->Animation.IsAirborne = false;
						laraItem->Animation.Velocity.y = 0.0f;
						lara.Control.HandStatus = HandStatus::Busy;
					}
				}
			}

			return;
		}

		// Player is not interacting with ladder; do regular object collision.
		if (!TestState(laraItem->Animation.ActiveState, LadderMountedStates) &&
			laraItem->Animation.ActiveState != LS_JUMP_BACK) // TODO: Player can jump through ladders.
		{
			ObjectCollision(itemNumber, laraItem, coll);
		}
	}

	bool TestLadderMount(const ItemInfo& ladderItem, ItemInfo& laraItem)
	{
		const auto& lara = *GetLaraInfo(&laraItem);

		// Check for Action input action.
		if (!IsHeld(In::Action))
			return false;

		// Check ladder usability.
		if (ladderItem.Flags & IFLAG_INVISIBLE)
			return false;

		// Check hand status.
		if (lara.Control.HandStatus != HandStatus::Free)
			return false;

		// Check active player state.
		if (!TestState(laraItem.Animation.ActiveState, LadderGroundedMountStates))
			return false;

		return true;
	}

	LadderMountType GetLadderMountType(ItemInfo& ladderItem, ItemInfo& laraItem)
	{
		const auto& lara = *GetLaraInfo(&laraItem);

		// Assesss ladder mountability.
		if (!TestLadderMount(ladderItem, laraItem))
			return LadderMountType::None;

		// Define extension for height of interaction bounds.
		// TODO: Please get height of full ladder stack. Must probe above and below for ladder objects. Steal from vertical pole?
		static auto ladderBounds = GameBoundingBox(&ladderItem);
		auto boundsExtension = GameBoundingBox(0, 0, ladderBounds.Y1, ladderBounds.Y2 + LADDER_STEP_HEIGHT, 0, 0);

		if (TestEntityInteraction(ladderItem, laraItem, LadderMountTopFrontBasis, boundsExtension))
			return LadderMountType::TopFront;

		if (TestEntityInteraction(ladderItem, laraItem, LadderMountTopBackBasis, boundsExtension))
			return LadderMountType::TopBack;

		if (TestEntityInteraction(ladderItem, laraItem, LadderMountFrontBasis, boundsExtension))
			return LadderMountType::Front;

		if (TestEntityInteraction(ladderItem, laraItem, LadderMountBackBasis, boundsExtension))
			return LadderMountType::Back;

		if (TestEntityInteraction(ladderItem, laraItem, LadderMountLeftBasis, boundsExtension))
			return LadderMountType::Left;

		if (TestEntityInteraction(ladderItem, laraItem, LadderMountRightBasis, boundsExtension))
			return LadderMountType::Right;

		return LadderMountType::None;
	}

	void DoLadderMount(const ItemInfo& ladderItem, ItemInfo& laraItem, LadderMountType mountType)
	{
		/*switch (mountType)
		{
		default:
		case LadderMountType::None:
			return;

		case LadderMountType::TopFront:
		case LadderMountType::TopBack:
		case LadderMountType::Front:
		case LadderMountType::Back:
		case LadderMountType::Left:
		case LadderMountType::Right:
		}*/
	}
}
