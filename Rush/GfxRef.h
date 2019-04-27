#pragma once

#include "GfxDevice.h"
#include "UtilBuffer.h"

namespace Rush
{

struct GfxRefCount
{
	void addReference() { m_refs++; }

	u32 removeReference()
	{
		RUSH_ASSERT(m_refs != 0);
		return m_refs--;
	}

	u32 m_refs = 0;
};

template <typename T> class GfxRef
{
public:
	GfxRef() : m_handle(InvalidResourceHandle()) {}

	GfxRef(const GfxRef& rhs)
	{
		if (rhs.m_handle.valid())
		{
			m_handle = rhs.m_handle;
			Gfx_Retain(m_handle);
		}
	}

	GfxRef(GfxRef&& rhs) : m_handle(rhs.m_handle) { rhs.m_handle = InvalidResourceHandle(); }

	~GfxRef()
	{
		if (m_handle.valid())
		{
			Gfx_Release(m_handle);
		}
	}

	GfxRef& operator=(GfxRef<T>&& rhs)
	{
		m_handle     = rhs.m_handle;
		rhs.m_handle = InvalidResourceHandle();
		return *this;
	}

	GfxRef& operator=(const GfxRef<T>& other)
	{
		retain(other.get());
		return *this;
	}

	bool operator==(const GfxRef<T>& other) const { return m_handle == other.m_handle; }

	bool operator==(const T& other) const { return m_handle == other; }

	bool operator!=(const GfxRef<T>& other) const { return m_handle != other.m_handle; }

	bool operator!=(const T& other) const { return m_handle != other; }

	T get() const { return m_handle; }

	bool valid() const { return m_handle.valid(); }

	void reset()
	{
		if (m_handle.valid())
		{
			Gfx_Release(m_handle);
		}
		m_handle = InvalidResourceHandle();
	}

	void retain(GfxArg<T> h)
	{
		if (h.valid())
		{
			Gfx_Retain(h);
		}

		if (m_handle.valid())
		{
			Gfx_Release(m_handle);
		}

		m_handle = h;
	}

private:
	T m_handle;
};

}
