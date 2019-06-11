#pragma once

#include "Rush.h"

#include "UtilLog.h"
#include "UtilArray.h"

#include <limits>
#include <string.h>

namespace Rush
{
struct InvalidResourceHandle
{
};

struct UntypedResourceHandle
{
	typedef u16 IndexType;
	UntypedResourceHandle(IndexType _index) : m_index(_index) {}

	bool      valid() const { return m_index != 0; }
	IndexType index() const { return m_index; }

	IndexType m_index;
};

template <typename T> class ResourceHandle
{
public:
	typedef u16 IndexType;

	ResourceHandle() : m_index(0) {}
	ResourceHandle(InvalidResourceHandle) : m_index(0) {}
	explicit ResourceHandle(UntypedResourceHandle h) : m_index(h.index()) {}

	bool      valid() const { return m_index != 0; }
	IndexType index() const { return m_index; }

	bool operator==(const ResourceHandle<T>& rhs) const { return m_index == rhs.m_index; }
	bool operator!=(const ResourceHandle<T>& rhs) const { return m_index != rhs.m_index; }

	operator UntypedResourceHandle() const { return UntypedResourceHandle(m_index); }

private:
	ResourceHandle(IndexType idx) : m_index(idx) {}
	IndexType m_index;
};

template <typename T, typename HANDLE_TYPE> class ResourcePool
{
public:
	typedef HANDLE_TYPE HandleType;

	ResourcePool()
	{
		T default_value;
		memset(&default_value, 0, sizeof(default_value));
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

	void reset()
	{
		data.clear();
		empty.clear();
	}

	u32 allocatedCount() const
	{
		return u32(data.size() - empty.size());
	}

	const T& operator[](HANDLE_TYPE h) const { return data[h.index()]; }
	T& operator[](HANDLE_TYPE h) { return data[h.index()]; }

	DynamicArray<T>      data;
	DynamicArray<size_t> empty;
};
}
