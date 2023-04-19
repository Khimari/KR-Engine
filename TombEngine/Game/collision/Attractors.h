#pragma once

class EulerAngles;
struct CollisionResult;
struct ItemInfo;

namespace TEN::Collision::Attractors
{
	class Attractor;

	enum class AttractorType
	{
		Edge,
		/*VerticalPole,
		HorizontalPole,
		SwingPole,
		ZipLine,
		Tightrope,
		Pinnacle*/
	};

	struct AttractorProximityData
	{
		Vector3		 ClosestPoint	   = Vector3::Zero;
		float		 Distance		   = 0.0f;
		float		 DistanceAlongLine = 0.0f;
		unsigned int SegmentIndex	   = 0;
	};

	struct AttractorCollisionData
	{
		const Attractor* AttractorPtr = nullptr;

		AttractorProximityData Proximity	= {};
		short				   HeadingAngle = 0;
		short				   SlopeAngle	= 0;

		bool IsIntersected = false;
		bool IsInFront	   = false;
	};

	class Attractor
	{
	private:
		// Members
		AttractorType		 Type		= AttractorType::Edge;
		std::vector<Vector3> Points		= {};
		int					 RoomNumber = 0;
		float				 Length		= 0.0f;

	public:
		// Constructors
		Attractor() {};
		Attractor(AttractorType type, const std::vector<Vector3>& points, int roomNumber);

		// Getters
		AttractorType		 GetType() const;
		std::vector<Vector3> GetPoints() const;
		int					 GetRoomNumber() const;
		float				 GetLength() const;

		// Utilities
		AttractorCollisionData GetCollision(const Vector3& basePos, const EulerAngles& orient, const Vector3& refPoint, float range) const;
		AttractorProximityData GetProximityData(const Vector3& refPoint) const;
		Vector3				   GetPointAtDistance(float distAlongLine) const;
		float				   GetDistanceAtPoint(const Vector3& pointOnLine, unsigned int segmentIndex) const;

		// Inquirers
		bool IsEdge() const;

		// Helpers
		void Update(const std::vector<Vector3>& points, int roomNumber);
		void DrawDebug() const;
	};

	// Temp
	std::vector<const Attractor*>		GetDebugAttractorPtrs(const ItemInfo& item);
	std::vector<AttractorCollisionData> GetAttractorCollisions(const std::vector<const Attractor*>& attracPtrs,
															   const ItemInfo& item, const Vector3& refPoint, float range);

	std::vector<AttractorCollisionData> GetAttractorCollisions(const Vector3& basePos, int roomNumber, const EulerAngles& orient,
															   const Vector3& refPoint, float range);
	std::vector<AttractorCollisionData> GetAttractorCollisions(const ItemInfo& item, const Vector3& refPoint, float range);

	Attractor				 GenerateAttractorFromPoints(std::vector<Vector3> points, int roomNumber, AttractorType type, bool isClosedLoop = true);
	std::optional<Attractor> GenerateSectorAttractor(const CollisionResult& pointColl);
}
