#include "framework.h"
#include "Objects/Effects/LensFlare.h"

#include "Game/camera.h"
#include "Game/control/los.h"
#include "Specific/level.h"

namespace TEN::Entities::Effects
{
	std::vector<LensFlare> LensFlares;

	static void SetupLensFlare(const Vector3& pos, int roomNumber, const Color& color, bool isGlobal, int spriteIndex)
	{
		auto lensFlarePos = Vector3::Zero;
		if (isGlobal)
		{
			if (TestEnvironment(ENV_FLAG_NO_LENSFLARE, Camera.pos.RoomNumber))
				return;

			lensFlarePos = pos;
			auto delta = (lensFlarePos - Camera.pos.ToVector3()) / 16.0f;
			while (abs(delta.x) > BLOCK(200) || abs(delta.y) > BLOCK(200) || abs(delta.z) > BLOCK(200))
				lensFlarePos -= delta;

			delta = (lensFlarePos - Camera.pos.ToVector3()) / 16.0f;
			while (abs(delta.x) > BLOCK(32) || abs(delta.y) > BLOCK(32) || abs(delta.z) > BLOCK(32))
				lensFlarePos -= delta;

			delta = (lensFlarePos - Camera.pos.ToVector3()) / 16.0f;
			for (int i = 0; i < 16; i++)
			{
				int foundRoomNumber = IsRoomOutside(lensFlarePos.x, lensFlarePos.y, lensFlarePos.z);
				if (foundRoomNumber != NO_VALUE)
				{
					roomNumber = foundRoomNumber;
					break;
				}

				lensFlarePos -= delta;
			}
		}
		else
		{
			float dist = Vector3::Distance(pos, Camera.pos.ToVector3());
			if (dist > BLOCK(32))
				return;

			lensFlarePos = pos;
		}

		bool isVisible = false;
		if (roomNumber != NO_VALUE)
		{
			if (TestEnvironment(ENV_FLAG_NOT_NEAR_OUTSIDE, roomNumber) || !isGlobal)
			{
				auto origin = Camera.pos;
				auto target = GameVector(lensFlarePos, roomNumber);
				isVisible = LOS(&origin, &target);
			}
		}

		if (!isVisible && !isGlobal)
			return;

		auto lensFlare = LensFlare{};
		lensFlare.Position = pos;
		lensFlare.RoomNumber = roomNumber;
		lensFlare.IsGlobal = isGlobal;
		lensFlare.Color = color;
		lensFlare.SpriteIndex = spriteIndex;

		LensFlares.push_back(lensFlare);
	}

	void ControlLensFlare(int itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
			SetupLensFlare(item.Pose.Position.ToVector3(), item.RoomNumber, Color(), false, SPR_LENS_FLARE_3);
	}

	void ClearLensFlares()
	{
		LensFlares.clear();
	}

	void SetupGlobalLensFlare(const Vector2& yawAndPitchInDegrees, const Color& color, int spriteIndex)
	{
		auto pos = Camera.pos.ToVector3();
		auto rotMatrix = Matrix::CreateFromYawPitchRoll(
			DEG_TO_RAD(yawAndPitchInDegrees.x), 
			DEG_TO_RAD(yawAndPitchInDegrees.y), 
			0.0f);

		pos += Vector3::Transform(Vector3(0.0f, 0.0f, BLOCK(256)), rotMatrix);
		SetupLensFlare(pos, NO_VALUE, color, true, spriteIndex);
	}
}
