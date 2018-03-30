#pragma once

#include "GfxCommon.h"
#include "UtilColor.h"

namespace Rush
{

struct GfxShaderSource;
class GfxContext;
class GfxDevice;

struct GfxStats
{
	u32    drawCalls        = 0;
	u32    vertices         = 0;
	double lastFrameGpuTime = 0.0; // in seconds

	enum
	{
		MaxCustomTimers = 16
	};
	double customTimer[MaxCustomTimers] = {};
};

struct GfxMappedBuffer
{
	void*     data = nullptr;
	u32       size = 0;
	GfxBuffer handle;
};

struct GfxMappedTexture
{
	void*      data = nullptr;
	u32        size = 0;
	GfxTexture handle;
};

struct GfxConfig
{
	GfxConfig() = default;
	GfxConfig(const AppConfig& cfg)
	{
		backBufferWidth  = cfg.maxWidth > 0 ? cfg.maxWidth : cfg.width;
		backBufferHeight = cfg.maxHeight > 0 ? cfg.maxHeight : cfg.height;
		useFullScreen    = cfg.fullScreen;
		presentInterval  = cfg.vsync;
		debug            = cfg.debug;
		warp             = cfg.warp;
		minimizeLatency  = cfg.minimizeLatency;
	}

	u32  backBufferWidth  = 640;
	u32  backBufferHeight = 480;
	u32  presentInterval  = 0;
	bool useFullScreen    = false;
	bool debug            = false;
	bool warp             = false;
	bool minimizeLatency  = false;

	enum PreferredCoordinateSystem
	{
		PreferredCoordinateSystem_Default,  // whatever is the default for current graphics API
		PreferredCoordinateSystem_Direct3D, // XY top-left: [-1, +1], Z range  0 to +1, texture top-left: [0, 0]
		PreferredCoordinateSystem_Vulkan,   // XY top-left: [-1, -1], Z range  0 to +1, texture top-left: [0, 0]
		PreferredCoordinateSystem_OpenGL,   // XY top-left: [-1, +1], Z range -1 to +1, texture top-left: [0, 1]
	};

	// Coordinate system will be configured according to this preference.
	// This does not provide any guarantees and device capability structure must be checked
	// to detect the actual coordinate system conventions used.
	PreferredCoordinateSystem preferredCoordinateSystem = PreferredCoordinateSystem_Default;
};

struct GfxCapability
{
	const char* apiName              = nullptr;
	bool        looseConstants       = false;
	bool        constantBuffers      = false;
	bool        debugOutput          = false;
	bool        debugMarkers         = false;
	bool        interopOpenCL        = false;
	bool        compute              = false;
	bool        combinedSamplers     = false;
	bool        separateSamplers     = false;
	bool        instancing           = false;
	bool        drawIndirect         = false;
	bool        dispatchIndirect     = false;
	bool        shaderInt16          = false;
	bool        shaderInt64          = false;
	bool        shaderWaveIntrinsics = false;
	bool        asyncCompute         = false;

	float deviceFarDepth  = 1.0f;
	float deviceNearDepth = 0.0f;
	Vec2  deviceTopLeft   = Vec2(-1.0f, -1.0f);
	Vec2  textureTopLeft  = Vec2(0.0f, 0.0f);

	u32 shaderTypeMask  = 0;
	u32 threadGroupSize = 64;

	ProjectionFlags projectionFlags = ProjectionFlags::Default;

	bool shaderTypeSupported(GfxShaderSourceType type) const { return (shaderTypeMask & (1 << type)) != 0; }
};

struct GfxTextureData
{
	GfxTextureData() = default;
	GfxTextureData(const void* pixels, u32 mip = 0, u32 slice = 0) : pixels(pixels), mip(mip), slice(slice) {}

	const void* pixels = nullptr;
	u32         mip    = 0;
	u32         slice  = 0;
	u32         pitch  = 0;
	u32         width  = 0;
	u32         height = 0;
	u32         depth  = 0;
};

enum class GfxPassFlags : u32
{
	None = 0,

	ClearColor        = 1 << 0,
	ClearDepthStencil = 1 << 1,

	DiscardColor = 1 << 2,

