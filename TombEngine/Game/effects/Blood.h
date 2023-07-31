#pragma once

struct CollisionResult;
struct ItemInfo;

namespace TEN::Effects::Blood
{
	struct BloodDripEffectParticle
	{
		static constexpr auto LIFE_START_FADING = 0.5f;

		unsigned int SpriteID	   = 0;
		bool		 CanSpawnStain = false;

		Vector3 Position   = Vector3::Zero;
		int		RoomNumber = 0;
		Vector3 Velocity   = Vector3::Zero;
		Vector2 Size	   = Vector2::Zero;
		Vector4 Color	   = Vector4::Zero;

		float Life			  = 0.0f;
		float LifeStartFading = 0.0f;
		float Opacity		  = 0.0f;
		float Gravity		  = 0.0f;

		void Update();
	};

	class BloodDripEffectController
	{
	private:
		// Members
		std::vector<BloodDripEffectParticle> Particles;

	public:
		// Getters
		const std::vector<BloodDripEffectParticle>& GetParticles();

		// Spawners
		void Spawn(const Vector3& pos, int roomNumber, const Vector3& vel, const Vector2& size, float lifeInSec, bool canSpawnStain);

		// Utilities
		void Update();
		void Clear();
	};

	struct BloodStainEffectParticle
	{
		static constexpr auto LIFE_MAX			= 5.0f * 60.0f;
		static constexpr auto LIFE_START_FADING = 30.0f;
		static constexpr auto SURFACE_OFFSET	= 4;
		static constexpr auto VERTEX_COUNT		= 4;

		unsigned int SpriteID = 0;

		Vector3	Position	  = Vector3::Zero;
		int		RoomNumber	  = 0;
		short	Orientation2D = 0;
		Vector3 Normal		  = Vector3::Zero;
		Vector4	Color		  = Vector4::Zero;
		Vector4	ColorStart	  = Vector4::Zero;
		Vector4	ColorEnd	  = Vector4::Zero;

		std::array<Vector3, VERTEX_COUNT> VertexPoints = {};

		float Life			  = 0.0f;
		float LifeStartFading = 0.0f;
		float Size			  = 0.0f;
		float SizeMax		  = 0.0f;
		float Scalar		  = 0.0f;
		float Opacity		  = 0.0f;
		float OpacityMax	  = 0.0f;
		float DelayTime		  = 0.0f;

		void Update();

		std::array<Vector3, VERTEX_COUNT> GetVertexPoints();
		bool							  TestSurface();
	};

	class BloodStainEffectController
	{
	private:
		// Members
		std::vector<BloodStainEffectParticle> Particles = {};

	public:
		// Getters
		const std::vector<BloodStainEffectParticle>& GetParticles();

		// Spawners
		void Spawn(const Vector3& pos, int roomNumber, const Vector3& normal, float sizeMax, float scalar, float delayInSec = 0.0f);
		void Spawn(const BloodDripEffectParticle& drip, const CollisionResult& pointColl, bool isOnFloor);
		void Spawn(const ItemInfo& item);

		// Utilities
		void Update();
		void Clear();
	};
	
	struct BloodMistEffectParticle
	{
		unsigned int SpriteID = 0;

		Vector3 Position	  = Vector3::Zero;
		int		RoomNumber	  = 0;
		short	Orientation2D = 0;
		Vector3 Velocity	  = Vector3::Zero;
		Vector4 Color		  = Vector4::Zero;
		
		float Life		 = 0.0f;
		float LifeMax	 = 0.0f;
		float Size		 = 0.0f;
		float SizeMax	 = 0.0f;
		float SizeMin	 = 0.0f;
		float Opacity	 = 0.0f;
		float OpacityMax = 0.0f;
		float Gravity	 = 0.0f;
		float Friction	 = 0.0f;
		short Rotation	 = 0;

		void Update();
	};

	class BloodMistEffectController
	{
	private:
		// Members
		std::vector<BloodMistEffectParticle> Particles = {};

	public:
		// Spawners
		const std::vector<BloodMistEffectParticle>& GetParticles();

		// Spawners
		void Spawn(const Vector3& pos, int roomNumber, const Vector3& dir, unsigned int count = 1);

		// Utilities
		void Update();
		void Clear();
	};

	// TODO: Copy approach from ripple effect.
	struct UnderwaterBloodEffectParticle
	{
		unsigned int SpriteID = 0;

		Vector3 Position   = Vector3::Zero;
		int		RoomNumber = 0;
		Vector4 Color	   = Vector4::Zero;

		float Life	  = 0.0f;
		float Init	  = 0.0f;
		float Size	  = 0.0f;
		float Opacity = 0.0f;

		void Update();
	};

	class UnderwaterBloodEffectController
	{
	private:
		// Members
		std::vector<UnderwaterBloodEffectParticle> Particles;

	public:
		// Getters
		const std::vector<UnderwaterBloodEffectParticle>& GetParticles();

		// Spawners
		void Spawn(const Vector3& pos, int roomNumber, float size, unsigned int count = 1);

		// Utilities
		void Update();
		void Clear();
	};

	extern BloodDripEffectController	   BloodDripEffect;
	extern BloodStainEffectController	   BloodStainEffect;
	extern BloodMistEffectController	   BloodMistEffect;
	extern UnderwaterBloodEffectController UnderwaterBloodEffect;

	void SpawnBloodSplatEffect(const Vector3& pos, int roomNumber, const Vector3& dir, const Vector3& baseVel, unsigned int baseCount);

	// Legacy spawners
	short DoBloodSplat(int x, int y, int z, short vel, short yRot, short roomNumber);
	void DoLotsOfBlood(int x, int y, int z, int vel, short headingAngle, int roomNumber, unsigned int count);
	void TriggerBlood(int x, int y, int z, short headingAngle, unsigned int count);
	void TriggerLaraBlood();

	void DrawBloodDebug();
}
