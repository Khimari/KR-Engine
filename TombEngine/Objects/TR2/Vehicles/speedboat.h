#pragma once
#include "Objects/TR2/Vehicles/speedboat_info.h"
#include "Objects/Utils/VehicleHelpers.h"

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Vehicles
{
	SpeedboatInfo* GetSpeedboatInfo(ItemInfo* speedboatItem);
	void InitialiseSpeedboat(short itemNumber);

	void SpeedboatPlayerCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void SpeedboatControl(short itemNumber);
	bool SpeedboatUserControl(ItemInfo* speedboatItem, ItemInfo* laraItem);
	void AnimateSpeedboat(ItemInfo* speedboatItem, ItemInfo* laraItem, VehicleImpactDirection impactDirection);

	void DoSpeedboatMount(ItemInfo* speedboatItem, ItemInfo* laraItem, VehicleMountType mountType);
	bool TestSpeedboatDismount(ItemInfo* speedboatItem, int direction);
	void DoSpeedboatDismount(ItemInfo* speedboatItem, ItemInfo* laraItem);
	void DoSpeedboatImpact(ItemInfo* speedboatItem, ItemInfo* laraItem, VehicleImpactDirection impactDirection);

	VehicleImpactDirection SpeedboatDynamics(short itemNumber, ItemInfo* laraItem);
	void DoSpeedboatBoatShift(ItemInfo* speedboatItem, int itemNumber);

	void TriggerSpeedboatFoamEffect(ItemInfo* speedboatItem);
	void TriggerSpeedboatSplashEffect(ItemInfo* speedboatItem, long verticalVelocity, long water); // TODO?
}
