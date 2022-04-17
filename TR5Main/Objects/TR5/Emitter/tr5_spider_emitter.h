#pragma once
#include "Game/items.h"
#include "Specific/EulerAngle.h"

constexpr auto NUM_SPIDERS = 64;

struct SpiderData
{
	byte On;
	PoseData Pose;
	EulerAngle Orientation;
	short RoomNumber;

	short Velocity;
	short VerticalVelocity;

	byte Flags;
};

extern int NextSpider;
extern SpiderData Spiders[NUM_SPIDERS];

short GetNextSpider();
void ClearSpiders();
void ClearSpidersPatch(ITEM_INFO* item);
void InitialiseSpiders(short itemNumber);
void SpidersEmitterControl(short itemNumber);
void UpdateSpiders();