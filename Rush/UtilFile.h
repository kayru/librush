#pragma once

#include "Rush.h"

#include "UtilDataStream.h"

#include <stdio.h>

namespace Rush
{
class FileBase : public DataStream
{

public:
	FileBase() : m_file(nullptr){};
	virtual ~FileBase() override{};

	virtual u64  tell() override;
	virtual void seek(u64 pos) override;
	virtual void skip(int distance) override;
	virtual void rewind() override;

	virtual bool valid() override;

protected:
	FILE* m_file;

private:
};

//////////////////////////////////////////////////////////////////////////

class FileIn : public FileBase
{

public:
	FileIn(const char* filename);
	virtual ~FileIn() override;

	virtual u64 read(void* buf, u64 size) override;
	virtual u64 write(const void*, u64) override { return 0; }

	virtual u64 length() override;
};

//////////////////////////////////////////////////////////////////////////

class FileOut : public FileBase
{

public:
	FileOut(const char* filename, u64 buffer_size = (1 << 20));
	virtual ~FileOut();

	virtual u64 read(void*, u64) override { return 0; }
	virtual u64 write(const void* buf, u64 size) override;

	virtual u64 length() override { return 0; }

	void close();

protected:
	void flush();

private:
	FileOut& operator=(const FileOut&) = delete;

	char*     m_buffer;
	const u64 m_bufferSize;
	u64       m_bufferPos;
};
}
