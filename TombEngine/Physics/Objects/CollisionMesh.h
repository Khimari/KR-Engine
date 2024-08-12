#pragma once

#include "Math/Math.h"
#include "Specific/Structures/BoundingTree.h"

using namespace TEN::Math;
using namespace TEN::Structures;

namespace TEN::Physics
{
	class CollisionTriangle
	{
	public:
		static constexpr auto VERTEX_COUNT = 3;

	private:
		// Members

		std::array<int, VERTEX_COUNT> _vertexIds		= {};
		int							  _normalID			= 0;
		BoundingBox					  _aabb				= BoundingBox();
		int							  _portalRoomNumber = NO_VALUE;

	public:
		// Constructors

		CollisionTriangle(int vertex0ID, int vertex1ID, int vertex2ID, int normalID, const BoundingBox& aabb, int portalRoomNumber);

		// Getters

		const Vector3&	   GetVertex0(const std::vector<Vector3>& vertices) const;
		const Vector3&	   GetVertex1(const std::vector<Vector3>& vertices) const;
		const Vector3&	   GetVertex2(const std::vector<Vector3>& vertices) const;
		const Vector3&	   GetNormal(const std::vector<Vector3>& normals) const;
		const BoundingBox& GetAabb() const;
		int				   GetPortalRoomNumber() const;

		Vector3 GetTangent(const BoundingSphere& sphere, const std::vector<Vector3>& vertices, const std::vector<Vector3>& normals) const;

		// Inquirers

		bool Intersects(const Ray& ray, float& dist, const std::vector<Vector3>& vertices, const std::vector<Vector3>& normals) const;
		bool Intersects(const BoundingSphere& sphere, const std::vector<Vector3>& vertices, const std::vector<Vector3>& normals) const;
		bool IsPortal() const;

		// Debug

		void DrawDebug(const std::vector<Vector3>& vertices, const std::vector<Vector3>& normals) const;
	};

	struct CollisionTriangleData
	{
		std::array<Vector3, CollisionTriangle::VERTEX_COUNT> Vertices = {};
		Vector3		Normal			 = Vector3::Zero;
		BoundingBox Aabb			 = BoundingBox();
		int			PortalRoomNumber = NO_VALUE;
	};

	struct CollisionMeshRayCollisionData
	{
		CollisionTriangleData Triangle = {};
		float Distance = 0.0f;
	};
	
	struct CollisionMeshSphereCollisionData
	{
		std::vector<CollisionTriangleData> Triangles = {};
		std::vector<Vector3>			   Tangents	 = {};

		unsigned int Count = 0;
	};

	// TODO: Relative transform.
	class CollisionMesh
	{
	private:
		struct Cache
		{
			std::unordered_map<Vector3, int> VertexMap = {}; // Key = vertex, value = vertex ID.
			std::unordered_map<Vector3, int> NormalMap = {}; // Key = normal, value = normal ID.
		};

		// Members

		std::vector<CollisionTriangle> _triangles = {};
		std::vector<Vector3>		   _vertices  = {};
		std::vector<Vector3>		   _normals	  = {};

		BoundingTree _triangleTree = BoundingTree();
		Cache		 _cache		   = {};

	public:
		// Constructors

		CollisionMesh() = default;

		// Getters

		std::optional<CollisionMeshRayCollisionData>	GetCollision(const Ray& ray, float dist) const;
		std::optional<CollisionMeshSphereCollisionData> GetCollision(const BoundingSphere& sphere) const;

		// Utilities

		void InsertTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& normal, int portalRoomNumber = NO_VALUE);
		void Initialize();

		// Debug

		void DrawDebug() const;
	};
}
