#pragma once
#include "Specific/phd_global.h"
#include "Game/items.h"

struct BaboonRespawnStruct
{
	int ID;
	PoseData Pos;
	unsigned int Count;
	unsigned int MaxCount;  // Used to limit the number of respawns.
};

class BaboonRespawnClass
{
private:
	std::vector<BaboonRespawnStruct> baboonRespawnArray;
public:
	void Free(void);
	void Add(ITEM_INFO* item, unsigned int maxCount);
	void Remove(int ID);
	int GetBaboonFreePlace(void);
	BaboonRespawnStruct* GetBaboonRespawn(int ID);
	int GetCount(int ID);
	int GetCountMax(int ID);
};

extern BaboonRespawnClass BaboonRespawn;

extern void InitialiseBaboon(short itemNumber);
extern void BaboonControl(short itemNumber);
