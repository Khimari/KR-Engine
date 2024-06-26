#include "framework.h"
#include "Game/Animation/Commands.h"

#include "Game/camera.h"
#include "Game/control/flipeffect.h"
#include "Game/collision/collide_room.h"
#include "Game/items.h"
#include "Game/lara/lara.h"
#include "Game/lara/lara_helpers.h"
#include "Game/Setup.h"

namespace TEN::Animation
{
	void MoveOriginCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased)
			return;

		TranslateItem(&item, item.Pose.Orientation.y, RelOffset.z, RelOffset.y, RelOffset.x);

		if (item.IsLara())
		{
			// NOTE: GameBoundingBox constructor always clamps to last frame to avoid errors.
			auto bounds = GameBoundingBox(&item);
			UpdateLaraRoom(&item, -bounds.GetHeight() / 2, -RelOffset.x, -RelOffset.z);
		}
		else
		{
			UpdateItemRoom(item.Index);
		}
	}

	void JumpVelocityCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased)
			return;

		item.Animation.IsAirborne = true;
		item.Animation.Velocity = JumpVelocity;

		if (item.IsLara())
		{
			auto& player = GetLaraInfo(item);
			if (player.Context.CalcJumpVelocity != 0.0f)
			{
				item.Animation.Velocity.y = player.Context.CalcJumpVelocity;
				player.Context.CalcJumpVelocity = 0.0f;
			}
		}
	}

	void AttackReadyCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased || !item.IsLara())
			return;

		auto& player = GetLaraInfo(item);
		if (player.Control.HandStatus != HandStatus::Special)
			player.Control.HandStatus = HandStatus::Free;
	}

	void DeactivateCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased)
			return;

		const auto& object = Objects[item.ObjectNumber];
		if (object.intelligent && !item.AfterDeath)
			item.AfterDeath = 1;

		item.Status = ITEM_DEACTIVATED;
	}

	void SoundEffectCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (!isFrameBased || item.Animation.FrameNumber != FrameNumber)
			return;

		const auto& object = Objects[item.ObjectNumber];
		if (object.waterCreature)
		{
			SoundEffect(SoundID, &item.Pose, TestEnvironment(ENV_FLAG_WATER, &item) ? SoundEnvironment::Water : SoundEnvironment::Land);
			return;
		}

		if (item.IsLara())
		{
			const auto& player = GetLaraInfo(item);	
			if (EnvCondition == SoundEffectEnvCondition::Always ||
				(EnvCondition == SoundEffectEnvCondition::Land && (player.Context.WaterSurfaceDist >= -SHALLOW_WATER_DEPTH || player.Context.WaterSurfaceDist == NO_HEIGHT)) ||
				(EnvCondition == SoundEffectEnvCondition::ShallowWater && player.Context.WaterSurfaceDist < -SHALLOW_WATER_DEPTH && player.Context.WaterSurfaceDist != NO_HEIGHT && !TestEnvironment(ENV_FLAG_SWAMP, &item)))
			{
				SoundEffect(SoundID, &item.Pose, SoundEnvironment::Always);
			}
		}
		else
		{
			if (item.RoomNumber == NO_VALUE)
			{
				SoundEffect(SoundID, &item.Pose, SoundEnvironment::Always);
			}
			else if (TestEnvironment(ENV_FLAG_WATER, &item))
			{
				if (EnvCondition == SoundEffectEnvCondition::Always ||
					(EnvCondition == SoundEffectEnvCondition::ShallowWater && TestEnvironment(ENV_FLAG_WATER, Camera.pos.RoomNumber)))
				{
					SoundEffect(SoundID, &item.Pose, SoundEnvironment::Always);
				}
			}
			else if (EnvCondition == SoundEffectEnvCondition::Always ||
				(EnvCondition == SoundEffectEnvCondition::Land && !TestEnvironment(ENV_FLAG_WATER, Camera.pos.RoomNumber) && !TestEnvironment(ENV_FLAG_SWAMP, Camera.pos.RoomNumber)))
			{
				SoundEffect(SoundID, &item.Pose, SoundEnvironment::Always);
			}
		}
	}

	void FlipEffectCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (!isFrameBased || item.Animation.FrameNumber != FrameNumber)
			return;

		int flipEffectID = FlipEffectID & 0x3FFF; // Bits 1-14.
		DoFlipEffect(flipEffectID, &item);
	}
}
