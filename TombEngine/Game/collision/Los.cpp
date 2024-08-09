#include "framework.h"
#include "Game/collision/Los.h"

#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/collision/sphere.h"
#include "Game/items.h"
#include "Game/room.h"
#include "Objects/game_object_ids.h"
#include "Math/Math.h"
#include "Physics/Physics.h"
#include "Renderer/Renderer.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;
using namespace TEN::Physics;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

namespace TEN::Collision::Los
{
	static std::vector<ItemInfo*> GetNearbyMoveables(const std::vector<int>& roomNumbers)
	{
		// Collect neighbor room numbers.
		auto neighborRoomNumbers = std::set<int>{};
		for (int roomNumber : roomNumbers)
		{
			const auto& room = g_Level.Rooms[roomNumber];
			neighborRoomNumbers.insert(room.NeighborRoomNumbers.begin(), room.NeighborRoomNumbers.end());
		}

		// Run through neighbor rooms.
		auto movs = std::vector<ItemInfo*>{};
		for (int neighborRoomNumber : neighborRoomNumbers)
		{
			const auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			if (!neighborRoom.Active())
				continue;

			// Run through moveables in room.
			int movID = neighborRoom.itemNumber;
			while (movID != NO_VALUE)
			{
				auto& mov = g_Level.Items[movID];

				// HACK: For some reason, infinite loop may sometimes occur.
				if (movID == mov.NextItem)
					break;
				movID = mov.NextItem;

				// 1) Ignore bridges (handled as part of room collision).
				if (mov.IsBridge())
					continue;

				// 2) Check collidability.
				if (!mov.Collidable)
					continue;

				// 3) Check status.
				if (mov.Status == ItemStatus::ITEM_INVISIBLE || mov.Status == ItemStatus::ITEM_DEACTIVATED)
					continue;

				movs.push_back(&mov);
			}
		}

		return movs;
	}

	static std::vector<MESH_INFO*> GetNearbyStatics(const std::vector<int>& roomNumbers)
	{
		// Run through neighbor rooms.
		auto statics = std::vector<MESH_INFO*>{};
		for (int roomNumber : roomNumbers)
		{
			// Run through neighbor rooms.
			const auto& room = g_Level.Rooms[roomNumber];
			for (auto& neighborRoomNumber : room.NeighborRoomNumbers)
			{
				auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
				if (!neighborRoom.Active())
					continue;

				// Run through statics.
				for (auto& staticObj : neighborRoom.mesh)
				{
					// Check visibility.
					if (!(staticObj.flags & StaticMeshFlags::SM_VISIBLE))
						continue;

					// Collect static.
					statics.push_back(&staticObj);
				}
			}
		}

		return statics;
	}
	
