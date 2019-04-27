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
	u32    triangles        = 0;
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
	bool        instancing           = false;
	bool        drawIndirect         = false;
	bool        dispatchIndirect     = false;
	bool        shaderInt16          = false;
	bool        shaderInt64          = false;
	bool        shaderWaveIntrinsics = false;
	bool        asyncCompute         = false;

	bool rayTracing                  = false;
	bool geometryShaderPassthroughNV = false;
	bool explicitVertexParameterAMD  = false;

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
	union {
		u64         offset = 0;
		const void* pixels;
	};
	u32 mip    = 0;
	u32 slice  = 0;
	u32 width  = 0;
	u32 height = 0;
	u32 depth  = 0;
};

enum class GfxPassFlags : u32
{
	None = 0,

	ClearColor        = 1 << 0,
	ClearDepthStencil = 1 << 1,

	DiscardColor = 1 << 2,

	ClearAll = ClearColor | ClearDepthStencil,
};
RUSH_IMPLEMENT_FLAG_OPERATORS(GfxPassFlags, u32)

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

GfxOwn<GfxVertexFormat>      Gfx_CreateVertexFormat(const GfxVertexFormatDesc& fmt);
GfxOwn<GfxVertexShader>      Gfx_CreateVertexShader(const GfxShaderSource& code);
GfxOwn<GfxPixelShader>       Gfx_CreatePixelShader(const GfxShaderSource& code);
GfxOwn<GfxGeometryShader>    Gfx_CreateGeometryShader(const GfxShaderSource& code);
GfxOwn<GfxComputeShader>     Gfx_CreateComputeShader(const GfxShaderSource& code);
GfxOwn<GfxTechnique>         Gfx_CreateTechnique(const GfxTechniqueDesc& desc);
GfxOwn<GfxTexture>           Gfx_CreateTexture(const GfxTextureDesc& tex, const GfxTextureData* data = nullptr, u32 count = 0, const void* texels = nullptr);
GfxOwn<GfxBlendState>        Gfx_CreateBlendState(const GfxBlendStateDesc& desc);
GfxOwn<GfxSampler>           Gfx_CreateSamplerState(const GfxSamplerDesc& desc);
GfxOwn<GfxDepthStencilState> Gfx_CreateDepthStencilState(const GfxDepthStencilDesc& desc);
GfxOwn<GfxRasterizerState>   Gfx_CreateRasterizerState(const GfxRasterizerDesc& desc);
GfxOwn<GfxBuffer>            Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data = nullptr);

void Gfx_Retain(GfxDevice* dev);
void Gfx_Retain(GfxContext* rc);
void Gfx_Retain(GfxVertexFormat h);
void Gfx_Retain(GfxVertexShader h);
void Gfx_Retain(GfxPixelShader h);
void Gfx_Retain(GfxGeometryShader h);
void Gfx_Retain(GfxComputeShader h);
void Gfx_Retain(GfxTechnique h);
void Gfx_Retain(GfxTexture h);
void Gfx_Retain(GfxBlendState h);
void Gfx_Retain(GfxSampler h);
void Gfx_Retain(GfxDepthStencilState h);
void Gfx_Retain(GfxRasterizerState h);
void Gfx_Retain(GfxBuffer h);

void Gfx_Release(GfxVertexFormat h);
void Gfx_Release(GfxVertexShader h);
void Gfx_Release(GfxPixelShader h);
void Gfx_Release(GfxGeometryShader h);
void Gfx_Release(GfxComputeShader h);
void Gfx_Release(GfxTechnique h);
void Gfx_Release(GfxTexture h);
void Gfx_Release(GfxBlendState h);
void Gfx_Release(GfxSampler h);
void Gfx_Release(GfxDepthStencilState h);
void Gfx_Release(GfxRasterizerState h);
void Gfx_Release(GfxBuffer h);

const GfxTextureDesc&  Gfx_GetTextureDesc(GfxTexture h);
GfxTexture             Gfx_GetBackBufferColorTexture();
GfxTexture             Gfx_GetBackBufferDepthTexture();

GfxMappedBuffer Gfx_MapBuffer(GfxBufferArg h, u32 offset = 0, u32 size = 0);
void            Gfx_UnmapBuffer(GfxMappedBuffer& lock);
void            Gfx_UpdateBuffer(GfxContext* rc, GfxBufferArg h, const void* data, u32 size = 0);
void*           Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferArg h, u32 size);
void            Gfx_EndUpdateBuffer(GfxContext* rc, GfxBufferArg h);
void            Gfx_DestroyBuffer(GfxBufferArg h);

GfxContext* Gfx_AcquireContext();
void        Gfx_Release(GfxContext* rc);

GfxContext* Gfx_BeginAsyncCompute(GfxContext* ctx);
void        Gfx_EndAsyncCompute(GfxContext* parentContext, GfxContext* asyncContext);

