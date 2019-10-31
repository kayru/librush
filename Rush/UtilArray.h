#pragma once

#include "Rush.h"
#include "UtilBuffer.h"
#include <new>

namespace Rush
{

template <typename T>
class DynamicArray
{
public:

	DynamicArray() = default;

	DynamicArray(size_t size)
		: DynamicArray()
	{
		resize(size);
	}

	DynamicArray(size_t size, const T& defaultValue)
		: DynamicArray()
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		resize(size, defaultValue);
	}

	DynamicArray(const DynamicArray& other)
		: DynamicArray()
	{
		Buffer<T>::reserve(m_buffer, other.m_buffer.m_size);
		Buffer<T>::constructCopyRange(begin(), other.begin(), other.end());
		m_buffer.m_size = other.m_buffer.m_size;
	}

	DynamicArray(DynamicArray&& other) noexcept
		: DynamicArray()
	{
		if (&other != this)
		{
			m_buffer = other.m_buffer;
			other.m_buffer = Buffer<T>();
		}
	}

	DynamicArray& operator = (const DynamicArray& other)
	{
		if (&other != this)
		{
			clear();
			Buffer<T>::reserve(m_buffer, other.m_buffer.m_size);
			Buffer<T>::constructCopyRange(begin(), other.begin(), other.end());
			m_buffer.m_size = other.m_buffer.m_size;
		}
		return *this;
	}

	DynamicArray& operator = (DynamicArray&& other)
	{
		if (&other != this)
		{
			m_buffer = other.m_buffer;
			other.m_buffer = Buffer<T>();
		}
		return *this;
	}


	~DynamicArray()
	{
		Buffer<T>::destroy(m_buffer);
	}

	T&       operator[](size_t i) { return m_buffer.m_data[i]; }
	const T& operator[](size_t i) const { return m_buffer.m_data[i]; }

	T*       data()       { return m_buffer.m_data; }
	const T* data() const { return m_buffer.m_data; }

	T* begin() { return m_buffer.m_data; }
	T* end()   { return m_buffer.m_data + m_buffer.m_size; }

	const T* begin() const { return m_buffer.m_data; }
	const T* end() const { return m_buffer.m_data + m_buffer.m_size; }

	T& front() { return m_buffer.m_data[0]; }
	const T& front() const { return m_buffer.m_data[0]; }

	T& back() { return m_buffer.m_data[m_buffer.m_size-1]; }
	const T& back() const { return m_buffer.m_data[m_buffer.m_size - 1]; }

	size_t size() const { return m_buffer.m_size; }

	RUSH_FORCEINLINE void resize(size_t desiredSize)
	{
		Buffer<T>::resize(m_buffer, desiredSize);
	}

	RUSH_FORCEINLINE void resize(size_t desiredSize, const T& defaultValue)
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		Buffer<T>::resize(m_buffer, desiredSize, defaultValue);
	}

	RUSH_FORCEINLINE void reserve(size_t desiredCapacity)
	{
		Buffer<T>::reserve(m_buffer, desiredCapacity);
	}

	RUSH_FORCEINLINE void push(const T& val)
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		Buffer<T>::push(m_buffer, val);
	}

	RUSH_FORCEINLINE void pop()
	{
		Buffer<T>::pop(m_buffer);
	}

	RUSH_FORCEINLINE void clear()
	{
		Buffer<T>::destructRange(begin(), end());
		m_buffer.m_size = 0;
	}

	bool empty() const
	{
		return m_buffer.m_size == 0;
	}

	// Aliases for STL compatibility

	RUSH_FORCEINLINE void push_back(const T& val)
	{
		static_assert(std::is_copy_constructible<T>::value, "Type must be copy-constructible");
		Buffer<T>::push(m_buffer, val);
	}

	RUSH_FORCEINLINE void push_back(T&& val)
	{
		Buffer<T>::push(m_buffer, std::move(val));
	}

	RUSH_FORCEINLINE void pop_back()
	{
		Buffer<T>::pop(m_buffer);
	}

private:

	Buffer<T> m_buffer;
};

template <typename T, size_t CAPACITY>
struct StaticArray
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

template <typename T, size_t N>
struct alignas(alignof(T)) InlineDynamicArray
{
	RUSH_DISALLOW_COPY_AND_ASSIGN(InlineDynamicArray)

	u8 m_inlineData[sizeof(T) * N];
	T * m_data = nullptr;
	u32 m_count = 0;

	InlineDynamicArray(u32 count)
		: m_data(reinterpret_cast<T*>(m_inlineData))
		, m_count(count)
	{
		if (m_count > N)
		{
			m_data = new T[m_count];
		}
		else
		{
			Buffer<T>::constructRange(m_data, m_data + m_count);
		}
	}

	~InlineDynamicArray()
	{
		if ((void*)m_data == (void*)m_inlineData)
		{
			Buffer<T>::destructRange(m_data, m_data + m_count);
		}
		else
		{
			delete[] m_data;
		}
	}

	T&       operator[](size_t i) { return m_data[i]; }
	const T& operator[](size_t i) const { return m_data[i]; }
};

}
