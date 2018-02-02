#pragma once

#include "Rush.h"

#include "UtilDataStream.h"

#include <stdio.h>
#include <string>
#include <vector>

namespace Rush
{
class FileBase : public DataStream
{

public:
	FileBase() : m_file(nullptr){};
	virtual ~FileBase() override{};

	virtual u32  tell() override;
	virtual void seek(u32 pos) override;
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

	virtual u32 read(void* buf, u32 size) override;
	virtual u32 write(const void*, u32) override { return 0; }

	virtual u32 length() override;
};

//////////////////////////////////////////////////////////////////////////

class FileOut : public FileBase
{

public:
	FileOut(const char* filename, u32 buffer_size = (1 << 20));
	virtual ~FileOut();

	virtual u32 read(void*, u32) override { return 0; }
	virtual u32 write(const void* buf, u32 size) override;

	virtual u32 length() override { return 0; }

	void close();

protected:
	void flush();

private:
	FileOut& operator=(const FileOut&) = delete;

	char*     m_buffer;
	const u32 m_bufferSize;
	u32       m_bufferPos;
};
}