	ClearAll = ClearColor | ClearDepthStencil,
};
RUSH_IMPLEMENT_FLAG_OPERATORS(GfxPassFlags, u32);

struct GfxPassDesc
{
	static const u32 MaxTargets = 8;

	GfxTexture   color[MaxTargets];
	GfxTexture   depth;
	GfxPassFlags flags                   = GfxPassFlags::None;
	ColorRGBA    clearColors[MaxTargets] = {};
	float        clearDepth              = 1.0f;
	u8           clearStencil            = 0xFF;

	u32 getColorTargetCount() const
	{
		u32 count = 0;
		for (u32 i = 0; i < MaxTargets && color[i].valid(); ++i)
		{
			count = i + 1;
		}
		return count;
	}
};

// device

GfxDevice* Gfx_CreateDevice(Window* window, const GfxConfig& cfg);
void       Gfx_Release(GfxDevice* dev);

void                 Gfx_BeginFrame();
void                 Gfx_EndFrame();
void                 Gfx_Present();
void                 Gfx_SetPresentInterval(u32 interval);
const GfxCapability& Gfx_GetCapability();
void                 Gfx_Finish();

const GfxStats& Gfx_Stats();
void            Gfx_ResetStats();

// vertex format
GfxVertexFormat Gfx_CreateVertexFormat(const GfxVertexFormatDesc& fmt);
void            Gfx_DestroyVertexFormat(GfxVertexFormat h);

// vertex shader
GfxVertexShader Gfx_CreateVertexShader(const GfxShaderSource& code);
void            Gfx_DestroyVertexShader(GfxVertexShader vsh);

// pixel shader
GfxPixelShader Gfx_CreatePixelShader(const GfxShaderSource& code);
void           Gfx_DestroyPixelShader(GfxPixelShader psh);

// compute shader
GfxComputeShader Gfx_CreateComputeShader(const GfxShaderSource& code);
void             Gfx_DestroyComputeShader(GfxComputeShader psh);

// technique
GfxTechnique Gfx_CreateTechnique(const GfxTechniqueDesc& desc);
void         Gfx_DestroyTechnique(GfxTechnique h);

// texture
GfxTexture            Gfx_CreateTextureFromFile(const char* filename, TextureType type = TextureType::Tex2D);
GfxTexture            Gfx_CreateTexture(const GfxTextureDesc& tex, const GfxTextureData* data = nullptr, u32 count = 0);
const GfxTextureDesc& Gfx_GetTextureDesc(GfxTexture h);
void                  Gfx_DestroyTexture(GfxTexture th);

GfxTexture Gfx_GetBackBufferColorTexture();
GfxTexture Gfx_GetBackBufferDepthTexture();

// blend state
GfxBlendState Gfx_CreateBlendState(const GfxBlendStateDesc& desc);
void          Gfx_DestroyBlendState(GfxBlendState h);

// sampler state
GfxSampler Gfx_CreateSamplerState(const GfxSamplerDesc& desc);
void       Gfx_DestroySamplerState(GfxSampler h);

// depth stencil state
GfxDepthStencilState Gfx_CreateDepthStencilState(const GfxDepthStencilDesc& desc);
void                 Gfx_DestroyDepthStencilState(GfxDepthStencilState h);

// rasterizer state
GfxRasterizerState Gfx_CreateRasterizerState(const GfxRasterizerDesc& desc);
void               Gfx_DestroyRasterizerState(GfxRasterizerState h);

// buffers
GfxBuffer       Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data = nullptr);
GfxMappedBuffer Gfx_MapBuffer(GfxBuffer h, u32 offset = 0, u32 size = 0);
void            Gfx_UnmapBuffer(GfxMappedBuffer& lock);
void            Gfx_UpdateBuffer(GfxContext* rc, GfxBuffer h, const void* data, u32 size = 0);
void*           Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBuffer h, u32 size);
void            Gfx_EndUpdateBuffer(GfxContext* rc, GfxBuffer h);
void            Gfx_DestroyBuffer(GfxBuffer h);

// context
GfxContext* Gfx_AcquireContext();
void        Gfx_Release(GfxContext* rc);

