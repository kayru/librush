#include "UtilFile.h"
#include "MathCommon.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // 'fopen': This function or variable may be unsafe. Consider using fopen_s instead.
#endif                          //_MSC_VER

namespace Rush
{
u64 FileIn::read(void* buf, u64 size)
{
	u64 br = (u64)fread(buf, 1, size, m_file);
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

u64 FileIn::length()
{
	u64 currPos = tell();

	fseek(m_file, 0, SEEK_END);

	u64 endPos = tell();

	fseek(m_file, long(currPos), SEEK_SET);

	return endPos;
}

u64 FileOut::write(const void* buf, u64 size)
{
	u64 inBufPos    = 0;
	u64 inBytesLeft = size;

	while (inBytesLeft != 0)
	{
		u64 outBytesLeft = m_bufferSize - m_bufferPos;

		u64 bytesToCopy = min(inBytesLeft, outBytesLeft);

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

FileOut::FileOut(const char* filename, u64 buffer_size) : m_buffer(nullptr), m_bufferSize(buffer_size), m_bufferPos(0)
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

u64 FileBase::tell()
{
	long fileSize = ftell(m_file);
	if (fileSize == long(-1))
	{
		return 0;
	}
	else
	{
		return u64(fileSize);
	}
}

void FileBase::seek(u64 pos) { fseek(m_file, long(pos), SEEK_SET); }

void FileBase::skip(int distance) { fseek(m_file, distance, SEEK_CUR); }

void FileBase::rewind() { fseek(m_file, 0, SEEK_SET); }

bool FileBase::valid() { return m_file != nullptr; }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif //_MSC_VER
