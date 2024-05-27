#include "framework.h"
#include "Objects/Generic/Object/BridgeObject.h"

//#include "Game/collision/Attractor.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/Pushable/PushableInfo.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Physics/Physics.h"
#include "Specific/level.h"

//using namespace TEN::Colllision::Attractor;
using namespace TEN::Math;
using namespace TEN::Physics;

namespace TEN::Entities::Generic
{
	const BoundingBox& BridgeObject::GetBox() const
	{
		return _box;
	}

	const CollisionMesh& BridgeObject::GetCollisionMesh() const
	{
		return _collisionMesh;
	}

	/*const Attractor& BridgeObject::GetAttractor() const
	{
		reutrn _attractor;
	}*/

	void BridgeObject::Initialize(const ItemInfo& item)
	{
		UpdateBox(item);
		UpdateCollisionMesh(item);
		//InitializeAttractor(item);
		UpdateSectors(item);

		_prevPose = item.Pose;
		_prevRoomNumber = item.RoomNumber;
	}

	void BridgeObject::Update(const ItemInfo& item)
	{
		// Bridge not moved; return early.
		if (item.Pose == _prevPose && item.RoomNumber == _prevRoomNumber)
			return;

		UpdateItemRoom(item.Index);
		UpdateBox(item);
		UpdateCollisionMesh(item);
		//UpdateAttractor(item);
		UpdateSectors(item);

		_prevPose = item.Pose;
		_prevRoomNumber = item.RoomNumber;
	}

	void BridgeObject::RemoveFromSectors(const ItemInfo& item) const
	{
		auto& room = g_Level.Rooms[item.RoomNumber];
		for (int neighborRoomNumber : room.neighbors)
		{
			auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			for (auto& sector : neighborRoom.floor)
				sector.RemoveBridge(item.Index);
		}
	}

	void BridgeObject::UpdateBox(const ItemInfo& item)
	{
		_box = Geometry::GetBoundingBox(item.GetBox());
	}

