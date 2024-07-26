#pragma once

#include "Objects/game_object_ids.h"

namespace TEN::Effects::Ripple
{
	enum class RippleFlags
	{
		SlowFade   = 1 << 0,
		LowOpacity = 1 << 1,
		OnGround   = 1 << 2
	};

	struct Ripple
	{
		GAME_OBJECT_ID SpriteSeqAssetID = GAME_OBJECT_ID::ID_DEFAULT_SPRITES;
		int			   SpriteID	   = 0;

		Vector3 Position   = Vector3::Zero;
		int		RoomNumber = 0;
		Vector3 Normal	   = Vector3::Zero;
		Vector4 Color	   = Vector4::Zero;

		float Life		   = 0.0f;
		float LifeMax	   = 0.0f;
		float Size		   = 0.0f;
		float FadeDuration = 0.0f;
		int	  Flags		   = 0;
	};

	extern std::vector<Ripple> Ripples;

	void SpawnRipple(const Vector3& pos, int roomNumber, float size, int flags = 0, const Vector3& normal = Vector3::Down);

	void UpdateRipples();
	void ClearRipples();
}