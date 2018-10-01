#pragma once

#include <Windows.h>
#include <stdlib.h>
#include <memory>
#include <string>

#include "LEB128.h"
#include "Streams.h"

using namespace std;

typedef struct ChunkId
{
private:
	byte*		m_chunkBytes;
	__int32		m_length;

public:
	ChunkId(char* bytes, __int32 length)
	{
		if (length == 0)
		{
			m_chunkBytes = NULL;
			m_length = 0;
		}
		else
		{
			m_chunkBytes = (byte*)malloc(length);
			memcpy(m_chunkBytes, bytes, length);
			m_length = length;
		}
	}

	~ChunkId()
	{
		if (m_chunkBytes != NULL)
			delete m_chunkBytes;
	}

	static ChunkId* FromString(char* str)
	{
		return new ChunkId(str, strlen(str));
	}

	static ChunkId* FromString(string* str)
	{
		return new ChunkId((char*)str->c_str(), str->length());
	}

	static ChunkId* FromStream(BaseStream* stream)
	{
		__int32 idLength = LEB128::ReadInt32(stream);
		char* buffer = (char*)malloc(idLength);
		stream->Read(buffer, idLength);
		ChunkId* chunk = new ChunkId(buffer, idLength);
		free(buffer);
		return chunk;
	}

	void ToStream(BaseStream* stream)
	{
		LEB128::Write(stream, m_length);
		stream->WriteBytes(m_chunkBytes, m_length);
	}

	byte* GetBytes()
	{
		return m_chunkBytes;
	}

	__int32 GetLength()
	{
		return m_length;
	}

	bool EqualsTo(ChunkId* other)
	{
		if (m_length != other->GetLength())
			return false;
		return (strncmp((const char*)m_chunkBytes, (const char*)other->GetBytes(), m_length) == 0);
	}
};

