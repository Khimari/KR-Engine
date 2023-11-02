#include "framework.h"
#include "Objects/TR5/Trap/ZipLine.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Interaction.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision;
using namespace TEN::Input;
using namespace TEN::Math;

namespace TEN::Traps::TR5
{
	const auto ZipLineInteractBasis = InteractionBasis
	{
		Vector3i(0, 0, BLOCK(3 / 8.0f)),
		GameBoundingBox(
			-BLOCK(0.25f), BLOCK(0.25f),
			-CLICK(1), CLICK(1),
			BLOCK(0.25f), BLOCK(0.5f)),
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD, ANGLE(10.0f)))
	};

	void InitializeZipLine(short itemNumber)
	{
		auto& zipLineItem = g_Level.Items[itemNumber];
		zipLineItem.Data = GameVector();
		auto& pos = *(GameVector*)zipLineItem.Data;

		pos = GameVector(zipLineItem.Pose.Position, zipLineItem.RoomNumber);
	}

	void ControlZipLine(short itemNumber)
	{
		constexpr auto VEL_ACCEL = 5.0f;
		constexpr auto VEL_MAX	 = 100.0f;

		auto& zipLineItem = g_Level.Items[itemNumber];
		auto& laraItem = *LaraItem;
		auto& player = GetLaraInfo(laraItem);

		if (zipLineItem.Status != ITEM_ACTIVE)
			return;

		if (!(zipLineItem.Flags & IFLAG_INVISIBLE))
		{
			auto& prevPos = *(GameVector*)zipLineItem.Data;

			zipLineItem.Pose.Position = prevPos.ToVector3i();

			if (prevPos.RoomNumber != zipLineItem.RoomNumber)
				ItemNewRoom(itemNumber, prevPos.RoomNumber);

			zipLineItem.Status = ITEM_NOT_ACTIVE;
			SetAnimation(zipLineItem, 0);

			RemoveActiveItem(itemNumber);
			return;
		}

		if (zipLineItem.Animation.ActiveState == 1)
		{
			AnimateItem(&zipLineItem);
			return;
		}

		AnimateItem(&zipLineItem);

		// Accelerate.
		if (zipLineItem.Animation.Velocity.y < VEL_MAX)
			zipLineItem.Animation.Velocity.y += VEL_ACCEL;

		// Translate.
		// TODO: Use proper calculation of the trajectory instead of bitwise operation.
		auto headingOrient = EulerAngles(0, zipLineItem.Pose.Orientation.y, 0);
		TranslateItem(&zipLineItem, headingOrient, zipLineItem.Animation.Velocity.y);
		zipLineItem.Pose.Position.y += ((int)zipLineItem.Animation.Velocity.y >> 2);

		int vPos = zipLineItem.Pose.Position.y + CLICK(0.25f);
		auto pointColl = GetCollision(&zipLineItem, zipLineItem.Pose.Orientation.y, zipLineItem.Animation.Velocity.y);

		// Update zip line room number.
		if (pointColl.RoomNumber != zipLineItem.RoomNumber)
			ItemNewRoom(itemNumber, pointColl.RoomNumber);

		if (pointColl.Position.Floor <= (vPos + CLICK(1)) || pointColl.Position.Ceiling >= (vPos - CLICK(1)))
		{
			// Dismount.
			if (laraItem.Animation.ActiveState == LS_ZIP_LINE)
			{
				laraItem.Animation.TargetState = LS_JUMP_FORWARD;
				laraItem.Animation.IsAirborne = true;
				laraItem.Animation.Velocity.y = zipLineItem.Animation.Velocity.y / 4;
				laraItem.Animation.Velocity.z = zipLineItem.Animation.Velocity.y;
				player.Context.InteractedItem = NO_ITEM;
			}

			SoundEffect(SFX_TR4_VONCROY_KNIFE_SWISH, &zipLineItem.Pose);
			RemoveActiveItem(itemNumber);
			zipLineItem.Status = ITEM_NOT_ACTIVE;
			zipLineItem.Flags -= IFLAG_INVISIBLE;
		}
		else
		{
			// Attach player to zip line.
			if (laraItem.Animation.ActiveState == LS_ZIP_LINE)
				laraItem.Pose.Position = zipLineItem.Pose.Position;

			// Whizz sound.
			SoundEffect(SFX_TR4_TRAIN_DOOR_CLOSE, &zipLineItem.Pose);
		}
	}

	static bool CanPlayerUseZipLine(const ItemInfo& playerItem, const ItemInfo& zipLineItem)
	{
		auto& player = GetLaraInfo(playerItem);

		// Check zip line status.
		if (zipLineItem.Status != ITEM_NOT_ACTIVE)
			return false;

		// Test for player input action.
		if (!IsHeld(In::Action))
			return false;

		// Check player status.
		if ((playerItem.Animation.ActiveState == LS_IDLE &&
			!playerItem.Animation.IsAirborne &&
			player.Control.HandStatus == HandStatus::Free) ||
			(player.Control.IsMoving && player.Context.InteractedItem == zipLineItem.Index))
		{
			return true;
		}

		return false;
	}

	static void InteractZipLine(ItemInfo& playerItem, ItemInfo& zipLineItem)
	{
		SetAnimation(playerItem, LA_ZIPLINE_MOUNT);
		ResetPlayerFlex(&playerItem);

		if (playerItem.Animation.ActiveState == LS_GRABBING)
		{
			if (!zipLineItem.Active)
				AddActiveItem(zipLineItem.Index);

			zipLineItem.Status = ITEM_ACTIVE;
			zipLineItem.Flags |= IFLAG_INVISIBLE;
		}
	}

	void CollideZipLine(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& zipLineItem = g_Level.Items[itemNumber];
		auto& player = GetLaraInfo(*playerItem);

		// Interact with zip line.
		if (CanPlayerUseZipLine(*playerItem, zipLineItem))
			HandlePlayerInteraction(*playerItem, zipLineItem, ZipLineInteractBasis, InteractZipLine);
	}
}