	void BridgeObject::UpdateCollisionMesh(const ItemInfo& item)
	{
		constexpr auto UP_NORMAL	  = Vector3(0.0f, -1.0f, 0.0f);
		constexpr auto DOWN_NORMAL	  = Vector3(0.0f, 1.0f, 0.0f);
		constexpr auto FORWARD_NORMAL = Vector3(0.0f, 0.0f, 1.0f);
		constexpr auto BACK_NORMAL	  = Vector3(0.0f, 0.0f, -1.0f);
		constexpr auto RIGHT_NORMAL	  = Vector3(1.0f, 0.0f, 0.0f);
		constexpr auto LEFT_NORMAL	  = Vector3(-1.0f, 0.0f, 0.0f);

		// Determine relative tilt offset.
		auto offset = Vector3::Zero;
		auto tiltRotMatrix = Matrix::Identity;
		switch (item.ObjectNumber)
		{
		case ID_BRIDGE_TILT1:
			offset = Vector3(0.0f, CLICK(1), 0.0f);
			tiltRotMatrix = EulerAngles(0, 0, ANGLE(-45.0f * 0.25f)).ToRotationMatrix();
			break;

		case ID_BRIDGE_TILT2:
			offset = Vector3(0.0f, CLICK(2), 0.0f);
			tiltRotMatrix = EulerAngles(0, 0, ANGLE(-45.0f * 0.5f)).ToRotationMatrix();
			break;

		case ID_BRIDGE_TILT3:
			offset = Vector3(0.0f, CLICK(3), 0.0f);
			tiltRotMatrix = EulerAngles(0, 0, ANGLE(-45.0f * 0.75f)).ToRotationMatrix();
			break;

		case ID_BRIDGE_TILT4:
			offset = Vector3(0.0f, CLICK(4), 0.0f);
			tiltRotMatrix = EulerAngles(0, 0, ANGLE(-45.0f)).ToRotationMatrix();
			break;

		default:
			break;
		}

		// Calculate absolute tilt offset.
		auto rotMatrix = item.Pose.Orientation.ToRotationMatrix();
		offset = Vector3::Transform(offset, rotMatrix);

		// Get box corners.
		auto box = item.GetBox();
		auto corners = std::array<Vector3, BoundingOrientedBox::CORNER_COUNT>{};
		box.GetCorners(corners.data());

		// Offset key corners.
		corners[1] += offset;
		corners[3] -= offset;
		corners[5] += offset;
		corners[7] -= offset;
		
		// Set collision mesh.
		_collisionMesh = CollisionMesh();
		_collisionMesh.InsertTriangle(corners[0], corners[1], corners[4], Vector3::Transform(UP_NORMAL, tiltRotMatrix * rotMatrix));
		_collisionMesh.InsertTriangle(corners[1], corners[4], corners[5], Vector3::Transform(UP_NORMAL, tiltRotMatrix * rotMatrix));
		_collisionMesh.InsertTriangle(corners[2], corners[3], corners[6], Vector3::Transform(DOWN_NORMAL, tiltRotMatrix * rotMatrix));
		_collisionMesh.InsertTriangle(corners[3], corners[6], corners[7], Vector3::Transform(DOWN_NORMAL, tiltRotMatrix * rotMatrix));
		_collisionMesh.InsertTriangle(corners[4], corners[5], corners[6], Vector3::Transform(FORWARD_NORMAL, rotMatrix));
		_collisionMesh.InsertTriangle(corners[4], corners[6], corners[7], Vector3::Transform(FORWARD_NORMAL, rotMatrix));
		_collisionMesh.InsertTriangle(corners[0], corners[1], corners[2], Vector3::Transform(BACK_NORMAL, rotMatrix));
		_collisionMesh.InsertTriangle(corners[0], corners[2], corners[3], Vector3::Transform(BACK_NORMAL, rotMatrix));
		_collisionMesh.InsertTriangle(corners[1], corners[2], corners[5], Vector3::Transform(LEFT_NORMAL, rotMatrix));
		_collisionMesh.InsertTriangle(corners[2], corners[5], corners[6], Vector3::Transform(LEFT_NORMAL, rotMatrix));
		_collisionMesh.InsertTriangle(corners[0], corners[3], corners[4], Vector3::Transform(RIGHT_NORMAL, rotMatrix));
		_collisionMesh.InsertTriangle(corners[3], corners[4], corners[7], Vector3::Transform(RIGHT_NORMAL, rotMatrix));
		_collisionMesh.GenerateBvh();
	}

	/*void BridgeObject::UpdateAttractor(const ItemInfo& item)
	{
	}*/

	void BridgeObject::UpdateSectors(const ItemInfo& item)
	{
		// CLear bridge assignment if killed.
		if (item.Flags & IFLAG_KILLED)
		{
			RemoveFromSectors(item);
			return;
		}

		// Get bridge box.
		auto box = item.GetBox();

		// Update neighbor room sectors.
		auto& room = g_Level.Rooms[item.RoomNumber];
		for (int neighborRoomNumber : room.neighbors)
		{
			auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			for (auto& sector : neighborRoom.floor)
			{
				// Clear previous bridge assignment.
				sector.RemoveBridge(item.Index);

				// Test if bridge AABB intersects sector.
				if (!_box.Intersects(sector.Box))
					continue;

				// Add bridge if within sector.
				if (box.Intersects(sector.Box))
					sector.AddBridge(item.Index);
			}
		}
	}

	const BridgeObject& GetBridgeObject(const ItemInfo& item)
	{
		// HACK: Pushable bridges.
		if (item.Data.is<PushableInfo>())
		{
			const auto& pushable = GetPushableInfo(item);
			return pushable.Bridge;
		}

		return (BridgeObject&)item.Data;
	}

	BridgeObject& GetBridgeObject(ItemInfo& item)
	{
		// HACK: Pushable bridges.
		if (item.Data.is<PushableInfo>())
		{
			auto& pushable = GetPushableInfo(item);
			return pushable.Bridge;
		}

		return (BridgeObject&)item.Data;
	}
}
