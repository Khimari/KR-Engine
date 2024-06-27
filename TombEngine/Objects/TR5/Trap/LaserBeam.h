#pragma once


struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	struct LaserBeamEffect
	{
		static constexpr auto SUBDIVISION_COUNT = 8;

		Vector4				Color			 = Vector4::Zero;
		BoundingOrientedBox BoundingBox		 = {};
		std::array<Vector3, SUBDIVISION_COUNT * 2> Vertices = {};

		float Radius = 0.0f;

		bool IsActive		  = false;
		bool IsLethal		  = false;
		bool IsHeavyActivator = false;

		void Initialize(const ItemInfo& item);
		void Update(const ItemInfo& item);
	};

	extern std::unordered_map<int, LaserBeamEffect> LaserBeams;

	void InitializeLaserBeam(short itemNumber);
	void ControlLaserBeam(short itemNumber);
	void CollideLaserBeam(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void ClearLaserBeamEffects();
}
