#include "UtilFile.h"
#include "MathCommon.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // 'fopen': This function or variable may be unsafe. Consider using fopen_s instead.
#endif                          //_MSC_VER

namespace Rush
{
u32 FileIn::read(void* buf, u32 size)
{
	u32 br = (u32)fread(buf, 1, size, m_file);
	return br;
}

FileIn::FileIn(const char* filename) { m_file = fopen(filename, "rb"); }

FileIn::~FileIn()
{
	if (m_file)
	{
		fclose(m_file);
		m_file = nullptr;
	}
}

u32 FileIn::length()
{
	u32 currPos = tell();

	fseek(m_file, 0, SEEK_END);

	u32 endPos = tell();

	fseek(m_file, currPos, SEEK_SET);

	return endPos;
}

u32 FileOut::write(const void* buf, u32 size)
{
	u32 inBufPos    = 0;
	u32 inBytesLeft = size;

	while (inBytesLeft != 0)
	{
		u32 outBytesLeft = m_bufferSize - m_bufferPos;

		u32 bytesToCopy = min(inBytesLeft, outBytesLeft);

		memcpy(&m_buffer[m_bufferPos], &((char*)buf)[inBufPos], bytesToCopy);

		m_bufferPos += bytesToCopy;
		inBufPos += bytesToCopy;

		inBytesLeft -= bytesToCopy;

		if (inBytesLeft != 0 || outBytesLeft == 0)
		{
			flush();
		}
	}
	return size;
}

FileOut::FileOut(const char* filename, u32 buffer_size) : m_buffer(nullptr), m_bufferSize(buffer_size), m_bufferPos(0)
{
	m_file   = fopen(filename, "wb");
	m_buffer = new char[m_bufferSize];
}

FileOut::~FileOut()
{
	if (m_file)
	{
		close();
	}
}

void FileOut::flush()
{
	fwrite(m_buffer, 1, m_bufferPos, m_file);
	m_bufferPos = 0;
}

void FileOut::close()
{
	flush();

	fclose(m_file);
	m_file = nullptr;

	delete m_buffer;
	m_buffer = nullptr;
}

u32 FileBase::tell()
{
	return (u32)ftell(m_file); // TODO: fix casting
}

void FileBase::seek(u32 pos) { fseek(m_file, pos, SEEK_SET); }

void FileBase::skip(int distance) { fseek(m_file, distance, SEEK_CUR); }

void FileBase::rewind() { fseek(m_file, 0, SEEK_SET); }

bool FileBase::valid() { return m_file != nullptr; }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER
