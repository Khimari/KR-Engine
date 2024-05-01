#include "framework.h"
#include "Game/collision/Attractor.h"

#include "Game/camera.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Collision::Attractor
{
	AttractorObject::AttractorObject(AttractorType type, const std::vector<Vector3>& points, int roomNumber)
	{
		assertion(!points.empty(), "Attempted to initialize invalid attractor.");
		
		_type = type;
		_points = points;
		_roomNumber = roomNumber;
		Cache();
	}

	AttractorObject::~AttractorObject()
	{
		DetachAllPlayers();
	}

	AttractorType AttractorObject::GetType() const
	{
		return _type;
	}

	const std::vector<Vector3>& AttractorObject::GetPoints() const
	{
		return _points;
	}

	int AttractorObject::GetRoomNumber() const
	{
		return _roomNumber;
	}

	const std::vector<float>& AttractorObject::GetSegmentLengths() const
	{
		return _segmentLengths;
	}

	float AttractorObject::GetLength() const
	{
		return _length;
	}

	const BoundingBox& AttractorObject::GetBox() const
	{
		return _box;
	}

	bool AttractorObject::IsLooped() const
	{
		// Single segment exists; loop not possible.
		if (GetSegmentCount() == 1)
			return false;

		// Test if start and end points occupy same approximate position.
		float distSqr = Vector3::DistanceSquared(_points.front(), _points.back());
		return (distSqr <= EPSILON);
	}

	unsigned int AttractorObject::GetSegmentCount() const
	{
		return std::max<unsigned int>(_points.size() - 1, 1);
	}

	unsigned int AttractorObject::GetSegmentIDAtChainDistance(float chainDist) const
	{
		// Single segment exists; return segment ID 0.
		if (GetSegmentCount() == 1)
			return 0;

		// Normalize distance along attractor.
		chainDist = NormalizeChainDistance(chainDist);

		// Chain distance is on attractor edge; return clamped segment ID.
		if (chainDist <= 0.0f)
		{
			return 0;
		}
		else if (chainDist >= _length)
		{
			return (GetSegmentCount() - 1);
		}

		// Find segment at distance along attractor.
		float chainDistTraveled = 0.0f;
		for (int i = 0; i < GetSegmentCount(); i++)
		{
			// Accumulate distance traveled along attractor.
			chainDistTraveled += _segmentLengths[i];

			// Segment found; return segment ID.
			if (chainDistTraveled >= chainDist)
				return i;
		}

		// FAILSAFE: Return end segment ID.
		return (GetSegmentCount() - 1);
	}

	Vector3 AttractorObject::GetIntersectionAtChainDistance(float chainDist) const
	{
		// Single point exists; return simple intersection.
		if (_points.size() == 1)
			return _points.front();

		// Normalize distance along attractor.
		chainDist = NormalizeChainDistance(chainDist);

		// Line distance is outside attractor; return clamped intersection.
		if (chainDist <= 0.0f)
		{
			return _points.front();
		}
		else if (chainDist >= _length)
		{
			return _points.back();
		}

		// Find intersection at distance along attractor.
		float chainDistTraveled = 0.0f;
		for (int i = 0; i < GetSegmentCount(); i++)
		{
			float segmentLength = _segmentLengths[i];
			float remainingChainDist = chainDist - chainDistTraveled;

			// Found segment of distance along attractor; return intersection.
			if (remainingChainDist <= segmentLength)
			{
				// Get segment points.
				const auto& origin = _points[i];
				const auto& target = _points[i + 1];

				float alpha = remainingChainDist / segmentLength;
				return Vector3::Lerp(origin, target, alpha);
			}

			// Accumulate distance traveled along attractor.
			chainDistTraveled += segmentLength;
		}

		// FAILSAFE: Return end point.
		return _points.back();
	}

	void AttractorObject::Update(const std::vector<Vector3>& points, int roomNumber)
	{
		if (points.empty())
			TENLog("Attempted to update attractor to invalid state.", LogLevel::Warning);

		_points = points;
		_roomNumber = roomNumber;
		Cache();
	}

	void AttractorObject::AttachPlayer(ItemInfo& playerItem)
	{
		if (!playerItem.IsLara())
		{
			TENLog("Attempted to attach non-player item to attractor.", LogLevel::Warning);
			return;
		}

		_attachedPlayerItemNumbers.insert(playerItem.Index);
	}

	void AttractorObject::DetachPlayer(ItemInfo& playerItem)
	{
		_attachedPlayerItemNumbers.erase(playerItem.Index);
	}

	void AttractorObject::DetachAllPlayers()
	{
		// TODO: Crashes trying to loop?

		for (int itemNumber : _attachedPlayerItemNumbers)
		{
			auto& playerItem = g_Level.Items[itemNumber];
			auto& player = GetLaraInfo(playerItem);

			if (player.Context.Attractor.Ptr != nullptr)
				player.Context.Attractor.Detach(playerItem);
		}
	}

	void AttractorObject::DrawDebug(unsigned int segmentID) const
	{
		constexpr auto LABEL_OFFSET				= Vector3(0.0f, -CLICK(0.5f), 0.0f);
		constexpr auto INDICATOR_LINE_LENGTH	= BLOCK(1 / 20.0f);
		constexpr auto SPHERE_RADIUS			= BLOCK(1 / 52.0f);
		constexpr auto COLOR_YELLOW_OPAQUE		= Color(1.0f, 1.0f, 0.4f);
		constexpr auto COLOR_YELLOW_TRANSLUCENT = Color(1.0f, 1.0f, 0.4f, 0.3f);
		constexpr auto COLOR_GREEN				= Color(0.4f, 1.0f, 0.4f);

		auto getLabelScale = [](const Vector3& cameraPos, const Vector3& labelPos)
		{
			constexpr auto RANGE		   = BLOCK(5);
			constexpr auto LABEL_SCALE_MAX = 0.8f;
			constexpr auto LABEL_SCALE_MIN = 0.4f;

			float cameraDist = Vector3::Distance(cameraPos, labelPos);
			float alpha = cameraDist / RANGE;
			return Lerp(LABEL_SCALE_MAX, LABEL_SCALE_MIN, alpha);
		};

		// Determine label string.
		auto labelString = std::string();
		switch (_type)
		{
		case AttractorType::Edge:
			labelString = "Edge";
			break;

		case AttractorType::WallEdge:
			labelString = "Wall Edge";
			break;

		default:
			labelString = "Unknown attractor";
			break;
		}

		// Draw debug elements.
		if (_points.size() == 1)
		{
			// Draw sphere.
			g_Renderer.AddDebugSphere(_points.front(), SPHERE_RADIUS, COLOR_YELLOW_TRANSLUCENT, RendererDebugPage::AttractorStats, false);

			// Determine label parameters.
			auto labelPos = _points.front();
			auto labelPos2D = g_Renderer.Get2DPosition(labelPos);
			float labelScale = getLabelScale(Camera.pos.ToVector3(), labelPos);

			// Draw label.
			if (labelPos2D.has_value())
				g_Renderer.AddDebugString(labelString, *labelPos2D, COLOR_YELLOW_OPAQUE, labelScale, (int)PrintStringFlags::Outline, RendererDebugPage::AttractorStats);
		}
		else
		{
			// Get segment points.
			const auto& origin = _points[segmentID];
			const auto& target = _points[segmentID + 1];

			// Draw main line.
			g_Renderer.AddDebugLine(origin, target, COLOR_YELLOW_OPAQUE, RendererDebugPage::AttractorStats);

			auto orient = EulerAngles(0, Geometry::GetOrientToPoint(origin, target).y + ANGLE(90.0f), 0);
			auto dir = orient.ToDirection();

			// Draw segment heading indicator lines.
			g_Renderer.AddDebugLine(origin, Geometry::TranslatePoint(origin, dir, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);
			g_Renderer.AddDebugLine(target, Geometry::TranslatePoint(target, dir, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);

			// Determine label parameters.
			auto labelPos = ((origin + target) / 2) + LABEL_OFFSET;
			auto labelPos2D = g_Renderer.Get2DPosition(labelPos);
			float labelScale = getLabelScale(Camera.pos.ToVector3(), labelPos);

			// Draw label.
			if (labelPos2D.has_value())
				g_Renderer.AddDebugString(labelString, *labelPos2D, COLOR_YELLOW_OPAQUE, labelScale, 0, RendererDebugPage::AttractorStats);

			// Draw start indicator line.
			if (segmentID == 0)
				g_Renderer.AddDebugLine(origin, Geometry::TranslatePoint(origin, -Vector3::UnitY, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);
			
			// Draw end indicator line
			if (segmentID == (GetSegmentCount() - 1))
				g_Renderer.AddDebugLine(target, Geometry::TranslatePoint(target, -Vector3::UnitY, INDICATOR_LINE_LENGTH), COLOR_GREEN, RendererDebugPage::AttractorStats);

			// Draw AABB.
			//g_Renderer.AddDebugBox(_box, Vector4::One, RendererDebugPage::AttractorStats);
		}
	}

	void AttractorObject::DrawDebug() const
	{
		for (int i = 0; i < GetSegmentCount(); i++)
			DrawDebug(i);
	}

	float AttractorObject::NormalizeChainDistance(float chainDist) const
	{
		// Distance along attractor within bounds; return it.
		if (chainDist >= 0.0f && chainDist <= _length)
			return chainDist;

		// Wrap or clamp distance along attractor.
		return (IsLooped() ? fmod(chainDist + _length, _length) : std::clamp(chainDist, 0.0f, _length));
	}

	void AttractorObject::Cache()
	{
		_segmentLengths.clear();
		_length = 0.0f;

		// Cache lengths.
		if (_points.size() == 1)
		{
			_segmentLengths.push_back(0.0f);
		}
		else
		{
			for (int i = 0; i < GetSegmentCount(); i++)
			{
				const auto& origin = _points[i];
				const auto& target = _points[i + 1];

				float segmentLength = Vector3::Distance(origin, target);

				_segmentLengths.push_back(segmentLength);
				_length += segmentLength;
			}
		}

		// Cache box.
		_box = Geometry::GetBoundingBox(_points);
	}

	AttractorCollisionData::AttractorCollisionData(AttractorObject& attrac, unsigned int segmentID, const Vector3& pos, short headingAngle, const Vector3& axis)
	{
		constexpr auto HEADING_ANGLE_OFFSET = ANGLE(270.0f);

		// FAILSAFE: Clamp segment ID.
		unsigned int segmentCount = attrac.GetSegmentCount();
		if (segmentID >= segmentCount)
		{
			TENLog("Attempted to get attractor collision data for invalid segment ID " + std::to_string(segmentID) + ".", LogLevel::Warning);
			segmentID = std::clamp<unsigned int>(segmentID, 0, segmentCount - 1);
		}

		const auto& points = attrac.GetPoints();
		bool isChain = (points.size() > 1);

		// Calculate intersection.
		auto intersect = isChain ? Geometry::GetClosestPointOnLinePerp(pos, points[segmentID], points[segmentID + 1], axis) : points.front();

		// Calculate chain distance.
		float chainDist = 0.0f;
		if (isChain)
		{
			// Accumulate distance traveled along attractor toward intersection.
			for (unsigned int i = 0; i < segmentID; i++)
			{
				float segmentLength = attrac.GetSegmentLengths()[i];
				chainDist += segmentLength;
			}

			// Accumulate final distance traveled along attractor toward intersection.
			chainDist += Vector3::Distance(points[segmentID], intersect);
		}

		// Calculate orientations.
		auto refOrient = EulerAngles(0, headingAngle, 0);
		auto segmentOrient = isChain ? Geometry::GetOrientToPoint(points[segmentID], points[segmentID + 1]) : refOrient;

		// Set data.
		Attractor = &attrac;
		SegmentID = segmentID;
		Intersection = intersect;
		Distance2D = Vector2::Distance(Vector2(pos.x, pos.z), Vector2(intersect.x, intersect.z));
		Distance3D = Vector3::Distance(pos, intersect);
		ChainDistance = chainDist;
		HeadingAngle = segmentOrient.y + HEADING_ANGLE_OFFSET;
		SlopeAngle = segmentOrient.x;
		IsInFront = Geometry::IsPointInFront(pos, intersect, refOrient);
	}

	AttractorCollisionData GetAttractorCollision(AttractorObject& attrac, unsigned int segmentID, const Vector3& pos, short headingAngle,
												 const Vector3& axis)
	{
		return AttractorCollisionData(attrac, segmentID, pos, headingAngle, axis);
	}

	AttractorCollisionData GetAttractorCollision(AttractorObject& attrac, float chainDist, short headingAngle,
												 const Vector3& axis)
	{
		unsigned int segmentID = attrac.GetSegmentIDAtChainDistance(chainDist);
		auto pos = attrac.GetIntersectionAtChainDistance(chainDist);

		return AttractorCollisionData(attrac, segmentID, pos, headingAngle, axis);
	}
	
	// Debug
	static std::vector<AttractorObject*> GetDebugAttractors()
	{
		auto& player = GetLaraInfo(*LaraItem);

		auto debugAttracs = std::vector<AttractorObject*>{};
		debugAttracs.push_back(&*player.Context.DebugAttracs.Attrac0);
		debugAttracs.push_back(&*player.Context.DebugAttracs.Attrac1);
		debugAttracs.push_back(&*player.Context.DebugAttracs.Attrac2);

		for (auto& attrac : player.Context.DebugAttracs.Attracs)
			debugAttracs.push_back(&attrac);

		return debugAttracs;
	}

	// TODO: Spacial partitioning may be ideal here. Would require a general collision refactor. -- Sezz 2023.07.30
	static std::vector<AttractorObject*> GetNearbyAttractors(const Vector3& pos, int roomNumber, float radius)
	{
		constexpr auto SECTOR_SEARCH_DEPTH = 2;

		auto nearbyAttracs = std::vector<AttractorObject*>{};

		auto sphere = BoundingSphere(pos, radius);
		g_Renderer.AddDebugSphere(sphere.Center, sphere.Radius, Vector4::One, RendererDebugPage::AttractorStats);

		// TEMP
		// 1) Collect debug attractors.
		auto debugAttracs = GetDebugAttractors();
		for (auto* attrac : debugAttracs)
		{
			if (sphere.Intersects(attrac->GetBox()))
				nearbyAttracs.push_back(attrac);
		}

		// 2) Collect room attractors in neighbor rooms.
		auto& room = g_Level.Rooms[roomNumber];
		for (int neighborRoomNumber : room.neighbors)
		{
			auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			if (!neighborRoom.Active())
				continue;

			for (auto& attrac : neighborRoom.Attractors)
			{
				if (sphere.Intersects(attrac.GetBox()))
					nearbyAttracs.push_back(&attrac);
			}
		}

		// Collect bridge item numbers from neighbor sectors.
		auto bridgeItemNumbers = std::set<int>{};
		auto sectors = GetNeighborSectors(pos, roomNumber, SECTOR_SEARCH_DEPTH);
		for (const auto* sector : sectors)
		{
			for (int bridgeItemNumber : sector->BridgeItemNumbers)
				bridgeItemNumbers.insert(bridgeItemNumber);
		}

		// 3) Collect bridge attractors.
		for (int bridgeItemNumber : bridgeItemNumbers)
		{
			auto& bridgeItem = g_Level.Items[bridgeItemNumber];
			//auto& bridge = GetBridgeObject(bridgeItem);

			auto& attrac = *bridgeItem.Attractor;//bridge.Attractor;
			if (sphere.Intersects(attrac.GetBox()))
				nearbyAttracs.push_back(&attrac);
		}

		// Return pointers to approximately nearby attractors from sphere-AABB tests.
		return nearbyAttracs;
	}

	std::vector<AttractorCollisionData> GetAttractorCollisions(const Vector3& pos, int roomNumber, short headingAngle, float radius,
															   const Vector3& axis)
	{
		constexpr auto COLL_COUNT_MAX = 64;

		// Get approximately nearby attractors.
		auto attracs = GetNearbyAttractors(pos, roomNumber, radius);

		// Collect attractor collisions.
		auto attracColls = std::vector<AttractorCollisionData>{};
		attracColls.reserve(attracs.size());
		for (auto* attrac : attracs)
		{
			// Get collisions for every segment.
			for (int i = 0; i < attrac->GetSegmentCount(); i++)
			{
				auto attracColl = GetAttractorCollision(*attrac, i, pos, headingAngle, axis);

				// Filter out non-intersection.
				if (attracColl.Distance3D > radius)
					continue;

				attracColls.push_back(std::move(attracColl));
			}
		}

		// Sort collisions by 2D then 3D distance.
		std::sort(
			attracColls.begin(), attracColls.end(),
			[](const auto& attracColl0, const auto& attracColl1)
			{
				if (attracColl0.Distance2D == attracColl1.Distance2D)
					return (attracColl0.Distance3D < attracColl1.Distance3D);
				
				return (attracColl0.Distance2D < attracColl1.Distance2D);
			});

		// Trim collection.
		if (attracColls.size() > COLL_COUNT_MAX)
			attracColls.resize(COLL_COUNT_MAX);

		// Return attractor collisions in capped vector sorted by 2D then 3D distance.
		return attracColls;
	}

	std::vector<AttractorCollisionData> GetAttractorCollisions(const Vector3& pos, int roomNumber, short headingAngle,
															   float forward, float down, float right, float radius,
															   const Vector3& axis)
	{
		auto relOffset = Vector3(right, down, forward);
		auto rotMatrix = AxisAngle(axis, headingAngle).ToRotationMatrix();
		auto probePos = pos + Vector3::Transform(relOffset, rotMatrix);
		int probeRoomNumber = GetPointCollision(pos, roomNumber, headingAngle, forward, down, right).GetRoomNumber();

		return GetAttractorCollisions(probePos, probeRoomNumber, headingAngle, radius);
	}
	
	std::vector<Vector3> GetBridgeAttractorPoints(const ItemInfo& bridgeItem)
	{
		constexpr auto TILT_STEP = CLICK(1);

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

		// Get corners.
		auto corners = std::array<Vector3, 8>{};
		auto box = GameBoundingBox(&bridgeItem).ToBoundingOrientedBox(bridgeItem.Pose);
		box.GetCorners(corners.data());

		// Collect relevant points.
		auto offset = Vector3(0.0f, tiltOffset, 0.0f);
		return std::vector<Vector3>
		{
			corners[0],
			corners[4],
			corners[5] + offset,
			corners[1] + offset,
			corners[0]
		};
	}

	AttractorObject GenerateBridgeAttractor(const ItemInfo& bridgeItem)
	{
		auto points = GetBridgeAttractorPoints(bridgeItem);
		return AttractorObject(AttractorType::Edge, points, bridgeItem.RoomNumber);
	}

	void DrawNearbyAttractors(const Vector3& pos, int roomNumber, short headingAngle)
	{
		constexpr auto RADIUS = BLOCK(5);

		auto uniqueAttracs = std::set<AttractorObject*>{};

		auto attracColls = GetAttractorCollisions(pos, roomNumber, headingAngle, 0.0f, 0.0f, 0.0f, RADIUS);
		for (const auto& attracColl : attracColls)
		{
			uniqueAttracs.insert(attracColl.Attractor);
			attracColl.Attractor->DrawDebug(attracColl.SegmentID);
		}

		if (g_Renderer.GetDebugPage() == RendererDebugPage::AttractorStats)
		{
			g_Renderer.PrintDebugMessage("Nearby attractors: %d", (int)uniqueAttracs.size());
			g_Renderer.PrintDebugMessage("Nearby attractor segments: %d", (int)attracColls.size());
		}
	}
}
