#pragma once
#include <vector>
#include <SimpleMath.h>
#include "Renderer/RendererEnums.h"
#include "RendererPolygon.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererBucket
	{
		int Texture;
		bool Animated;
		BLEND_MODES BlendMode;
		int StartVertex;
		int StartIndex;
		int NumVertices;
		int NumIndices;
		Vector3 Centre;
		std::vector<RendererPolygon> Polygons;
	};
}
