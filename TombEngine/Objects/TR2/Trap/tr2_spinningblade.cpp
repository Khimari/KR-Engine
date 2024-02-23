#include "framework.h"
#include "Objects/TR2/Trap/tr2_spinningblade.h"

#include "Game/animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/PointCollision.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::PointCollision;

void InitializeSpinningBlade(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	SetAnimation(item, 3);
}

void SpinningBladeControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	bool isSpinning = false;

	if (item->Animation.ActiveState == 2)
	{
		if (item->Animation.TargetState != 1)
		{
			int x = item->Pose.Position.x + BLOCK(3) * phd_sin(item->Pose.Orientation.y) / 2;
			int z = item->Pose.Position.z + BLOCK(3) * phd_cos(item->Pose.Orientation.y) / 2;

			int floorHeight = GetPointCollision(Vector3i(x, item->Pose.Position.y, z), item->RoomNumber).GetFloorHeight();
			if (floorHeight == NO_HEIGHT)
				item->Animation.TargetState = 1;
		}

		isSpinning = true;

		if (item->TouchBits.TestAny())
		{
			DoDamage(LaraItem, 100);
			DoLotsOfBlood(LaraItem->Pose.Position.x, LaraItem->Pose.Position.y - CLICK(2), LaraItem->Pose.Position.z, (short)(item->Animation.Velocity.z * 2), LaraItem->Pose.Orientation.y, LaraItem->RoomNumber, 2);
		}

		SoundEffect(SFX_TR2_ROLLING_BLADE, &item->Pose);
	}
	else
	{
		if (TriggerActive(item))
			item->Animation.TargetState = 2;

		isSpinning = false;
	}

	AnimateItem(item);

	auto pointColl = GetPointCollision(*item);

	item->Floor = pointColl.GetFloorHeight();
	item->Pose.Position.y = pointColl.GetFloorHeight();

	if (pointColl.GetRoomNumber() != item->RoomNumber)
		ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

	if (isSpinning && item->Animation.ActiveState == 1)
		item->Pose.Orientation.y += -ANGLE(180.0f);
}
