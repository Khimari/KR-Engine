#pragma once
#include <deque>

#include "Game/room.h"

namespace TEN::Effects::Blood
{
	constexpr auto BLOOD_DRIP_NUM_MAX  = 128;
	constexpr auto BLOOD_STAIN_NUM_MAX = 128;

	constexpr auto BLOOD_DRIP_SPRAY_NUM_MAX_DEFAULT = 2;

	struct BloodDrip
	{
		bool IsActive = false;

		Vector3 Position   = Vector3::Zero;
		int		RoomNumber = NO_ROOM;
		Vector3 Velocity   = Vector3::Zero;
		Vector4 Color	   = Vector4::Zero;

		float Life	  = 0.0f;
		float Scale	  = 0.0f;
		float Gravity = 0.0f;
	};

	struct BloodStain
	{
		int SpriteIndex = 0;

		Vector3	Position	  = Vector3::Zero;
		int		RoomNumber	  = NO_ROOM;
		short	Orientation2D = 0;
		Vector3 Normal		  = Vector3::Zero;
		Vector4	Color		  = Vector4::Zero;
		Vector4	ColorStart	  = Vector4::Zero;
		Vector4	ColorEnd	  = Vector4::Zero;

		std::array<Vector3, 4> VertexPoints = {};

		float Life			  = 0.0f;
		float LifeStartFading = 0.0f;

		float Scale		= 0.0f;
		float ScaleMax	= 0.0f;
		float ScaleRate = 0.0f;

		float Opacity	   = 0.0f;
		float OpacityStart = 0.0f;
	};

	extern std::array<BloodDrip, BLOOD_DRIP_NUM_MAX> BloodDrips;
	extern std::deque<BloodStain>					 BloodStains;

	std::array<Vector3, 4> GetBloodStainVertexPoints(const Vector3& normal, const Vector3& pos, short orient2D, float scale);

	void SpawnBloodMist(const Vector3& pos, int roomNumber, const Vector3& direction, unsigned int count);
	void SpawnBloodMistCloud(const Vector3& pos, int roomNumber, const Vector3& direction, float velocity, unsigned int maxCount);
	void SpawnBloodMistCloudUnderwater(const Vector3& pos, int roomNumber, float velocity);

	void SpawnBloodDrip(const Vector3& pos, int roomNumber, const Vector3& velocity, float scale);
	void SpawnBloodDripSpray(const Vector3& pos, int roomNumber, const Vector3& direction, const Vector3& baseVelocity, unsigned int maxCount = BLOOD_DRIP_SPRAY_NUM_MAX_DEFAULT);

	void SpawnBloodStain(const Vector3& pos, int roomNumber, const Vector3& normal, float scaleMax, float scaleRate);
	void SpawnBloodStainBeneathEntity(const ItemInfo& item, float scale);

	void UpdateBloodMists();
	void UpdateBloodDrips();
	void UpdateBloodStains();

	void DrawIdioticPlaceholders();
}
