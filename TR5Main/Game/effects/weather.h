#pragma once
#include <SimpleMath.h>
#include "Scripting/ScriptInterfaceLevel.h"
#include "Specific/trmath.h"

namespace TEN {
namespace Effects {
namespace Environment 
{
	constexpr auto WEATHER_PARTICLES_SPAWN_DENSITY = 32;
	constexpr auto WEATHER_PARTICLES_MAX_COUNT = 1024;
	constexpr auto WEATHER_PARTICLES_MAX_COLL_CHECK_DELAY = 5.0f;

	constexpr auto MAX_SNOW_SIZE = 32.0f;
	constexpr auto MAX_RAIN_SIZE = 128.0f;
	
	constexpr auto WEATHER_PARTICLE_HORIZONTAL_SPEED = 8.0f;
	constexpr auto SNOW_SPEED = 128.0f;
	constexpr auto RAIN_SPEED = 256.0f;

	constexpr auto WEATHER_PARTICLES_TRANSPARENCY = 0.8f;
	constexpr auto WEATHER_PARTICLES_NEAR_DEATH_LIFE_VALUE = 20.0f;
	constexpr auto WEATHER_PARTICLES_NEAR_DEATH_MELT_FACTOR = 1.0f - (1.0f / (WEATHER_PARTICLES_NEAR_DEATH_LIFE_VALUE * 2));

	constexpr auto SKY_POSITION_LIMIT = 9728;

	struct WeatherParticle
	{
		WeatherType Type = WeatherType::None;

		int Room = -1;
		Vector3 Position = Vector3::Zero;
		Vector3 Velocity = Vector3::Zero;

		float StartLife = 0.0f;
		float Life = 0.0f;
		float CollisionCheckDelay = 0.0f;
		float Size = 0.0f;

		bool Enabled = false;
		bool Stopped = false;

		float Transparency() const;
	};

	class EnvironmentController
	{
	public:
		EnvironmentController();

		Vector3 Wind() { return Vector3(WindX / 2.0f, 0, WindZ / 2.0f); }
		Vector3 FlashColor() { return FlashColorBase * sin(FlashProgress * PI / 2.0f); }
		Vector4 SkyColor() { return SkyCurrentColor; }
		short   SkyLayer1Position() { return SkyPosition1; }
		short   SkyLayer2Position() { return SkyPosition2; }

		void Flash(int r, int g, int b, float speed);
		void Update();
		void Clear();

		const std::vector<WeatherParticle>& GetParticles() const { return Particles; }

	private:
		// Weather
		std::vector<WeatherParticle> Particles;

		// Sky
		Vector4 SkyCurrentColor = Vector4::Zero;
		short   SkyPosition1 = 0;
		short   SkyPosition2 = 0;

		// Wind
		int WindX = 0;
		int WindZ = 0;
		int WindAngle = 0;
		int WindDAngle = 0;
		int WindCurrent = 0;

		// Flash fader
		Vector3 FlashColorBase = Vector3::Zero;
		float   FlashSpeed = 1.0f;
		float   FlashProgress = 0.0f;

		// Lightning
		int  StormCount = 0;
		int  StormRand = 0;
		int  StormTimer = 0;
		byte StormSkyColor = 1;
		byte StormSkyColor2 = 1;

		void UpdateSky(ScriptInterfaceLevel* level);
		void UpdateStorm(ScriptInterfaceLevel* level);
		void UpdateWind(ScriptInterfaceLevel* level);
		void UpdateFlash(ScriptInterfaceLevel* level);
		void UpdateWeather(ScriptInterfaceLevel* level);
		void UpdateLightning();

		void SpawnWeatherParticles(ScriptInterfaceLevel* level);
	};

	extern EnvironmentController Weather;
}}}