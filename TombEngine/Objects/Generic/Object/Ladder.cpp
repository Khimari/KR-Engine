#include "framework.h"
#include "Objects/Generic/Object/Ladder.h"

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
	constexpr auto LADDER_STEP_HEIGHT = CLICK(0.5f);

	enum class LadderMountType
	{
		None,
		TopFront,
		TopBack,
		Front,
		Back,
		Left,
		Right,
		JumpUp,
		JumpReach
	};

	const auto LadderMountedStates = std::vector<int>
	{
		LS_LADDER_IDLE,
		LS_LADDER_UP,
		LS_LADDER_DOWN
	};
	const auto LadderGroundedMountStates = std::vector<int>
	{
		LS_IDLE,
		LS_TURN_LEFT_SLOW,
		LS_TURN_RIGHT_SLOW,
		LS_TURN_LEFT_FAST,
		LS_TURN_RIGHT_FAST,
		LS_WALK_FORWARD,
		LS_RUN_FORWARD
	};
	const auto LadderAirborneMountStates = std::vector<int>
	{
		LS_REACH,
		LS_JUMP_UP
	};

	const auto LadderMountedOffset2D = Vector3i(0, 0, -LARA_RADIUS * 1.5f);
	const auto LadderInteractBounds2D = GameBoundingBox(
		-BLOCK(0.25f), BLOCK(0.25f),
		0, 0,
		-BLOCK(3 / 8.0f), BLOCK(3 / 8.0f));

	const auto LadderMountTopFrontBasis = InteractionBasis(
		LadderMountedOffset2D, // TODO
		EulerAngles(0, ANGLE(180.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(180.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(180.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))));

	const auto LadderMountTopBackBasis = InteractionBasis(
		LadderMountedOffset2D, // TODO
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD, ANGLE(10.0f))));

	const auto LadderMountFrontBasis = InteractionBasis(
		LadderMountedOffset2D,
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD, ANGLE(10.0f))));

	const auto LadderMountBackBasis = InteractionBasis(
		LadderMountedOffset2D,
		EulerAngles(0, ANGLE(180.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(180.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(180.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))));

	const auto LadderMountLeftBasis = InteractionBasis(
		LadderMountedOffset2D + Vector3i(-BLOCK(0.25f), 0, 0),
		EulerAngles(0, ANGLE(90.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(90.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(90.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))));

	const auto LadderMountRightBasis = InteractionBasis(
		LadderMountedOffset2D + Vector3i(BLOCK(0.25f), 0, 0),
		EulerAngles(0, ANGLE(-90.0f), 0),
		LadderInteractBounds2D,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-90.0f) - LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(-90.0f) + LARA_GRAB_THRESHOLD, ANGLE(10.0f))));

	static void DrawLadderDebug(ItemInfo& ladderItem)
	{
		// Render collision bounds.
		auto ladderBounds = GameBoundingBox(&ladderItem);
		auto collBox = ladderBounds.ToBoundingOrientedBox(ladderItem.Pose);
		g_Renderer.AddDebugBox(collBox, Vector4(1, 0, 0, 1), RENDERER_DEBUG_PAGE::NO_PAGE);

		// Render interaction bounds.
		auto ladderInteractBounds = LadderInteractBounds2D + GameBoundingBox(0, 0, ladderBounds.Y1, ladderBounds.Y2, 0, 0);
		auto interactBox = ladderInteractBounds.ToBoundingOrientedBox(ladderItem.Pose);
		g_Renderer.AddDebugBox(interactBox, Vector4(0, 1, 1, 1), RENDERER_DEBUG_PAGE::NO_PAGE);
	}

	void LadderCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& ladderItem = g_Level.Items[itemNumber];
		auto& player = *GetLaraInfo(laraItem);

		DrawLadderDebug(ladderItem);

		// TODO!! This will be MUCH cleaner.
		auto mountType = GetLadderMountType(ladderItem, *laraItem);
		if (mountType == LadderMountType::None)
		{
			if (!TestState(laraItem->Animation.ActiveState, LadderMountedStates) &&
				laraItem->Animation.ActiveState != LS_JUMP_BACK) // TODO: Player can jump through ladders.
			{
				ObjectCollision(itemNumber, laraItem, coll);
			}
		}

		DoLadderMount(itemNumber, ladderItem, *laraItem, (LadderMountType)mountType);
		return;
		
		// Mount while grounded.
		if ((IsHeld(In::Action) &&
			TestState(laraItem->Animation.ActiveState, LadderGroundedMountStates) &&
			player.Control.HandStatus == HandStatus::Free) ||
			(player.Control.IsMoving && player.Context.InteractedItem == itemNumber))
		{
			if (player.Control.IsMoving && player.Context.InteractedItem == itemNumber)
			{
				player.Control.HandStatus = HandStatus::Free;
				player.Control.IsMoving = false;
			}

			return;
		}

		// Mount while airborne.
		if (IsHeld(In::Action) &&
			TestState(laraItem->Animation.ActiveState, LadderAirborneMountStates) &&
			laraItem->Animation.IsAirborne &&
			laraItem->Animation.Velocity.y > 0.0f &&
			player.Control.HandStatus == HandStatus::Free)
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
						player.Control.HandStatus = HandStatus::Busy;
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

	LadderMountType GetLadderMountType(ItemInfo& ladderItem, ItemInfo& laraItem)
	{
		const auto& player = *GetLaraInfo(&laraItem);

		// Test ladder mountability.
		if (!TestLadderMount(ladderItem, laraItem))
			return LadderMountType::None;

		// Define extension for height of interaction bounds.
		// TODO: Get height of full ladder stack. Must probe above and below for ladder objects. Steal from vertical pole?
		auto ladderBounds = GameBoundingBox(&ladderItem);
		auto boundsExtension = GameBoundingBox(0, 0, ladderBounds.Y1, ladderBounds.Y2 + LADDER_STEP_HEIGHT, 0, 0);

		/*if (TestEntityInteraction(laraItem, ladderItem, LadderMountTopFrontBasis, boundsExtension))
			return LadderMountType::TopFront;

		if (TestEntityInteraction(laraItem, ladderItem, LadderMountTopBackBasis, boundsExtension))
			return LadderMountType::TopBack;*/

		if (TestEntityInteraction(laraItem, ladderItem, LadderMountFrontBasis, boundsExtension))
		{
			if (laraItem.Animation.IsAirborne && laraItem.Animation.Velocity.y > 0.0f)
			{
				if (laraItem.Animation.ActiveState == LS_JUMP_UP)
				{
					return LadderMountType::JumpUp;
				}
				else if (laraItem.Animation.ActiveState == LS_REACH)
				{
					return LadderMountType::JumpReach;
				}
			}
			else
			{
				return LadderMountType::Front;
			}
		}

		if (TestEntityInteraction(laraItem, ladderItem, LadderMountBackBasis, boundsExtension))
			return LadderMountType::Back;

		/*if (TestEntityInteraction(laraItem, ladderItem, LadderMountLeftBasis, boundsExtension))
			return LadderMountType::Left;*/

		if (TestEntityInteraction(laraItem, ladderItem, LadderMountRightBasis, boundsExtension))
			return LadderMountType::Right;

		return LadderMountType::None;
	}

	bool TestLadderMount(const ItemInfo& ladderItem, ItemInfo& laraItem)
	{
		const auto& player = *GetLaraInfo(&laraItem);

		// Check for input action.
		if (!IsHeld(In::Action))
			return false;

		// Check ladder usability.
		if (ladderItem.Flags & IFLAG_INVISIBLE)
			return false;

		// Check hand status.
		if (player.Control.HandStatus != HandStatus::Free)
			return false;

		// Check active player state.
		if (!TestState(laraItem.Animation.ActiveState, LadderGroundedMountStates))
			return false;

		// TODO: Assess whether ladder is in front.

		return true;
	}

	void DoLadderMount(int itemNumber, ItemInfo& ladderItem, ItemInfo& laraItem, LadderMountType mountType)
	{
		auto& player = *GetLaraInfo(&laraItem);

		// Avoid interference if already interacting.
		if (laraItem.OffsetBlend.IsActive)
			player.Context.InteractedItem = itemNumber;

		ModulateLaraTurnRateY(&laraItem, 0, 0, 0);

		auto ladderBounds = GameBoundingBox(&ladderItem);

		switch (mountType)
		{
		default:
		case LadderMountType::None:
			return;

		case LadderMountType::TopFront:
			break;

		case LadderMountType::TopBack:
			break;

		case LadderMountType::Front:
		{
			auto boundsOffset = Vector3i(0, 0, ladderBounds.Z1);
			SetEntityInteraction(laraItem, ladderItem, LadderMountFrontBasis, boundsOffset);
			SetAnimation(&laraItem, LA_LADDER_MOUNT_FRONT);
			break;
		}

		case LadderMountType::Back:
		{
			auto boundsOffset = Vector3i(0, 0, ladderBounds.Z2);
			SetEntityInteraction(laraItem, ladderItem, LadderMountBackBasis, boundsOffset);
			SetAnimation(&laraItem, LA_LADDER_MOUNT_FRONT);
			break;
		}

		case LadderMountType::Left:
			break;

		case LadderMountType::Right:
		{
			auto boundsOffset = Vector3i(0, 0, ladderBounds.Z1)/* +
				Vector3i(
				0,
				round(laraItem.Pose.Position.y / ladderItem.Pose.Position.y) * LADDER_STEP_HEIGHT, // Target closest step on ladder.
				0)*/;

			SetEntityInteraction(laraItem, ladderItem, LadderMountRightBasis, boundsOffset);
			SetAnimation(&laraItem, LA_LADDER_MOUNT_RIGHT);
			break;
		}

		case LadderMountType::JumpUp:
			break;

		case LadderMountType::JumpReach:
			break;
		}

		player.Control.IsMoving = false;
		player.Control.HandStatus = HandStatus::Busy;
	}
}
