#pragma once

#include "Rush.h"

namespace Rush
{
template <typename T, size_t CAPACITY> struct StaticArray
{
	StaticArray() : currentSize(0) {}

	T&       operator[](size_t i) { return data[i]; }
	const T& operator[](size_t i) const { return data[i]; }
	const T& back() const { return data[size() - 1]; }
	T&       back() { return data[size() - 1]; }

	bool pushBack(const T& val)
	{
		if (currentSize < CAPACITY)
		{
			data[currentSize] = val;
			++currentSize;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool popBack()
	{
		if (currentSize > 0)
		{
			--currentSize;
			return true;
		}
		else
		{
			return false;
		}
	}

	void pushBackUnsafe(const T& val)
	{
		data[currentSize] = val;
		++currentSize;
	}

	void popBackUnsafe(const T& val) { --currentSize; }

	void clear() { currentSize = 0; }

	size_t        capacity() const { return CAPACITY; }
	size_t        size() const { return currentSize; }
	static size_t elementSize() { return sizeof(T); }

	T*       begin() { return data; }
	T*       end() { return data + currentSize; }
	const T* begin() const { return data; }
	const T* end() const { return data + currentSize; }

	T      data[CAPACITY];
	size_t currentSize;
};
}
