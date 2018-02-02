#pragma once

namespace Rush
{
template <typename T> class LinearAllocator
{
public:
	LinearAllocator() : m_data(nullptr), m_size(0), m_capacity(0) {}
	LinearAllocator(T* data, size_t capacity) : m_data(data), m_size(0), m_capacity(capacity){};

	bool canAllocate(size_t count) const { return (m_size + count) <= m_capacity; }

	T* allocate(size_t count)
	{
		size_t newSize = m_size + count;
		if (newSize <= m_capacity)
		{
			T* res = &m_data[m_size];
			m_size = newSize;
			return res;
		}
		else
		{
			return nullptr;
		}
	}

	T* allocateUnsafe(size_t count)
	{
		T* res = &m_data[m_size];
		m_size += count;
		return res;
	}

	size_t capacity() const { return m_capacity; }
	void   clear() { m_size = 0; }
	size_t size() const { return m_size; }
	size_t sizeInBytes() const { return m_size * sizeof(T); }

	T* data() { return m_data; }

	T*     m_data;
	size_t m_size;
	size_t m_capacity;
};
}
