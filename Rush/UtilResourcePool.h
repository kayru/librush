#pragma once

#include "Rush.h"

#include "UtilLog.h"
#include "UtilStaticArray.h"

#include <limits>
#include <vector>

namespace Rush
{
struct InvalidResourceHandle
{
};

struct UntypedResourceHandle
{
	typedef u16 IndexType;
	UntypedResourceHandle(IndexType _index) : index(_index) {}
	IndexType index;
};

template <typename T> class ResourceHandle
{
public:
	typedef u16 IndexType;

	ResourceHandle() : m_index(0) {}
	ResourceHandle(InvalidResourceHandle) : m_index(0) {}
	explicit ResourceHandle(UntypedResourceHandle h) : m_index(h.index) {}

	bool      valid() const { return m_index != 0; }
	IndexType index() const { return m_index; }

	bool operator==(const ResourceHandle<T>& rhs) const { return m_index == rhs.m_index; }
	bool operator!=(const ResourceHandle<T>& rhs) const { return m_index != rhs.m_index; }

private:
	ResourceHandle(IndexType idx) : m_index(idx) {}
	IndexType m_index;
};

template <typename T, typename HANDLE_TYPE, size_t CAPACITY> class ResourcePool
{
public:
	typedef HANDLE_TYPE HandleType;

	ResourcePool()
	{
		T default_value;
		rush_memset(&default_value, 0, sizeof(default_value));

		for (size_t i = CAPACITY; i != 0; --i)
		{
			empty.pushBack(i - 1);
		}

		push(default_value);
	}

	HANDLE_TYPE push(const T& val)
	{
		RUSH_ASSERT(empty.size() != 0);
		size_t idx = empty.back();
		empty.popBack();
		data[idx] = val;
		return HANDLE_TYPE(UntypedResourceHandle(idx));
	}

	void remove(HANDLE_TYPE h)
	{
		if (h.valid())
		{
			empty.pushBack(h.index());
		}
	}

	const T& operator[](HANDLE_TYPE h) const { return data[h.index()]; }

	T& operator[](HANDLE_TYPE h) { return data[h.index()]; }

	u32 capacity() const { return CAPACITY; }

	T                             data[CAPACITY];
	StaticArray<size_t, CAPACITY> empty;
};

template <typename T, typename HANDLE_TYPE> class DynamicResourcePool
{
public:
	typedef HANDLE_TYPE HandleType;

	DynamicResourcePool()
	{
		T default_value;
		std::memset(&default_value, 0, sizeof(default_value));
		push(default_value);
	}

	HANDLE_TYPE push(const T& val)
	{
		size_t idx;
		if (empty.empty())
		{
			idx = data.size();
			data.push_back(val);
		}
		else
		{
			idx = empty.back();
			empty.pop_back();
			data[idx] = val;
		}
		RUSH_ASSERT(idx < std::numeric_limits<UntypedResourceHandle::IndexType>::max())
		return HANDLE_TYPE(UntypedResourceHandle((UntypedResourceHandle::IndexType)idx));
	}

	void remove(HANDLE_TYPE h)
	{
		if (h.valid())
		{
			empty.push_back(h.index());
		}
	}

	const T& operator[](HANDLE_TYPE h) const { return data[h.index()]; }

	T& operator[](HANDLE_TYPE h) { return data[h.index()]; }

	std::vector<T>      data;
	std::vector<size_t> empty;
};
}
