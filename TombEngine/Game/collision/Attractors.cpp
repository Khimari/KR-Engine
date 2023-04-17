#include "framework.h"
#include "Game/collision/Attractors.h"

#include "Game/collision/collide_room.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "lara_helpers.h"
#include "Math/Math.h"
#include "Renderer/Renderer11.h"
#include "Specific/level.h"

using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Collision::Attractors
{
	Attractor::Attractor(AttractorType type, const std::vector<Vector3>& points, int roomNumber)
	{
		Type = type;
		Points = points;
		RoomNumber = roomNumber;

		if (points.size() > 1)
		{
			for (int i = 0; i < (points.size() - 1); i++)
				Length += Vector3::Distance(points[i], points[i + 1]);
		}
	}

	AttractorType Attractor::GetType() const
	{
		return Type;
	}

	std::vector<Vector3> Attractor::GetPoints() const
	{
		return Points;
	}

	int Attractor::GetRoomNumber() const
	{
		return RoomNumber;
	}

	Vector3 Attractor::GetPointAtDistance(float dist) const
	{
		if (Points.empty())
			return Vector3::Zero;

		if (dist < 0.0f)
		{
			return Points[0];
		}
		else if (dist > Length)
		{
			return Points.back();
		}

		auto point = Points[0];
		float currentDist = 0.0f;

		for (int i = 0; i < (Points.size() - 1); i++)
		{
			float segmentLength = Vector3::Distance(Points[i], Points[i + 1]);
			currentDist += segmentLength;

			if (currentDist > dist)
			{
				float segmentDist = currentDist - dist;
				auto direction = Points[i + 1] - Points[i];
				direction.Normalize();
				point = Points[i] + (direction * segmentDist);
				break;
			}
		}

		return point;
	}

	float Attractor::GetLength() const
	{
		return Length;
	}

	bool Attractor::IsEdge() const
	{
		return (Type == AttractorType::Edge);
	}

	void Attractor::DrawDebug() const
	{
		constexpr auto LABEL_SCALE			 = 0.75f;
		constexpr auto LABEL_OFFSET			 = Vector3(0.0f, -CLICK(0.25f), 0.0f);
		constexpr auto INDICATOR_LINE_LENGTH = 50.0f;
		constexpr auto SPHERE_SCALE			 = 15.0f;
		constexpr auto COLOR_GREEN			 = Vector4(0.0f, 1.0f, 0.0f, 1.0f);
		constexpr auto COLOR_YELLOW			 = Vector4(1.0f, 1.0f, 0.0f, 1.0f);

		auto labelString = std::string();
		switch (Type)
		{
		default:
			labelString = "Attractor";
			break;

		case AttractorType::Edge:
			labelString = "Edge";
			break;
		}

		if (Points.size() > 1)
		{
			for (int i = 0; i < (Points.size() - 1); i++)
			{
				auto orient = Geometry::GetOrientToPoint(Points[i], Points[i + 1]);
				orient.y += ANGLE(90.0f);
				auto direction = orient.ToDirection();

				auto labelPos = ((Points[i] + Points[i + 1]) / 2) + LABEL_OFFSET;
				auto labelPos2D = g_Renderer.GetScreenSpacePosition(labelPos);

				// Draw main line.
				g_Renderer.AddLine3D(Points[i], Points[i + 1], COLOR_YELLOW);

				// Draw indicator lines.
				g_Renderer.AddLine3D(Points[i], Points[i] + (direction * INDICATOR_LINE_LENGTH), COLOR_GREEN);
				g_Renderer.AddLine3D(Points[i + 1], Points[i + 1] + (direction * INDICATOR_LINE_LENGTH), COLOR_GREEN);

				// Draw attractor label.
				g_Renderer.AddString(labelString, labelPos2D, Color(PRINTSTRING_COLOR_WHITE), LABEL_SCALE, 0);
			}
		}
		else
		{
			// Draw single sphere.
			if (!Points.empty())
				g_Renderer.AddSphere(Points[0], SPHERE_SCALE, COLOR_YELLOW);
		}
	}

	Attractor& Attractor::operator =(const Attractor& attrac)
	{
		Type = attrac.GetType();
		Points = attrac.GetPoints();
		RoomNumber = attrac.GetRoomNumber();
		Length = attrac.GetLength();
		return *this;
	}

	static AttractorTargetData GetAttractorTargetData(const Attractor& attrac, const Vector3& refPoint)
	{
		auto points = attrac.GetPoints();

		int segmentIndex = 0;
		auto targetPoint = points[0];
		float closestDist = INFINITY;

		for (int i = 0; i < (points.size() - 1); i++)
		{
			auto closestPoint = Geometry::GetClosestPointOnLinePerp(refPoint, points[i], points[i + 1]);
			float distance = Vector3::Distance(refPoint, closestPoint);

			if (distance < closestDist)
			{
				segmentIndex = i;
				targetPoint = closestPoint;
				closestDist = distance;
			}
		}

		// Return attractor target.
		return AttractorTargetData{ targetPoint, closestDist, segmentIndex };
	}

	static float GetDistanceAlongAttractor(const Attractor& attrac, const Vector3& targetPoint, int segmentIndex)
	{
		auto points = attrac.GetPoints();
		assertion(segmentIndex < points.size(), "Attractor segment index out of range.");

		// Calculate distance along attractor to segment point.
		float distance = 0.0f;
		for (int i = 0; i <= segmentIndex; i++)
		{
			if (i != segmentIndex)
			{
				distance += Vector3::Distance(points[i], points[i + 1]);
				continue;
			}

			distance += Vector3::Distance(points[i], targetPoint);
		}

		return distance;
	}

	std::vector<const Attractor*> GetNearbyAttractorPtrs(const Vector3& refPoint, int roomNumber, float range)
	{
		constexpr auto COUNT_MAX = 32;

		auto nearbyAttracPtrMap = std::multimap<float, const Attractor*>{};

		// Assess attractors in current room.
		const auto& room = g_Level.Rooms[roomNumber];
		for (const auto& attrac : room.Attractors)
		{
			auto attracTarget = GetAttractorTargetData(attrac, refPoint);
			if (attracTarget.Distance <= range)
				nearbyAttracPtrMap.insert({ attracTarget.Distance, &attrac });
		}

		// Assess attractors in neighboring rooms (search depth of 2).
		for (const int& subRoomNumber : room.neighbors)
		{
			const auto& subRoom = g_Level.Rooms[subRoomNumber];
			for (const auto& attrac : subRoom.Attractors)
			{
				auto attracTarget = GetAttractorTargetData(attrac, refPoint);
				if (attracTarget.Distance <= range)
					nearbyAttracPtrMap.insert({ attracTarget.Distance, &attrac });
			}
		}

		// TODO
		// Assess bridge attractors.
		/*for (const auto& [bridgeItemNumber, attracs] : g_Level.BridgeAttractors)
		{
			const auto& bridgeItem = g_Level.Items[bridgeItemNumber];

			if (Vector3::DistanceSquared(refPoint, bridgeItem.Pose.Position.ToVector3()) > range)
				continue;
		}*/

		auto nearbyAttracPtrs = std::vector<const Attractor*>{};
		nearbyAttracPtrs.reserve(COUNT_MAX);

		// Move elements pointer into vector.
		auto it = nearbyAttracPtrMap.begin();
		for (int i = 0; i < COUNT_MAX && it != nearbyAttracPtrMap.end(); i++, it++)
			nearbyAttracPtrs.push_back(std::move(it->second));

		return nearbyAttracPtrs;
	}

	std::vector<const Attractor*> GetNearbyAttractorPtrs(const ItemInfo& item)
	{
		constexpr auto RANGE	 = BLOCK(5);
		constexpr auto COUNT_MAX = 64;

		auto& player = GetLaraInfo(item);

		auto nearbyAttracPtrs = std::vector<const Attractor*>{};
		for (auto& attrac : player.Context.Attractor.SectorAttractors)
		{
			assertion(nearbyAttracPtrs.size() <= COUNT_MAX, "Nearby attractor pointer collection overflow.");
			if (nearbyAttracPtrs.size() == COUNT_MAX)
				return nearbyAttracPtrs;

			nearbyAttracPtrs.push_back(&attrac);
			attrac.DrawDebug();
		}
		
		nearbyAttracPtrs.push_back(&player.Context.Attractor.DebugAttractor0);
		/*nearbyAttracPtrs.push_back(&player.Context.Attractor.DebugAttractor1);
		nearbyAttracPtrs.push_back(&player.Context.Attractor.DebugAttractor2);
		nearbyAttracPtrs.push_back(&player.Context.Attractor.DebugAttractor3);
		nearbyAttracPtrs.push_back(&player.Context.Attractor.DebugAttractor4);*/
		return nearbyAttracPtrs;
	}

	static AttractorCollisionData GetAttractorCollision(const Attractor& attrac, const Vector3& basePos, const EulerAngles& orient,
														const Vector3& refPoint, float range)
	{
		static const auto EMPTY_ATTRAC_COLL = AttractorCollisionData{};

		auto points = attrac.GetPoints();
		if (points.empty())
			return EMPTY_ATTRAC_COLL;

		auto attracTarget = GetAttractorTargetData(attrac, refPoint);
		float distFromStart = GetDistanceAlongAttractor(attrac, attracTarget.TargetPoint, attracTarget.SegmentIndex);

		// Calculate angles.
		auto attracOrient = Geometry::GetOrientToPoint(points[attracTarget.SegmentIndex], points[attracTarget.SegmentIndex + 1]);
		short headingAngle = attracOrient.y - ANGLE(90.0f);
		short slopeAngle = attracOrient.x;

		// Determine inquiries.
		bool isIntersected = (attracTarget.Distance <= range);
		bool isInFront = Geometry::IsPointInFront(basePos, attracTarget.TargetPoint, orient);

		// Create new attractor collision.
		auto attracColl = EMPTY_ATTRAC_COLL;

		attracColl.AttractorPtr = &attrac;
		attracColl.TargetPoint = attracTarget.TargetPoint;
		attracColl.Distance = attracTarget.Distance;
		attracColl.DistanceFromStart = distFromStart;
		attracColl.HeadingAngle = headingAngle;
		attracColl.SlopeAngle = slopeAngle;
		attracColl.IsIntersected = isIntersected;
		attracColl.IsInFront = isInFront;

		return attracColl;
	}
	
	std::vector<AttractorCollisionData> GetAttractorCollisions(const std::vector<const Attractor*>& attracPtrs,
															   const Vector3& basePos, const EulerAngles& orient,
															   const Vector3& refPoint, float range)
	{
		auto attracColls = std::vector<AttractorCollisionData>{};
		attracColls.reserve(attracPtrs.size());

		for (const auto* attrac : attracPtrs)
		{
			auto attracColl = GetAttractorCollision(*attrac, basePos, orient, refPoint, range);
			attracColls.push_back(attracColl);
		}

		return attracColls;
	}

	std::vector<AttractorCollisionData> GetAttractorCollisions(const std::vector<const Attractor*>& attracPtrs,
															   const ItemInfo& item, const Vector3& refPoint, float range)
	{
		return GetAttractorCollisions(attracPtrs, item.Pose.Position.ToVector3(), item.Pose.Orientation, refPoint, range);
	}

	Attractor GenerateAttractorFromPoints(std::vector<Vector3> points, int roomNumber, AttractorType type, bool isClosedLoop)
	{
		static const auto DEFAULT_ATTRACTOR = Attractor{};

		// No points; return default.
		if (points.empty())
			return DEFAULT_ATTRACTOR;

		// Add point to create loop (if applicable).
		if (isClosedLoop)
			points.push_back(points[0]);

		// Generate attractor.
		return Attractor(type, points, roomNumber);
	}

	static Attractor GenerateBridgeAttractor(const ItemInfo& bridgeItem)
	{
		constexpr auto TILT_STEP = CLICK(1);

		// Get bounding box.
		auto box = GameBoundingBox(&bridgeItem).ToBoundingOrientedBox(bridgeItem.Pose);

		// Determine tilt offset.
		int tiltOffset = 0;
		switch (bridgeItem.ObjectNumber)
		{
		default:
		case ID_BRIDGE_FLAT:
			break;

		case ID_BRIDGE_TILT1:
			tiltOffset = TILT_STEP;
			break;

		case ID_BRIDGE_TILT2:
			tiltOffset = TILT_STEP * 2;
			break;

		case ID_BRIDGE_TILT3:
			tiltOffset = TILT_STEP * 3;
			break;

		case ID_BRIDGE_TILT4:
			tiltOffset = TILT_STEP * 4;
			break;
		}

		// Determine relative corner points.
		auto point0 = Vector3(box.Extents.x, -box.Extents.y + tiltOffset, box.Extents.z);
		auto point1 = Vector3(-box.Extents.x, -box.Extents.y, box.Extents.z);
		auto point2 = Vector3(-box.Extents.x, -box.Extents.y, -box.Extents.z);
		auto point3 = Vector3(box.Extents.x, -box.Extents.y + tiltOffset, -box.Extents.z);

		// Calculate absolute corner points.
		auto rotMatrix = Matrix::CreateFromQuaternion(box.Orientation);
		auto points = std::vector<Vector3>
		{
			box.Center + Vector3::Transform(point0, rotMatrix),
			box.Center + Vector3::Transform(point1, rotMatrix),
			box.Center + Vector3::Transform(point2, rotMatrix),
			box.Center + Vector3::Transform(point3, rotMatrix)
		};

		// Generate and return attractor.
		return GenerateAttractorFromPoints(points, bridgeItem.RoomNumber, AttractorType::Edge);
	}

	std::optional<Attractor> GenerateSectorAttractor(const CollisionResult& pointColl)
	{
		// Invalid sector; return nullopt.
		if (pointColl.Position.Floor == NO_HEIGHT)
			return std::nullopt;

		// Generate and return bridge attractor.
		if (pointColl.Position.Bridge >= 0)
		{
			const auto& bridgeItem = g_Level.Items[pointColl.Position.Bridge];
			return GenerateBridgeAttractor(bridgeItem);
		}

		// Generate and return floor attractor.
		auto points = pointColl.BottomBlock->GetSurfaceVertices(pointColl.Coordinates.x, pointColl.Coordinates.z, true);
		return GenerateAttractorFromPoints(points, pointColl.RoomNumber, AttractorType::Edge);
	}
}
