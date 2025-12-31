#pragma once

#include "Rush.h"
#include <string.h>

namespace Rush
{

class String
{
public:

	String() = default;

	String(const String& other)
	{
		copyFrom(other);
	}

	String(const char* data, size_t length=0)
	{
		if (length == 0 && data)
		{
			length = strlen(data);
		}
		copyFrom(data, length);
	}

	String(String&& other) noexcept
	{
		moveFrom((String&&)other);
	}

	String& operator=(const char* data)
	{
		copyFrom(data, data ? strlen(data) : 0);
		return *this;
	}

	String& operator=(const String& other)
	{
		copyFrom(other);
		return *this;
	}

	String& operator=(String&& other) noexcept
	{
		moveFrom((String&&)other);
		return *this;
	}

	~String()
	{
		delete[] m_data;
	}

	const char* c_str() const
	{
		return m_data ? m_data : getEmptyString();
	}

	char* data()
	{
		return m_data;
	}

	size_t length() const { return m_length; }

	char& operator [] (size_t idx)
	{
		return m_data[idx];
	}

	const char& operator [] (size_t idx) const
	{
		return m_data[idx];
	}

	void reset(size_t length)
	{
		delete[] m_data;
		m_length = length;
		if (length)
		{
			m_data = new char[length + 1];
			m_data[length] = 0;
		}
		else
		{
			m_data = nullptr;
		}
	}

	static const char* getEmptyString() { static const char* emptyString = ""; return emptyString; }

private:

	void copyFrom(const char* inData, size_t inLength)
	{
		delete[] m_data;
		if (inData)
		{
			m_length = inLength;
			m_data = new char[inLength + 1];
			memcpy(m_data, inData, inLength);
			m_data[inLength] = 0;
		}
		else
		{
			m_length = 0;
			m_data = nullptr;
		}
	}

	void copyFrom(const String& other)
	{
		copyFrom(other.c_str(), other.length());
	}

	void moveFrom(String&& other)
	{
		m_data = other.m_data;
		m_length = other.m_length;

		other.m_data = nullptr;
		other.m_length = 0;
	}

	char* m_data = nullptr;
	size_t m_length = 0;
};

class StringView
{
public:

	StringView(const char* inData, size_t inLength)
		: m_data(inData)
		, m_length(inLength)
	{
	}

	size_t length() const { return m_length; }

	char operator [] (size_t idx) const
	{
		return m_data[idx];
	}

	const char* data() const
	{
		return m_data ? m_data : String::getEmptyString();
	}

	bool operator == (const char* other) const
	{
		const char* rhs = other ? other : String::getEmptyString();
		return strncmp(data(), rhs, m_length) == 0;
	}

	bool operator != (const char* other) const
	{
		const char* rhs = other ? other : String::getEmptyString();
		return strncmp(data(), rhs, m_length) != 0;
	}

	bool operator == (const StringView& other) const
	{
		return m_length == other.m_length && memcmp(data(), other.data(), m_length) == 0;
	}

	bool operator != (const StringView& other) const
	{
		return m_length != other.m_length || memcmp(data(), other.data(), m_length) != 0;
	}

private:

	const char* m_data = nullptr;
	size_t m_length = 0;
};


}
