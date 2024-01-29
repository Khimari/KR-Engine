#include "framework.h"
#include "Game/collision/AttractorCollision.h"

#include "Game/collision/Attractor.h"
#include "Game/collision/floordata.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Collision::Attractor
{
	AttractorCollisionData::AttractorCollisionData(Attractor& attrac, unsigned int segmentID, const Vector3& pos, short headingAngle, const Vector3& axis)
	{
		constexpr auto HEADING_ANGLE_OFFSET = ANGLE(270.0f);

		const auto& points = attrac.GetPoints();

		// FAILSAFE: Clamp segment ID.
		unsigned int segmentCount = attrac.GetSegmentCount();
		if (segmentID < 0 || segmentID >= segmentCount)
		{
			TENLog("Attempted to get attractor collision data for invalid segment.", LogLevel::Warning);
			segmentID = std::clamp<unsigned int>(segmentID, 0, segmentCount - 1);
		}

		// Set attractor pointer and get proximity data.
		AttractorPtr = &attrac;
		Proximity = GetProximity(pos, segmentID, axis);

		// TODO: Incorporate axis.
		// Calculate orientations.
		auto refOrient = EulerAngles(0, headingAngle, 0);
		auto segmentOrient = (points.size() == 1) ?
			refOrient : Geometry::GetOrientToPoint(points[Proximity.SegmentID], points[Proximity.SegmentID + 1]);

		// Set remaining collision data.
		HeadingAngle = segmentOrient.y + HEADING_ANGLE_OFFSET;
		SlopeAngle = segmentOrient.x;
		IsInFront = Geometry::IsPointInFront(pos, Proximity.Intersection, refOrient);
	}

	AttractorCollisionData::ProximityData AttractorCollisionData::GetProximity(const Vector3& pos, unsigned int segmentID, const Vector3& axis) const
	{
		const auto& points = AttractorPtr->GetPoints();

		// Single point exists; return simple proximity data.
		if (points.size() == 1)
		{
			const auto& intersect = points.front();

			float dist2D = Vector2::Distance(Vector2(pos.x, pos.z), Vector2(intersect.x, intersect.z));
			float dist3D = Vector3::Distance(pos, intersect);
			return ProximityData{ intersect, dist2D, dist3D, 0.0f, 0 };
		}

		// Accumulate distance traveled along attractor toward intersection.
		float chainDistTraveled = 0.0f;
		for (unsigned int i = 0; i < segmentID; i++)
		{
			float segmentLength = AttractorPtr->GetSegmentLengths()[i];
			chainDistTraveled += segmentLength;
		}

		// Get segment points.
		const auto& origin = points[segmentID];
		const auto& target = points[segmentID + 1];

		// Calculate axis-perpendicular intersection.
		auto intersect = Geometry::GetClosestPointOnLinePerp(pos, origin, target, axis);

		// Accumulate final distance traveled along attractor toward intersection.
		chainDistTraveled += Vector3::Distance(origin, intersect);

		// Create proximity data.
		auto attracProx = ProximityData{};
		attracProx.Intersection = intersect;
		attracProx.Distance2D = Vector2::Distance(Vector2(pos.x, pos.z), Vector2(intersect.x, intersect.z));
		attracProx.Distance3D = Vector3::Distance(pos, intersect);
		attracProx.ChainDistance = chainDistTraveled;
		attracProx.SegmentID = segmentID;

		// Return proximity data.
		return attracProx;
	}

	AttractorCollisionData GetAttractorCollision(Attractor& attrac, unsigned int segmentID, const Vector3& pos, short headingAngle, const Vector3& axis)
	{
		return AttractorCollisionData(attrac, segmentID, pos, headingAngle, axis);
	}

	AttractorCollisionData GetAttractorCollision(Attractor& attrac, float chainDist, short headingAngle, const Vector3& axis)
	{
		unsigned int segmentID = attrac.GetSegmentIDAtChainDistance(chainDist);
		auto pos = attrac.GetIntersectionAtChainDistance(chainDist);

		return AttractorCollisionData(attrac, segmentID, pos, headingAngle, axis);
	}
	
	// Debug
	static std::vector<Attractor*> GetDebugAttractorPtrs()
	{
		auto& player = GetLaraInfo(*LaraItem);

		auto debugAttracPtrs = std::vector<Attractor*>{};
		debugAttracPtrs.push_back(&*player.Context.DebugAttracs.Attrac0);
		debugAttracPtrs.push_back(&*player.Context.DebugAttracs.Attrac1);
		debugAttracPtrs.push_back(&*player.Context.DebugAttracs.Attrac2);

		for (auto& attrac : player.Context.DebugAttracs.Attracs)
			debugAttracPtrs.push_back(&attrac);

		return debugAttracPtrs;
	}

	// TODO: Spacial partitioning may be ideal here. Would require a general collision refactor. -- Sezz 2023.07.30
	static std::vector<Attractor*> GetNearbyAttractorPtrs(const Vector3& pos, int roomNumber, float detectRadius)
	{
		constexpr auto SECTOR_SEARCH_DEPTH = 2;

		auto sphere = BoundingSphere(pos, detectRadius);
		auto nearbyAttracPtrs = std::vector<Attractor*>{};

		// Draw debug sphere.
		g_Renderer.AddDebugSphere(sphere.Center, sphere.Radius, Vector4::One, RendererDebugPage::AttractorStats);

		// TEMP
		// Collect debug attractors.
		auto debugAttracPtrs = GetDebugAttractorPtrs();
		for (auto* attracPtr : debugAttracPtrs)
		{
			if (sphere.Intersects(attracPtr->GetBox()))
				nearbyAttracPtrs.push_back(attracPtr);
		}

		// Collect room attractors in neighbor rooms.
		auto& room = g_Level.Rooms[roomNumber];
		for (int neighborRoomNumber : room.neighbors)
		{
			auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			for (auto& attrac : neighborRoom.Attractors)
			{
				if (sphere.Intersects(attrac.GetBox()))
					nearbyAttracPtrs.push_back(&attrac);
			}
		}

		auto bridgeItemNumbers = std::set<int>{};

		// Collect bridge item numbers in neighbor sectors.
		auto sectorPtrs = GetNeighborSectorPtrs(pos, roomNumber, SECTOR_SEARCH_DEPTH);
		for (auto* sectorPtr : sectorPtrs)
		{
			for (int bridgeItemNumber : sectorPtr->BridgeItemNumbers)
				bridgeItemNumbers.insert(bridgeItemNumber);
		}

		// Collect bridge attractors.
		for (int bridgeItemNumber : bridgeItemNumbers)
		{
			auto& bridgeItem = g_Level.Items[bridgeItemNumber];
			//auto& bridge = GetBridgeObject(bridgeItem);

			auto& attrac = *bridgeItem.Attractor;//bridge.Attractor;
			if (sphere.Intersects(attrac.GetBox()))
				nearbyAttracPtrs.push_back(&attrac);
		}

		// Return pointers to approximately nearby attractors from sphere-AABB tests.
		return nearbyAttracPtrs;
	}

	std::vector<AttractorCollisionData> GetAttractorCollisions(const Vector3& pos, int roomNumber, short headingAngle, float detectRadius, const Vector3& axis)
	{
		constexpr auto COLL_COUNT_MAX = 64;

		// Get pointers to approximately nearby attractors.
		auto nearbyAttracPtrs = GetNearbyAttractorPtrs(pos, roomNumber, detectRadius);

		// Collect attractor collisions.
		auto attracColls = std::vector<AttractorCollisionData>{};
		attracColls.reserve(nearbyAttracPtrs.size());
		for (auto* attracPtr : nearbyAttracPtrs)
		{
			// Get collisions for every segment.
			for (int i = 0; i < attracPtr->GetSegmentCount(); i++)
			{
				auto attracColl = GetAttractorCollision(*attracPtr, i, pos, headingAngle, axis);

				// Filter out non-intersection.
				if (attracColl.Proximity.Distance3D > detectRadius)
					continue;

				attracColls.push_back(std::move(attracColl));
			}
		}

		// Sort collisions by 2D then 3D distance.
		std::sort(
			attracColls.begin(), attracColls.end(),
			[](const auto& attracColl0, const auto& attracColl1)
			{
				if (attracColl0.Proximity.Distance2D == attracColl1.Proximity.Distance2D)
					return (attracColl0.Proximity.Distance3D < attracColl1.Proximity.Distance3D);
				
				return (attracColl0.Proximity.Distance2D < attracColl1.Proximity.Distance2D);
			});

		// Trim collection.
		if (attracColls.size() > COLL_COUNT_MAX)
			attracColls.resize(COLL_COUNT_MAX);

		// Return attractor collisions in capped vector sorted by 2D then 3D distance.
		return attracColls;
	}

	std::vector<AttractorCollisionData> GetAttractorCollisions(const Vector3& pos, int roomNumber, short headingAngle,
															   float forward, float down, float right, float detectRadius, const Vector3& axis)
	{
		auto relOffset = Vector3(right, down, forward);
		auto rotMatrix = AxisAngle(axis, headingAngle).ToRotationMatrix();
		auto probePos = pos + Vector3::Transform(relOffset, rotMatrix);
		int probeRoomNumber = GetCollision(pos, roomNumber, headingAngle, forward, down, right).RoomNumber;

		return GetAttractorCollisions(probePos, probeRoomNumber, headingAngle, detectRadius);
	}

	void DrawNearbyAttractors(const ItemInfo& item)
	{
		constexpr auto DETECT_RADIUS = BLOCK(5);

		auto uniqueAttracPtrs = std::set<Attractor*>{};

		auto attracColls = GetAttractorCollisions(item.Pose.Position.ToVector3(), item.RoomNumber, item.Pose.Orientation.y, 0.0f, 0.0f, 0.0f, DETECT_RADIUS);
		for (const auto& attracColl : attracColls)
		{
			uniqueAttracPtrs.insert(attracColl.AttractorPtr);
			attracColl.AttractorPtr->DrawDebug(attracColl.Proximity.SegmentID);
		}

		if (g_Renderer.GetDebugPage() == RendererDebugPage::AttractorStats)
		{
			g_Renderer.PrintDebugMessage("Nearby attractors: %d", (int)uniqueAttracPtrs.size());
			g_Renderer.PrintDebugMessage("Nearby attractor segments: %d", (int)attracColls.size());
		}
	}
}
