#pragma once

#include "GfxDevice.h"

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

	void takeover(T h)
	{
		if (m_handle.valid())
		{
			Gfx_Release(m_handle);
		}

		m_handle = h;
	}

	void retain(T h)
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

typedef GfxRef<GfxVertexFormat>      GfxVertexFormatRef;
typedef GfxRef<GfxVertexShader>      GfxVertexShaderRef;
typedef GfxRef<GfxPixelShader>       GfxPixelShaderRef;
typedef GfxRef<GfxComputeShader>     GfxComputeShaderRef;
typedef GfxRef<GfxTexture>           GfxTextureRef;
typedef GfxRef<GfxBuffer>            GfxBufferRef;
typedef GfxRef<GfxSampler>           GfxSamplerRef;
typedef GfxRef<GfxBlendState>        GfxBlendStateRef;
typedef GfxRef<GfxDepthStencilState> GfxDepthStencilStateRef;
typedef GfxRef<GfxRasterizerState>   GfxRasterizerStateRef;
typedef GfxRef<GfxTechnique>         GfxTechniqueRef;

inline void Gfx_SetTechnique(GfxContext* rc, GfxTechniqueRef h) { Gfx_SetTechnique(rc, h.get()); }

inline void Gfx_SetIndexStream(GfxContext* rc, GfxBufferRef h) { Gfx_SetIndexStream(rc, h.get()); }

inline void Gfx_SetVertexStream(GfxContext* rc, u32 idx, GfxBufferRef h) { Gfx_SetVertexStream(rc, idx, h.get()); }

inline void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTextureRef h)
{
	Gfx_SetTexture(rc, stage, idx, h.get());
}

inline void Gfx_SetSampler(GfxContext* rc, GfxStage stage, u32 idx, GfxSamplerRef h)
{
	Gfx_SetSampler(rc, stage, idx, h.get());
}

inline void Gfx_SetStorageImage(GfxContext* rc, GfxStage stage, u32 idx, GfxTextureRef h)
{
	Gfx_SetStorageImage(rc, stage, idx, h.get());
}

inline void Gfx_SetStorageBuffer(GfxContext* rc, GfxStage stage, u32 idx, GfxBufferRef h)
{
	Gfx_SetStorageBuffer(rc, stage, idx, h.get());
}

inline void Gfx_SetBlendState(GfxContext* rc, GfxBlendStateRef h) { Gfx_SetBlendState(rc, h.get()); }

inline void Gfx_SetDepthStencilState(GfxContext* rc, GfxDepthStencilStateRef h)
{
	Gfx_SetDepthStencilState(rc, h.get());
}

inline void Gfx_SetRasterizerState(GfxContext* rc, GfxRasterizerStateRef h) { Gfx_SetRasterizerState(rc, h.get()); }

inline void Gfx_SetConstantBuffer(GfxContext* rc, u32 index, GfxBufferRef h, size_t offset = 0)
{
	Gfx_SetConstantBuffer(rc, index, h.get(), offset);
}

inline void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTextureRef th, GfxSampler sh)
{
	Gfx_SetTexture(rc, stage, idx, th.get(), sh);
}

inline void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTextureRef th, GfxSamplerRef sh)
{
	Gfx_SetTexture(rc, stage, idx, th.get(), sh.get());
}

inline void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTexture th, GfxSamplerRef sh)
{
	Gfx_SetTexture(rc, stage, idx, th, sh.get());
}

inline const GfxTextureDesc& Gfx_GetTextureDesc(GfxTextureRef h) { return Gfx_GetTextureDesc(h.get()); }

inline void Gfx_UpdateBuffer(GfxContext* rc, GfxBufferRef h, const void* data, u32 size = 0)
{
	Gfx_UpdateBuffer(rc, h.get(), data, size);
}

template <typename T> inline void Gfx_UpdateBuffer(GfxContext* rc, GfxBufferRef h, const T& data)
{
	Gfx_UpdateBuffer(rc, h.get(), &data, sizeof(data));
}

template <typename T> inline u32 Gfx_UpdateBufferT(GfxContext* rc, GfxBufferRef h, const T& data)
{
	return Gfx_UpdateBufferT(rc, h.get(), data);
}

template <typename T> inline u32 Gfx_UpdateBufferT(GfxContext* rc, GfxBufferRef h, const std::vector<T>& data)
{
	return Gfx_UpdateBufferT(rc, h.get(), data);
}

template <typename T> inline T* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferRef h)
{
	return reinterpret_cast<T*>(Gfx_BeginUpdateBuffer(rc, h.get(), sizeof(T)));
}

inline void Gfx_EndUpdateBuffer(GfxContext* rc, GfxBufferRef h) { Gfx_EndUpdateBuffer(rc, h.get()); }

inline void Gfx_AddImageBarrier(GfxContext* rc, GfxTextureRef textureHandle, GfxResourceState desiredState,
    GfxSubresourceRange* subresourceRange = nullptr)
{
	Gfx_AddImageBarrier(rc, textureHandle.get(), desiredState, subresourceRange);
}
}