void Gfx_BeginPass(GfxContext* rc, const GfxPassDesc& desc);
void Gfx_EndPass(GfxContext* rc);

void Gfx_Clear(GfxContext* rc, ColorRGBA8 color, GfxClearFlags clearFlags = GfxClearFlags::All, float depth = 1.0f, u32 stencil = 0);
void Gfx_SetViewport(GfxContext* rc, const GfxViewport& _viewport);
void Gfx_SetScissorRect(GfxContext* rc, const GfxRect& rect);
void Gfx_SetTechnique(GfxContext* rc, GfxTechniqueArg h);
void Gfx_SetPrimitive(GfxContext* rc, GfxPrimitive type);
void Gfx_SetIndexStream(GfxContext* rc, GfxBufferArg h);
void Gfx_SetVertexStream(GfxContext* rc, u32 idx, GfxBufferArg h);
void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTextureArg h);
void Gfx_SetSampler(GfxContext* rc, GfxStage stage, u32 idx, GfxSamplerArg h);
void Gfx_SetStorageImage(GfxContext* rc, u32 idx, GfxTextureArg h);
void Gfx_SetStorageBuffer(GfxContext* rc, u32 idx, GfxBufferArg h);
void Gfx_SetBlendState(GfxContext* rc, GfxBlendStateArg nextState);
void Gfx_SetDepthStencilState(GfxContext* rc, GfxDepthStencilStateArg nextState);
void Gfx_SetRasterizerState(GfxContext* rc, GfxRasterizerStateArg nextState);
void Gfx_SetConstantBuffer(GfxContext* rc, u32 index, GfxBufferArg h, size_t offset = 0);
void Gfx_AddImageBarrier(GfxContext* rc, GfxTextureArg textureHandle, GfxResourceState desiredState,
    GfxSubresourceRange* subresourceRange = nullptr);

void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ);
void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ, const void* pushConstants, u32 pushConstantsSize);

void Gfx_Draw(GfxContext* rc, u32 firstVertex, u32 vertexCount);
void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount);
void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
    const void* pushConstants, u32 pushConstantsSize);
void Gfx_DrawIndexedInstanced(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
    u32 instanceCount, u32 instanceOffset);

void Gfx_DrawIndexedIndirect(GfxContext* rc, GfxBufferArg argsBuffer, size_t argsBufferOffset, u32 drawCount);
void Gfx_DispatchIndirect(GfxContext* rc, GfxBufferArg argsBuffer, size_t argsBufferOffset,
    const void* pushConstants = nullptr, u32 pushConstantsSize = 0);

void Gfx_PushMarker(GfxContext* rc, const char* marker);
void Gfx_PopMarker(GfxContext* rc);

void Gfx_BeginTimer(GfxContext* rc, u32 timestampId);
void Gfx_EndTimer(GfxContext* rc, u32 timestampId);

using GfxScreenshotCallback = void (*)(const ColorRGBA8* pixels, Tuple2u size, void* userData);
void Gfx_RequestScreenshot(GfxScreenshotCallback callback, void* userData = nullptr);

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

inline GfxOwn<GfxTexture> Gfx_CreateTexture(const GfxTextureDesc& desc, const void* pixels)
{
	GfxTextureData data;
	return Gfx_CreateTexture(desc, &data, 1, pixels);
}

inline GfxOwn<GfxTexture> Gfx_CreateTexture(const GfxTextureDesc& desc, const GfxTextureData& data)
{
	return Gfx_CreateTexture(desc, &data, 1);
}

inline GfxOwn<GfxTexture> Gfx_CreateTexture(const GfxTextureDesc& desc, const std::initializer_list<GfxTextureData>& data)
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

template <typename T> inline u32 Gfx_UpdateBufferT(GfxContext* rc, GfxBufferArg h, const T& data)
{
	Gfx_UpdateBuffer(rc, h, &data, sizeof(data));
	return (u32)sizeof(data);
}

template <typename T> inline u32 Gfx_UpdateBufferT(GfxContext* rc, GfxBufferArg h, const DynamicArray<T>& data)
{
	u32 dataSize = (u32)(data.size() * sizeof(data[0]));
	Gfx_UpdateBuffer(rc, h, data.data(), dataSize);
	return dataSize;
}

template <typename T> inline T* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferArg h)
{
	return reinterpret_cast<T*>(Gfx_BeginUpdateBuffer(rc, h, sizeof(T)));
}

#ifndef RUSH_RENDER_SUPPORT_IMAGE_BARRIERS
inline void Gfx_AddImageBarrier(
    GfxContext* rc, GfxTextureArg textureHandle, GfxResourceState desiredState, GfxSubresourceRange* subresourceRange)
{
}
#endif

#ifndef RUSH_RENDER_SUPPORT_ASYNC_COMPUTE
inline GfxContext* Gfx_BeginAsyncCompute(GfxContext*) { return nullptr; }
inline void        Gfx_EndAsyncCompute(GfxContext*, GfxContext*){};
#endif
}
