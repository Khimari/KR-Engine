#pragma once

#include <sol.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include "..\Global\global.h"

using namespace std;

#define ITEM_PARAM_CURRENT_ANIM_STATE		0
#define ITEM_PARAM_GOAL_ANIM_STATE			1
#define ITEM_PARAM_REQUIRED_ANIM_STATE		2
#define ITEM_PARAM_FRAME_NUMBER				3
#define ITEM_PARAM_ANIM_NUMBER				4
#define ITEM_PARAM_HIT_POINTS				5
#define ITEM_PARAM_HIT_STATUS				6
#define ITEM_PARAM_GRAVITY_STATUS			7
#define ITEM_PARAM_COLLIDABLE				8
#define ITEM_PARAM_POISONED					9
#define ITEM_PARAM_ROOM_NUMBER				10

typedef struct LuaFunction {
	string Name;
	string Code;
	bool Executed;
};

typedef struct GameScriptItemPosition {
	__int32 x;
	__int32 y;
	__int32 z;
	__int16 xRot;
	__int16 yRot;
	__int16 zRot;
	__int16 room;
};

typedef struct GameScriptItem {
	ITEM_INFO*		NativeItem;

	__int16 Get(__int32 param)
	{
		if (NativeItem == NULL)
			return 0;

		switch (param)
		{
		case ITEM_PARAM_CURRENT_ANIM_STATE:
			return NativeItem->currentAnimState;
		case ITEM_PARAM_REQUIRED_ANIM_STATE:
			return NativeItem->requiredAnimState;
		case ITEM_PARAM_GOAL_ANIM_STATE:
			return NativeItem->goalAnimState;
		case ITEM_PARAM_ANIM_NUMBER:
			return NativeItem->animNumber;
		case ITEM_PARAM_FRAME_NUMBER:
			return NativeItem->frameNumber;
		case ITEM_PARAM_HIT_POINTS:
			return NativeItem->hitPoints;
		case ITEM_PARAM_HIT_STATUS:
			return NativeItem->hitStatus;
		case ITEM_PARAM_GRAVITY_STATUS:
			return NativeItem->gravityStatus;
		case ITEM_PARAM_COLLIDABLE:
			return NativeItem->collidable;
		case ITEM_PARAM_POISONED:
			return NativeItem->poisoned;
		case ITEM_PARAM_ROOM_NUMBER:
			return NativeItem->roomNumber;
		default:
			return 0;
		}
	}

	void Set(__int32 param, __int16 value)
	{
		if (NativeItem == NULL)
			return;

		switch (param)
		{
		case ITEM_PARAM_CURRENT_ANIM_STATE:
			NativeItem->currentAnimState = value; break;
		case ITEM_PARAM_REQUIRED_ANIM_STATE:
			NativeItem->requiredAnimState = value; break;
		case ITEM_PARAM_GOAL_ANIM_STATE:
			NativeItem->goalAnimState = value; break;
		case ITEM_PARAM_ANIM_NUMBER:
			NativeItem->animNumber = value;
			NativeItem->frameNumber = Anims[NativeItem->animNumber].frameBase;
			break;
		case ITEM_PARAM_FRAME_NUMBER:
			NativeItem->frameNumber = value; break;
		case ITEM_PARAM_HIT_POINTS:
			NativeItem->hitPoints = value; break;
		case ITEM_PARAM_HIT_STATUS:
			NativeItem->hitStatus = value; break;
		case ITEM_PARAM_GRAVITY_STATUS:
			NativeItem->gravityStatus = value; break;
		case ITEM_PARAM_COLLIDABLE:
			NativeItem->collidable = value; break;
		case ITEM_PARAM_POISONED:
			NativeItem->poisoned = value; break;
		case ITEM_PARAM_ROOM_NUMBER:
			NativeItem->roomNumber = value; break;
		default:
			break;
		}
	}

	GameScriptItemPosition GetItemPosition()
	{
		GameScriptItemPosition pos;
		
		//if (m_item == NULL)
		//	return pos;

		pos.x = NativeItem->pos.xPos;
		pos.y = NativeItem->pos.yPos;
		pos.z = NativeItem->pos.zPos;
		pos.xRot = TR_ANGLE_TO_DEGREES(NativeItem->pos.xRot);
		pos.yRot = TR_ANGLE_TO_DEGREES(NativeItem->pos.yRot);
		pos.zRot = TR_ANGLE_TO_DEGREES(NativeItem->pos.zRot);
		pos.room = NativeItem->roomNumber;

		return pos;
	}

	void SetItemPosition(__int32 x, __int32 y, __int32 z)
	{
		if (NativeItem == NULL)
			return;

		NativeItem->pos.xPos = x;
		NativeItem->pos.yPos = y;
		NativeItem->pos.zPos = z;
	}

	void SetItemRotation(__int32 x, __int32 y, __int32 z)
	{
		if (NativeItem == NULL)
			return;

		NativeItem->pos.xRot = ANGLE(x);
		NativeItem->pos.yRot = ANGLE(y);
		NativeItem->pos.zRot = ANGLE(z);
	}
};

class GameScript
{
private:
	sol::state*							m_lua;
	sol::table							m_globals;
	sol::table							m_locals;
	map<__int32, __int32>				m_itemsMap;
	vector<LuaFunction*>				m_triggers;
	GameScriptItem						m_items[NUM_ITEMS];

	string								loadScriptFromFile(char* luaFilename);

public:	
	GameScript(sol::state* lua);
	~GameScript();
	
	bool								ExecuteScript(char* luaFilename);
	void								FreeLevelScripts();
	void								AddTrigger(LuaFunction* function);
	void								AddLuaId(int luaId, int itemId);
	void								SetItem(__int32 index, ITEM_INFO* item);
	void								AssignVariables();
	void								ResetVariables();

	void								EnableItem(__int16 id);
	void								DisableItem(__int16 id);
	void								PlayAudioTrack(__int16 track);
	void								ChangeAmbientSoundTrack(__int16 track);
	bool								ExecuteTrigger(__int16 index);
	void								JumpToLevel(__int32 levelNum);
	__int32								GetSecretsCount();
	void								SetSecretsCount(__int32 secretsNum);
	void								AddOneSecret();
	void								MakeItemInvisible(__int16 id);
	GameScriptItem						GetItem(__int16 id);
	void								PlaySoundEffectAtPosition(__int16 id, __int32 x, __int32 y, __int32 z, __int32 flags);
	void								PlaySoundEffect(__int16 id, __int32 flags);
};