GfxContext* Gfx_BeginAsyncCompute(GfxContext* ctx);
void        Gfx_EndAsyncCompute(GfxContext* parentContext, GfxContext* asyncContext);

void Gfx_BeginPass(GfxContext* rc, const GfxPassDesc& desc);
void Gfx_EndPass(GfxContext* rc);

void Gfx_Clear(GfxContext* rc, ColorRGBA8 color, GfxClearFlags clearFlags = GfxClearFlags::All, float depth = 1.0f,
    u32 stencil = 0);
void Gfx_SetViewport(GfxContext* rc, const GfxViewport& _viewport);
void Gfx_SetScissorRect(GfxContext* rc, const GfxRect& rect);
void Gfx_SetTechnique(GfxContext* rc, GfxTechnique h);
void Gfx_SetPrimitive(GfxContext* rc, GfxPrimitive type);
void Gfx_SetIndexStream(GfxContext* rc, GfxBuffer h);
void Gfx_SetVertexStream(GfxContext* rc, u32 idx, GfxBuffer h);
void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTexture h);
void Gfx_SetSampler(GfxContext* rc, GfxStage stage, u32 idx, GfxSampler h);
void Gfx_SetStorageImage(GfxContext* rc, u32 idx, GfxTexture h);
void Gfx_SetStorageBuffer(GfxContext* rc, u32 idx, GfxBuffer h);
void Gfx_SetBlendState(GfxContext* rc, GfxBlendState nextState);
void Gfx_SetDepthStencilState(GfxContext* rc, GfxDepthStencilState nextState);
void Gfx_SetRasterizerState(GfxContext* rc, GfxRasterizerState nextState);
void Gfx_SetConstantBuffer(GfxContext* rc, u32 index, GfxBuffer h, size_t offset = 0);
void Gfx_AddImageBarrier(GfxContext* rc, GfxTexture textureHandle, GfxResourceState desiredState,
    GfxSubresourceRange* subresourceRange = nullptr);

void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ);
void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ, const void* pushConstants, u32 pushConstantsSize);

void Gfx_Draw(GfxContext* rc, u32 firstVertex, u32 vertexCount);
void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount);
void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
    const void* pushConstants, u32 pushConstantsSize);
void Gfx_DrawIndexedInstanced(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
    u32 instanceCount, u32 instanceOffset);

void Gfx_DrawIndexedIndirect(GfxContext* rc, GfxBuffer argsBuffer, size_t argsBufferOffset, u32 drawCount);
void Gfx_DispatchIndirect(GfxContext* rc, GfxBuffer argsBuffer, size_t argsBufferOffset,
    const void* pushConstants = nullptr, u32 pushConstantsSize = 0);

void Gfx_PushMarker(GfxContext* rc, const char* marker);
void Gfx_PopMarker(GfxContext* rc);

void Gfx_BeginTimer(GfxContext* rc, u32 timestampId);
void Gfx_EndTimer(GfxContext* rc, u32 timestampId);

using GfxScreenshotCallback = void (*)(const ColorRGBA8* pixels, Tuple2u size, void* userData);
void Gfx_RequestScreenshot(GfxScreenshotCallback callback, void* userData = nullptr);

//

void Gfx_Retain(GfxDevice* dev);
void Gfx_Retain(GfxContext* rc);
void Gfx_Retain(GfxVertexFormat h);
void Gfx_Retain(GfxVertexShader h);
void Gfx_Retain(GfxPixelShader h);
void Gfx_Retain(GfxComputeShader h);
void Gfx_Retain(GfxTechnique h);
void Gfx_Retain(GfxTexture h);
void Gfx_Retain(GfxBlendState h);
void Gfx_Retain(GfxSampler h);
void Gfx_Retain(GfxDepthStencilState h);
void Gfx_Retain(GfxRasterizerState h);
void Gfx_Retain(GfxBuffer h);

