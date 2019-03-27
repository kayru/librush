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
		copyFrom(data, data ? strlen(data) : 0);
	}

	String(String&& other)
	{
		moveFrom((String&&)other);
	}

	String& operator=(const char* data)
	{
		copyFrom(data, data ? strlen(data) : 0);
		return *this;
	}

	String& operator=(String& other)
	{
		copyFrom(other);
		return *this;
	}

	String& operator=(String&& other)
	{
		moveFrom((String&&)other);
		return *this;
	}

	~String()
	{
		delete m_data;
	}

	const char* c_str() const
	{
		return m_data ? m_data : getEmptyString();
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

	static const char* getEmptyString() { static const char* emptyString = ""; return emptyString; }

private:

	void copyFrom(const char* inData, size_t inLength)
	{
		delete m_data;
		if (inData)
		{
			m_length = inLength;
			m_data = new char[inLength + 1];
			memcpy(&m_data, inData, inLength + 1);
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

	const char* c_str() const
	{
		return m_data ? m_data : String::getEmptyString();
	}

private:

	const char* m_data = nullptr;
	size_t m_length = 0;
};


}