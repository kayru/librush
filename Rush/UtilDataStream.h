#pragma once

#include "MathCommon.h"
#include "UtilLog.h"

#include <string.h>

namespace Rush
{

class DataStream
{

public:
	virtual ~DataStream() {}

	virtual u64               write(const void* buf, u64 size) = 0;
	template <typename T> u64 writeT(const T& val) { return write(&val, sizeof(val)); }

	virtual u64 read(void* buf, u64 size) = 0;

	template <typename T> u64 readT(T& val) { return read(&val, sizeof(val)); }

	virtual u64  tell() const       = 0;
	virtual void seek(u64 pos)      = 0;
	virtual void skip(int distance) = 0;
	virtual void rewind()           = 0;

	virtual bool valid() const = 0;
	virtual u64 length() const = 0;
};

class MemDataStream : public DataStream
{
public:
	MemDataStream(void* data, u64 size) : m_dataRW((u8*)data), m_dataRO((const u8*)data), m_pos(0), m_size(size)
	{
		RUSH_ASSERT(data != nullptr);
	};
	MemDataStream(const void* data, u64 size) : m_dataRW(nullptr), m_dataRO((const u8*)data), m_pos(0), m_size(size)
	{
		RUSH_ASSERT(data != nullptr);
	};

	virtual u64 write(const void* buf, u64 size)
	{
		if (m_dataRW == nullptr)
		{
			return 0;
		}

		u64 pos2 = m_pos + size;
		pos2     = min(pos2, m_size);
		size     = pos2 - m_pos;

		memcpy(&m_dataRW[m_pos], buf, size);
		m_pos = pos2;

		return size;
	}
	virtual u64 read(void* buf, u64 size)
	{
		u64 pos2 = m_pos + size;
		pos2     = min(pos2, m_size);
		size     = pos2 - m_pos;

		memcpy(buf, &m_dataRO[m_pos], size);
		m_pos = pos2;

		return size;
	}

	virtual u64  tell() const { return m_pos; };
	virtual void seek(u64 pos) { m_pos = min(pos, m_size); };
	virtual void skip(int distance)
	{
		u64 pos2 = m_pos + distance;
		pos2     = min(pos2, m_size);
		m_pos    = pos2;
	};
	virtual void rewind() { m_pos = 0; };

	virtual bool valid() const { return m_dataRO != nullptr; }

	virtual u64 length() const { return m_size; }

private:
	u8*       m_dataRW;
	const u8* m_dataRO;

	u64 m_pos;
	u64 m_size;
};

class NullDataStream : public DataStream
{
public:
	NullDataStream() : m_pos(0), m_size(0) {}

	virtual u64 write(const void*, u64 size)
	{
		m_pos += size;
		return size;
	}
	virtual u64 read(void*, u64 size)
	{
		m_pos += size;
		return size;
	}

	virtual u64  tell() const { return m_pos; }
	virtual void seek(u64 pos) { m_pos = pos; }
	virtual void skip(int distance) { m_pos += distance; }
	virtual void rewind() { m_pos = 0; }

	virtual bool valid() const { return true; }
	virtual u64 length() const { return m_size; }

private:
	u64 m_pos;
	u64 m_size;
};
}
