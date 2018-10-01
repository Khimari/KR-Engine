#pragma once

#include "..\Global\global.h"
#include "..\Specific\IO\ChunkId.h"
#include "..\Specific\IO\ChunkReader.h"
#include "..\Specific\IO\ChunkWriter.h"
#include "..\Specific\IO\LEB128.h"
#include "..\Specific\IO\Streams.h"
#include "..\Scripting\GameFlowScript.h"
#include "..\Scripting\GameLogicScript.h"

#define RestoreGame ((__int32 (__cdecl*)()) 0x00472060)	
#define ReadSavegame ((__int32 (__cdecl*)(__int32)) 0x004A8E10)	
#define CreateSavegame ((void (__cdecl*)()) 0x00470FA0)	
#define WriteSavegame ((__int32 (__cdecl*)(__int32)) 0x004A8BC0)	

#define SAVEGAME_BUFFER_SIZE 1048576

extern GameFlow* g_GameFlow;
extern GameScript* g_GameScript;

class SaveGame {
private:
	static FileStream* m_stream;
	static ChunkReader* m_reader;
	static ChunkWriter* m_writer;

	static ChunkId* m_chunkHeader;
	static ChunkId* m_chunkGameStatus;
	static ChunkId* m_chunkItems;
	static ChunkId* m_chunkItem;
	static ChunkId* m_chunkLara;
	static ChunkId* m_chunkLuaVariables;
	static ChunkId* m_chunkLuaLocal;
	static ChunkId* m_chunkLuaGlobal;

	static void SaveHeader(__int32 param);
	static void SaveGameStatus(__int32 param);
	static void SaveLara(__int32 param);
	static void SaveItem(__int32 param);
	static void SaveItems(__int32 param);

public:
	static bool Save(char* fileName);
};