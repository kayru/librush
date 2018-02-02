#pragma once

#include "MathCommon.h"
#include "Rush.h"

#include <cstring>

namespace Rush
{

class DataStream
{

public:
	virtual ~DataStream() {}

	virtual u32               write(const void* buf, u32 size) = 0;
	template <typename T> u32 writeT(const T& val) { return write(&val, sizeof(val)); }

	virtual u32 read(void* buf, u32 size) = 0;

	template <typename T> u32 readT(T& val) { return read(&val, sizeof(val)); }

	virtual u32  tell()             = 0;
	virtual void seek(u32 pos)      = 0;
	virtual void skip(int distance) = 0;
	virtual void rewind()           = 0;

	virtual bool valid() = 0;

	virtual u32 length() = 0;
};

class MemDataStream : public DataStream
{
public:
	MemDataStream(const void* data, u32 size) : m_dataRW((u8*)data), m_dataRO((u8*)data), m_pos(0), m_size(size){};
	MemDataStream(void* data, u32 size) : m_dataRW(nullptr), m_dataRO((const u8*)data), m_pos(0), m_size(size){};

	virtual u32 write(const void* buf, u32 size)
	{
		u32 pos2 = m_pos + size;
		pos2     = min(pos2, m_size);
		size     = pos2 - m_pos;

		std::memcpy(&m_dataRW[m_pos], buf, size);
		m_pos = pos2;

		return size;
	}
	virtual u32 read(void* buf, u32 size)
	{

		u32 pos2 = m_pos + size;
		pos2     = min(pos2, m_size);
		size     = pos2 - m_pos;

		std::memcpy(buf, &m_dataRW[m_pos], size);
		m_pos = pos2;

		return size;
	}

	virtual u32  tell() { return m_pos; };
	virtual void seek(u32 pos) { m_pos = min(pos, m_size); };
	virtual void skip(int distance)
	{
		u32 pos2 = m_pos + distance;
		pos2     = min(pos2, m_size);
		m_pos    = pos2;
	};
	virtual void rewind() { m_pos = 0; };

	virtual bool valid() { return m_dataRO != nullptr; }

	virtual u32 length() { return m_size; }

private:
	u8*       m_dataRW;
	const u8* m_dataRO;

	u32 m_pos;
	u32 m_size;
};

class NullDataStream : public DataStream
{
public:
	NullDataStream() : m_pos(0), m_size(0) {}

	virtual u32 write(const void*, u32 size)
	{
		m_pos += size;
		return m_pos;
	}
	virtual u32 read(void*, u32 size)
	{
		m_pos += size;
		return m_pos;
	}

	virtual u32  tell() { return m_pos; }
	virtual void seek(u32 pos) { m_pos = pos; }
	virtual void skip(int distance) { m_pos += distance; }
	virtual void rewind() { m_pos = 0; }

	virtual bool valid() { return true; }

	virtual u32 length() { return m_size; }

private:
	u32 m_pos;
	u32 m_size;
};
}
