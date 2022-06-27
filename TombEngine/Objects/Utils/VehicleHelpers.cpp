#include "framework.h"
#include "Objects/Utils/VehicleHelpers.h"

#include "Game/effects/simple_particle.h"
#include "Game/effects/tomb4fx.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/sphere.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/room.h"
#include "Sound/sound.h"
#include "Specific/input.h"
#include "Specific/prng.h"

using namespace TEN::Input;

namespace TEN::Entities::Vehicles
{
	VehicleMountType GetVehicleMountType(ItemInfo* vehicleItem, ItemInfo* laraItem, CollisionInfo* coll, vector<VehicleMountType> allowedMountTypes, float minDistance2D)
	{
		auto* lara = GetLaraInfo(laraItem);

		// Assess vehicle usability.
		if (vehicleItem->Flags & ONESHOT)
			return VehicleMountType::None;

		// TODO: Jump mounts don't need an input!
		// Assess ACTION input and hand status.
		if (!(TrInput & IN_ACTION) || lara->Control.HandStatus != HandStatus::Free)
			return VehicleMountType::None;

		// TODO: This tolerance must be larger for water vehicles.
		// Assess vertical distance to vehicle.
		if (abs(laraItem->Pose.Position.y - vehicleItem->Pose.Position.y) > STEPUP_HEIGHT)
			return VehicleMountType::None;

		// Assess 2D distance to vehicle.
		float distance2D = Vector2::Distance(
			Vector2(laraItem->Pose.Position.x, laraItem->Pose.Position.z),
			Vector2(vehicleItem->Pose.Position.x, vehicleItem->Pose.Position.z));
		if (distance2D > minDistance2D)
			return VehicleMountType::None;

		// Assess object collision.
		if (!TestBoundsCollide(vehicleItem, laraItem, coll->Setup.Radius) || !TestCollision(vehicleItem, laraItem))
			return VehicleMountType::None;

		short deltaHeadingAngle = vehicleItem->Pose.Orientation.y - laraItem->Pose.Orientation.y;
		short angleBetweenPositions = vehicleItem->Pose.Orientation.y - GetOrientBetweenPoints(laraItem->Pose.Position, vehicleItem->Pose.Position).y;
		bool onCorrectSide = abs(deltaHeadingAngle - angleBetweenPositions) < ANGLE(45.0f);

		// Assess mount types allowed for vehicle.
		for (auto mountType : allowedMountTypes)
		{
			switch (mountType)
			{
			case VehicleMountType::LevelStart:
				if (!laraItem->Animation.Velocity &&
					!laraItem->Animation.VerticalVelocity &&
					laraItem->Pose.Position == vehicleItem->Pose.Position)
				{
					break;
				}

				continue;

			case VehicleMountType::Front:
				if (deltaHeadingAngle > ANGLE(135.0f) && deltaHeadingAngle < -ANGLE(135.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Back:
				if (abs(deltaHeadingAngle) < ANGLE(45.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Left:
				if (deltaHeadingAngle > -ANGLE(135.0f) && deltaHeadingAngle < -ANGLE(45.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Right:
				if (deltaHeadingAngle > ANGLE(45.0f) && deltaHeadingAngle < ANGLE(135.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Jump:
				if (laraItem->Animation.IsAirborne &&
					laraItem->Animation.VerticalVelocity > 0 &&
					laraItem->Pose.Position.y < vehicleItem->Pose.Position.y)
				{
					break;
				}

				continue;

			default:
				return VehicleMountType::None;
			}

			return mountType;
		}

		return VehicleMountType::None;
	}

	int GetVehicleHeight(ItemInfo* vehicleItem, int forward, int right, bool clamp, Vector3Int* pos)
	{
		float sinX = phd_sin(vehicleItem->Pose.Orientation.x);
		float sinY = phd_sin(vehicleItem->Pose.Orientation.y);
		float cosY = phd_cos(vehicleItem->Pose.Orientation.y);
		float sinZ = phd_sin(vehicleItem->Pose.Orientation.z);

		pos->x = vehicleItem->Pose.Position.x + (forward * sinY) + (right * cosY);
		pos->y = vehicleItem->Pose.Position.y - (forward * sinX) + (right * sinZ);
		pos->z = vehicleItem->Pose.Position.z + (forward * cosY) - (right * sinY);

		// Get collision a bit higher to be able to detect bridges.
		auto probe = GetCollision(pos->x, pos->y - CLICK(2), pos->z, vehicleItem->RoomNumber);

		if (pos->y < probe.Position.Ceiling || probe.Position.Ceiling == NO_HEIGHT)
			return NO_HEIGHT;

		if (pos->y > probe.Position.Floor && clamp)
			pos->y = probe.Position.Floor;

		return probe.Position.Floor;
	}

	int GetVehicleWaterHeight(ItemInfo* vehicleItem, int forward, int right, bool clamp, Vector3Int* pos)
	{
		Matrix world =
			Matrix::CreateFromYawPitchRoll(TO_RAD(vehicleItem->Pose.Orientation.y), TO_RAD(vehicleItem->Pose.Orientation.x), TO_RAD(vehicleItem->Pose.Orientation.z)) *
			Matrix::CreateTranslation(vehicleItem->Pose.Position.x, vehicleItem->Pose.Position.y, vehicleItem->Pose.Position.z);

		Vector3 vec = Vector3(right, 0, forward);
		vec = Vector3::Transform(vec, world);

		pos->x = vec.x;
		pos->y = vec.y;
		pos->z = vec.z;

		auto probe = GetCollision(pos->x, pos->y, pos->z, vehicleItem->RoomNumber);
		int probedRoomNumber = probe.RoomNumber;

		int height = GetWaterHeight(pos->x, pos->y, pos->z, probedRoomNumber);

		if (height == NO_HEIGHT)
		{
			height = probe.Position.Floor;
			if (height == NO_HEIGHT)
				return height;
		}

		height -= 5;

		if (pos->y > height && clamp)
			pos->y = height;

		return height;
	}

	void DoVehicleCollision(ItemInfo* vehicleItem, int radius)
	{
		CollisionInfo coll = {};
		coll.Setup.Radius = radius * 0.8f; // HACK: Most vehicles use radius larger than needed.
		coll.Setup.UpperCeilingBound = MAX_HEIGHT; // HACK: this needs to be set to prevent GCI result interference.
		coll.Setup.OldPosition = vehicleItem->Pose.Position;
		coll.Setup.EnableObjectPush = true;

		DoObjectCollision(vehicleItem, &coll);
	}

	int DoVehicleWaterMovement(ItemInfo* vehicleItem, ItemInfo* laraItem, int currentVelocity, int radius, short* turnRate)
	{
		if (TestEnvironment(ENV_FLAG_WATER, vehicleItem) ||
			TestEnvironment(ENV_FLAG_SWAMP, vehicleItem))
		{
			auto waterDepth = (float)GetWaterDepth(vehicleItem);
			auto waterHeight = vehicleItem->Pose.Position.y - GetWaterHeight(vehicleItem);

			// HACK: Sometimes quadbike test position may end up under non-portal ceiling block.
			// GetWaterDepth returns DEEP_WATER constant in that case, which is too large for our needs.
			if (waterDepth == DEEP_WATER)
				waterDepth = VEHICLE_WATER_HEIGHT_MAX;

			if (waterDepth <= VEHICLE_WATER_HEIGHT_MAX)
			{
				bool isWater = TestEnvironment(ENV_FLAG_WATER, vehicleItem);

				if (currentVelocity != 0)
				{
					auto coeff = isWater ? VEHICLE_WATER_VELOCITY_COEFF : VEHICLE_SWAMP_VELOCITY_COEFF;
					currentVelocity -= std::copysign(currentVelocity * ((waterDepth / VEHICLE_WATER_HEIGHT_MAX) / coeff), currentVelocity);

					if (TEN::Math::Random::GenerateInt(0, 32) > 28)
						SoundEffect(SFX_TR4_LARA_WADE, &PHD_3DPOS(vehicleItem->Pose.Position), SoundEnvironment::Land, isWater ? 0.8f : 0.7f);

					if (isWater)
						TEN::Effects::TriggerSpeedboatFoam(vehicleItem, Vector3(0, -waterDepth / 2.0f, -radius));
				}

				if (*turnRate)
				{
					auto coeff = isWater ? VEHICLE_WATER_TURN_RATE_COEFF : VEHICLE_SWAMP_TURN_RATE_COEFF;
					*turnRate -= *turnRate * ((waterDepth / VEHICLE_WATER_HEIGHT_MAX) / coeff);
				}
			}
			else
			{
				if (waterDepth > VEHICLE_WATER_HEIGHT_MAX && waterHeight > VEHICLE_WATER_HEIGHT_MAX)
					ExplodeVehicle(laraItem, vehicleItem);
				else if (TEN::Math::Random::GenerateInt(0, 32) > 25)
					Splash(vehicleItem);
			}
		}

		return currentVelocity;
	}

	void ModulateVehicleTurnRateX(short* turnRate, short accelRate, short minTurnRate, short maxTurnRate)
	{
		*turnRate = ModulateLaraTurnRate(*turnRate, accelRate, minTurnRate, maxTurnRate, -AxisMap[InputAxis::MoveVertical]);
	}

	void ModulateVehicleTurnRateY(short* turnRate, short accelRate, short minTurnRate, short maxTurnRate)
	{
		*turnRate = ModulateLaraTurnRate(*turnRate, accelRate, minTurnRate, maxTurnRate, AxisMap[InputAxis::MoveHorizontal]);
	}
}