	LosCollisionData GetLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist,
									 bool collideMoveables, bool collideSpheres, bool collideStatics)
	{
		// FAILSAFE.
		if (dir == Vector3::Zero)
		{
			TENLog("GetLosCollision(): dir is not a unit vector.", LogLevel::Warning);
			return LosCollisionData{ RoomLosCollisionData{ {}, origin, roomNumber, {}, 0.0f, false}, {}, {}, {} };
		}

		auto los = LosCollisionData{};

		// 1) Collect room LOS collision.
		los.Room = GetRoomLosCollision(origin, roomNumber, dir, dist);
		dist = los.Room.Distance;

		// 2) Collect moveable and sphere LOS collisions.
		if (collideMoveables || collideSpheres)
		{
			// Run through nearby moveables.
			auto movs = GetNearbyMoveables(los.Room.RoomNumbers);
			for (auto* mov : movs)
			{
				// 2.1) Collect moveable LOS collisions.
				if (collideMoveables)
				{
					auto obb = mov->GetObb();

					float intersectDist = 0.0f;
					if (obb.Intersects(origin, dir, intersectDist) && intersectDist <= dist)
					{
						auto pos = Geometry::TranslatePoint(origin, dir, intersectDist);
						auto offset = pos - mov->Pose.Position.ToVector3();
						int roomNumber = GetPointCollision(mov->Pose.Position, mov->RoomNumber, offset).GetRoomNumber();

						auto movLos = MoveableLosCollisionData{};
						movLos.Moveable = mov;
						movLos.Position = pos;
						movLos.RoomNumber = roomNumber;
						movLos.Distance = intersectDist;
						movLos.IsOriginContained = (bool)obb.Contains(origin);
						los.Moveables.push_back(movLos);
					}
				}

				// 2.2) Collect moveable sphere LOS collisions.
				if (collideSpheres)
				{
					int sphereCount = GetSpheres(mov, CreatureSpheres, SPHERES_SPACE_WORLD, Matrix::Identity);
					for (int i = 0; i < sphereCount; i++)
					{
						auto sphere = BoundingSphere(Vector3(CreatureSpheres[i].x, CreatureSpheres[i].y, CreatureSpheres[i].z), CreatureSpheres[i].r);

						float intersectDist = 0.0f;
						if (sphere.Intersects(origin, dir, intersectDist) && intersectDist <= dist)
						{
							auto pos = Geometry::TranslatePoint(origin, dir, intersectDist);
							auto offset = pos - mov->Pose.Position.ToVector3();
							int roomNumber = GetPointCollision(mov->Pose.Position, mov->RoomNumber, offset).GetRoomNumber();

							auto sphereLos = SphereLosCollisionData{};
							sphereLos.Moveable = mov;
							sphereLos.SphereID = i;
							sphereLos.Position = pos;
							sphereLos.RoomNumber = roomNumber;
							sphereLos.Distance = intersectDist;
							sphereLos.IsOriginContained = (bool)sphere.Contains(origin);
							los.Spheres.push_back(sphereLos);
						}
					}
				}
			}

			// 2.3) Sort moveable LOS collisions.
			std::sort(
				los.Moveables.begin(), los.Moveables.end(),
				[](const auto& movLos0, const auto& movLos1)
				{
					return (movLos0.Distance < movLos1.Distance);
				});

			// 2.4) Sort sphere LOS collisions.
			std::sort(
				los.Spheres.begin(), los.Spheres.end(),
				[](const auto& sphereLos0, const auto& sphereLos1)
				{
					return (sphereLos0.Distance < sphereLos1.Distance);
				});
		}

		// 3) Collect static LOS collisions.
		if (collideStatics)
		{
			// Run through nearby statics.
			auto statics = GetNearbyStatics(los.Room.RoomNumbers);
			for (auto* staticObj : statics)
			{
				auto obb = staticObj->GetObb();

				float intersectDist = 0.0f;
				if (obb.Intersects(origin, dir, intersectDist) && intersectDist <= dist)
				{
					auto pos = Geometry::TranslatePoint(origin, dir, intersectDist);
					auto offset = pos - staticObj->pos.Position.ToVector3();
					int roomNumber = GetPointCollision(staticObj->pos.Position, staticObj->roomNumber, offset).GetRoomNumber();

					auto staticLos = StaticLosCollisionData{};
					staticLos.Static = staticObj;
					staticLos.Position = pos;
					staticLos.RoomNumber = roomNumber;
					staticLos.Distance = intersectDist;
					staticLos.IsOriginContained = (bool)obb.Contains(origin);
					los.Statics.push_back(staticLos);
				}
			}

			// 3.1) Sort static LOS collisions.
			std::sort(
				los.Statics.begin(), los.Statics.end(),
				[](const auto& staticLos0, const auto& staticLos1)
				{
					return (staticLos0.Distance < staticLos1.Distance);
				});
		}

		// 4) Return sorted LOS collision data.
		return los;
	}

	RoomLosCollisionData GetRoomLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collideBridges)
	{
		// FAILSAFE.
		if (dir == Vector3::Zero)
		{
			TENLog("GetRoomLosCollision(): Direction is not a unit vector.", LogLevel::Warning);
			return RoomLosCollisionData{ {}, origin, roomNumber, {}, 0.0f, false };
		}

		auto roomLos = RoomLosCollisionData{};

		// 1) Initialize ray.
		auto ray = Ray(origin, dir);
		float rayDist = dist;
		int rayRoomNumber = roomNumber;

		// 2) Traverse rooms through portals.
		bool traversePortal = true;
		while (traversePortal)
		{
			const CollisionTriangleData* closestTri = nullptr;
			float closestDist = rayDist;

			// 2.1) Clip room.
			const auto& room = g_Level.Rooms[rayRoomNumber];
			auto meshColl = room.CollisionMesh.GetCollision(ray, closestDist);
			if (meshColl.has_value())
			{
				closestTri = &meshColl->Triangle;
				closestDist = meshColl->Distance;
			}

			// 2.2) Clip bridge (if applicable).
			if (collideBridges)
			{
				// Run through neighbor rooms.
				for (int neighborRoomNumber : room.NeighborRoomNumbers)
				{
					const auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];

					// Run through bounded bridges.
					auto bridgeMovIds = neighborRoom.Bridges.GetBoundedIds(ray, closestDist);
					for (int bridgeMovID : bridgeMovIds)
					{
						const auto& bridgeMov = g_Level.Items[bridgeMovID];
						const auto& bridge = GetBridgeObject(bridgeMov);

						// Check bridge status.
						if (bridgeMov.Status == ItemStatus::ITEM_INVISIBLE || bridgeMov.Status == ItemStatus::ITEM_DEACTIVATED)
							continue;

						// Clip bridge.
						auto meshColl = bridge.GetCollisionMesh().GetCollision(ray, closestDist);
						if (meshColl.has_value() && meshColl->Distance < closestDist)
						{
							closestTri = &meshColl->Triangle;
							closestDist = meshColl->Distance;
						}
					}
				}
			}

			// 2.4) Collect room number.
			roomLos.RoomNumbers.push_back(rayRoomNumber);

			// 2.3) Return room LOS collision or traverse new room.
			if (closestTri != nullptr)
			{
				bool isPortal = (closestTri->PortalRoomNumber != NO_VALUE);
				auto intersectPos = Geometry::TranslatePoint(ray.position, ray.direction, closestDist);

				// Hit portal triangle; update ray to traverse new room.
				if (isPortal &&
					rayRoomNumber != closestTri->PortalRoomNumber) // FAILSAFE: Prevent infinite loop.
				{
					ray.position = intersectPos;
					rayDist -= closestDist;
					rayRoomNumber = closestTri->PortalRoomNumber;
				}
				// Hit tangible triangle; collect remaining room LOS collision data.
				else
				{
					if (isPortal)
						TENLog("GetRoomLosCollision(): Room portal cannot link back to itself.", LogLevel::Warning);

					roomLos.Triangle = *closestTri;
					roomLos.Position = intersectPos;
					roomLos.RoomNumber = rayRoomNumber;
					roomLos.IsIntersected = true;
					roomLos.Distance = Vector3::Distance(origin, intersectPos);

					traversePortal = false;
				}
			}
			else
			{
				roomLos.Triangle = std::nullopt;
				roomLos.Position = Geometry::TranslatePoint(ray.position, ray.direction, rayDist);
				roomLos.RoomNumber = rayRoomNumber;
				roomLos.IsIntersected = false;
				roomLos.Distance = dist;

				traversePortal = false;
			}
		}

		// 3) Return room LOS collision.
		return roomLos;
	}

	std::optional<MoveableLosCollisionData> GetMoveableLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collidePlayer)
	{
		// Run through moveable LOS collisions.
		auto los = GetLosCollision(origin, roomNumber, dir, dist, true, false, false);
		for (auto& movLos : los.Moveables)
		{
			// Check if moveable is not player (if applicable).
			if (!collidePlayer && movLos.Moveable->IsLara())
				continue;

			return movLos;
		}

		return std::nullopt;
	}

	std::optional<SphereLosCollisionData> GetSphereLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collidePlayer)
	{
		// Run through sphere LOS collisions.
		auto los = GetLosCollision(origin, roomNumber, dir, dist, false, true, false);
		for (auto& sphereLos : los.Spheres)
		{
			// Check if moveable is not player (if applicable).
			if (!collidePlayer && sphereLos.Moveable->IsLara())
				continue;

			return sphereLos;
		}

		return std::nullopt;
	}

	std::optional<StaticLosCollisionData> GetStaticLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collideOnlySolid)
	{
		// Run through static LOS collisions.
		auto los = GetLosCollision(origin, roomNumber, dir, dist, false, false, true);
		for (auto& staticLos : los.Statics)
		{
			// Check if static is solid (if applicable).
			if (collideOnlySolid && !(staticLos.Static->flags & StaticMeshFlags::SM_SOLID))
				continue;

			return staticLos;
		}

		return std::nullopt;
	}

	std::pair<GameVector, GameVector> GetRayFrom2DPosition(const Vector2& screenPos)
	{
		auto posPair = g_Renderer.GetRay(screenPos);

		auto dir = posPair.second - posPair.first;
		dir.Normalize();
		float dist = Vector3::Distance(posPair.first, posPair.second);

		auto roomLos = GetRoomLosCollision(posPair.first, g_Camera.RoomNumber, dir, dist);

		auto origin = GameVector(posPair.first, g_Camera.RoomNumber);
		auto target = GameVector(roomLos.Position, roomLos.RoomNumber);
		return std::pair(origin, target);
	}
}
