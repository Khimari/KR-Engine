#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererLight
	{
		Vector3 Position;
		unsigned int Type;
		Vector3 Color;
		float Intensity;
		Vector3 Direction;
		float In;
		float Out;
		float InRange;
		float OutRange;
		
		BoundingSphere BoundingSphere;
		int RoomNumber;
		float LocalIntensity;
		float Distance;
		bool AffectNeighbourRooms;
		bool CastShadows;
		float Luma;
	};
}