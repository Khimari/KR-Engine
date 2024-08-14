#include "Game/effects/DisplaySprite.h"

#include "Game/effects/effects.h"
#include "Game/Setup.h"
#include "Objects/objectslist.h"
#include "Renderer/Renderer.h"

namespace TEN::Effects::DisplaySprite
{
	std::vector<DisplaySprite> DisplaySprites = {};

	void AddDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vector2& pos, short orient, const Vector2& scale, const Vector4& color,
						  int priority, DisplaySpriteAlignMode alignMode, DisplaySpriteScaleMode scaleMode, BlendMode blendMode)
	{
		auto displaySprite = DisplaySprite{};
		displaySprite.ObjectID = objectID;
		displaySprite.SpriteID = spriteID;
		displaySprite.Position = pos;
		displaySprite.Orientation = orient;
		displaySprite.Scale = scale;
		displaySprite.Color = color;
		displaySprite.Priority = priority;
		displaySprite.AlignMode = alignMode;
		displaySprite.ScaleMode = scaleMode;
		displaySprite.BlendMode = blendMode;

		DisplaySprites.push_back(displaySprite);
	}

	void ClearDisplaySprites()
	{
		DisplaySprites.clear();
	}
}