inline void Gfx_Release(GfxVertexFormat h) { Gfx_DestroyVertexFormat(h); }
inline void Gfx_Release(GfxVertexShader h) { Gfx_DestroyVertexShader(h); }
inline void Gfx_Release(GfxPixelShader h) { Gfx_DestroyPixelShader(h); }
inline void Gfx_Release(GfxComputeShader h) { Gfx_DestroyComputeShader(h); }
inline void Gfx_Release(GfxTechnique h) { Gfx_DestroyTechnique(h); }
inline void Gfx_Release(GfxTexture h) { Gfx_DestroyTexture(h); }
inline void Gfx_Release(GfxBlendState h) { Gfx_DestroyBlendState(h); }
inline void Gfx_Release(GfxSampler h) { Gfx_DestroySamplerState(h); }
inline void Gfx_Release(GfxDepthStencilState h) { Gfx_DestroyDepthStencilState(h); }
inline void Gfx_Release(GfxRasterizerState h) { Gfx_DestroyRasterizerState(h); }
inline void Gfx_Release(GfxBuffer h) { Gfx_DestroyBuffer(h); }

inline void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTexture th, GfxSampler sh)
{
	Gfx_SetTexture(rc, stage, idx, th);
	Gfx_SetSampler(rc, stage, idx, sh);
}

template <typename T> inline void Gfx_SetViewport(GfxContext* rc, const Tuple2<T>& size)
{
	GfxViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.w = (float)size.x;
	viewport.h = (float)size.y;
	Gfx_SetViewport(rc, viewport);
}

template <typename T> inline void Gfx_SetScissorRect(GfxContext* rc, const Tuple2<T>& size)
{
	GfxRect rect;
	rect.left   = 0;
	rect.top    = 0;
	rect.right  = (int)size.x;
	rect.bottom = (int)size.y;
	Gfx_SetScissorRect(rc, rect);
}

inline GfxTexture Gfx_CreateTexture(const GfxTextureDesc& desc, const void* pixels)
{
	GfxTextureData data(pixels);
	return Gfx_CreateTexture(desc, &data, 1);
}

inline GfxTexture Gfx_CreateTexture(const GfxTextureDesc& desc, const GfxTextureData& data)
{
	return Gfx_CreateTexture(desc, &data, 1);
}

inline GfxTexture Gfx_CreateTexture(const GfxTextureDesc& desc, const std::initializer_list<GfxTextureData>& data)
{
	return Gfx_CreateTexture(desc, data.begin(), (u32)data.size());
}

struct GfxMarkerScope
{
	GfxMarkerScope(GfxContext* rc, const char* marker) : m_rc(rc) { Gfx_PushMarker(m_rc, marker); }
	~GfxMarkerScope() { Gfx_PopMarker(m_rc); }

	GfxContext* m_rc;
};

struct GfxTimerScope
{
	GfxTimerScope(GfxContext* rc, u32 timestampId) : m_rc(rc), m_timestampId(timestampId)
	{
		Gfx_BeginTimer(m_rc, m_timestampId);
	}
	~GfxTimerScope() { Gfx_EndTimer(m_rc, m_timestampId); }

	GfxContext* m_rc;
	u32         m_timestampId;
};

template <typename T> inline u32 Gfx_UpdateBufferT(GfxContext* rc, GfxBuffer h, const T& data)
{
	Gfx_UpdateBuffer(rc, h, &data, 0, sizeof(data), true);
	return (u32)sizeof(data);
}

template <typename T> inline u32 Gfx_UpdateBufferT(GfxContext* rc, GfxBuffer h, const std::vector<T>& data)
{
	u32 dataSize = (u32)(data.size() * sizeof(data[0]));
	Gfx_UpdateBuffer(rc, h, data.data(), dataSize);
	return dataSize;
}

template <typename T> inline T* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBuffer h)
{
	return reinterpret_cast<T*>(Gfx_BeginUpdateBuffer(rc, h, sizeof(T)));
}

#ifndef RUSH_RENDER_SUPPORT_IMAGE_BARRIERS
inline void Gfx_AddImageBarrier(
    GfxContext* rc, GfxTexture textureHandle, GfxResourceState desiredState, GfxSubresourceRange* subresourceRange)
{
}
#endif

#ifndef RUSH_RENDER_SUPPORT_ASYNC_COMPUTE
inline GfxContext* Gfx_BeginAsyncCompute(GfxContext*) { return nullptr; }
inline void        Gfx_EndAsyncCompute(GfxContext*, GfxContext*){};
#endif
}
