#pragma once

namespace TEN::Effects::Drip
{
	constexpr auto DRIP_NUM_MAX = 512;
	constexpr auto DRIP_WIDTH	= 4.0f;

	struct DripParticle
	{
		bool IsActive = false;

		Vector3 Position   = Vector3::Zero;
		int		RoomNumber = 0;
		Vector3 Velocity   = Vector3::Zero;
		Vector4 Color	   = Vector4::Zero;
		float	Height	   = 0.0f;

		float Life	  = 0.0f;
		float LifeMax = 0.0f;
		float Gravity = 0.0f;
	};

	extern std::array<DripParticle, DRIP_NUM_MAX> DripParticles;

	DripParticle& GetFreeDrip();

	void SpawnDripParticle(const Vector3& pos, int roomNumber, const Vector3& velocity, float lifeInSec, float gravity);
	void SpawnWetnessDrip(const Vector3& pos, int roomNumber);
	void SpawnSplashDrips(const Vector3& pos, int roomNumber, unsigned int count);
	void SpawnGunshellSplashDrips(const Vector3& pos, int roomNumber, unsigned int count);

	void UpdateDripParticles();
	void ClearDripParticles();
}
