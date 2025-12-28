#pragma once

#include "Rush.h"
#include "UtilMemory.h"

#include <utility>

namespace Rush
{

// Generic buffer similar to tinystl::buffer
template<typename T>
struct Buffer
{
	T* m_data = nullptr;
	size_t m_size = 0;
	size_t m_capacity = 0;

	T* begin() { return m_data; }
	T* end()   { return m_data + m_size; }

	static void destructRange(T* begin, T* end)
	{
		for (; begin < end; ++begin)
		{
			begin->~T();
		}
	}

	static void push(Buffer& buf, const T& val)
	{
		if (buf.m_size < buf.m_capacity)
		{
			new(&buf.m_data[buf.m_size++]) T(val);
		}
		else
		{
			size_t newCapacity = (buf.m_capacity > 0) ? (2 * buf.m_capacity) : 1;
			reserve(buf, newCapacity);
			new(&buf.m_data[buf.m_size++]) T(val);
		}
	}

	static void push(Buffer& buf, T&& val)
	{
		if (buf.m_size < buf.m_capacity)
		{
			new(&buf.m_data[buf.m_size++]) T(std::move(val));
		}
		else
		{
			size_t newCapacity = (buf.m_capacity > 0) ? (2 * buf.m_capacity) : 1;
			reserve(buf, newCapacity);
			new(&buf.m_data[buf.m_size++]) T(std::move(val));
		}
	}

	static void pop(Buffer& buf)
	{
		--buf.m_size;
		buf.m_data[buf.m_size].~T();
	}

	static void constructCopyRange(T* where, const T* begin, const T* end)
	{
		for (; begin < end; ++begin, ++where)
		{
			new(where) T(*begin);
		}
	}

	static void constructMoveRange(T* where, T* begin, T* end)
	{
		for (; begin < end; ++begin, ++where)
		{
			new(where) T(std::move(*begin));
		}
	}

	static void constructRange(T* begin, T* end)
	{
		for (; begin < end; ++begin)
		{
			new(begin) T();
		}
	}

	static void constructRange(T* begin, T* end, const T& val)
	{
		for (; begin < end; ++begin)
		{
			new(begin) T(val);
		}
	}

	static void reserve(Buffer& buf, size_t newCapacity)
	{
		if (newCapacity <= buf.m_capacity) return;

		T* newData = (T*)allocateBytes(newCapacity * sizeof(T));

		constructMoveRange(newData, buf.begin(), buf.end());
		destructRange(buf.begin(), buf.end());
		deallocateBytes(buf.m_data);

		buf.m_data = newData;
		buf.m_capacity = newCapacity;
	}

	static void resize(Buffer& buf, size_t newSize)
	{
		reserve(buf, newSize);

		constructRange(buf.m_data + buf.m_size, buf.m_data + newSize);
		destructRange(buf.m_data + newSize, buf.m_data + buf.m_size);

		buf.m_size = newSize;
	}

	static void resize(Buffer& buf, size_t newSize, const T& defaultValue)
	{
		reserve(buf, newSize);

		constructRange(buf.m_data + buf.m_size, buf.m_data + newSize, defaultValue);
		destructRange(buf.m_data + newSize, buf.m_data + buf.m_size);

		buf.m_size = newSize;
	}

	static void destroy(Buffer& buf)
	{
		destructRange(buf.m_data, buf.m_data + buf.m_size);
		deallocateBytes(buf.m_data);
	}
};

}
