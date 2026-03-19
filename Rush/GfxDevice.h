#pragma once

#include "GfxCommon.h"
#include "Platform.h"
#include "UtilColor.h"
#include "RushC.h"

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
		headless         = cfg.headless;
	}

	u32  backBufferWidth  = 640;
	u32  backBufferHeight = 480;
	u32  presentInterval  = 0;
	bool useFullScreen    = false;
	bool debug            = false;
	bool warp             = false;
	bool minimizeLatency  = false;
	bool headless         = false;
};

struct GfxCapability
{
	const char* apiName              = nullptr;
	bool        debugOutput          = false;
	bool        debugMarkers         = false;
	bool        compute              = false;
	bool        instancing           = false;
	bool        drawIndirect         = false;
	bool        dispatchIndirect     = false;
	bool        shaderInt16          = false;
	bool        shaderInt64          = false;
	bool        shaderWaveIntrinsics = false;
	bool        asyncCompute         = false;
	bool        sampleLocations      = false;
	bool        pushConstants        = false;
	bool        descriptorIndexing   = false;

	bool explicitVertexParameterAMD  = false;

	bool rayTracing                  = false;
	bool rayTracingPipeline          = false;
	bool rayTracingInline            = false;
	bool geometryShaderPassthroughNV = false;
	bool mixedSamplesNV              = false;
	bool meshShaderNV                = false;

	float deviceFarDepth  = 1.0f;
	float deviceNearDepth = 0.0f;

	u32 shaderTypeMask  = 0;
	u32 threadGroupSize = 64;

	u32 colorSampleCounts = 1;
	u32 depthSampleCounts = 1;

	u32 constantBufferAlignment = 4;

	u32 rtShaderHandleSize = 0;
	u32 rtSbtMaxStride = 0;
	u32 rtSbtAlignment = 0;

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
	None = RUSH_GFX_PASS_NONE,

	ClearColor        = RUSH_GFX_PASS_CLEAR_COLOR,
	ClearDepthStencil = RUSH_GFX_PASS_CLEAR_DEPTH_STENCIL,

	DiscardColor = RUSH_GFX_PASS_DISCARD_COLOR,

	ClearAll = ClearColor | ClearDepthStencil,
};
RUSH_IMPLEMENT_FLAG_OPERATORS(GfxPassFlags, u32)

struct GfxPassDesc
{
	static constexpr u32 MaxTargets = 8;

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

enum class GfxFinishFlags : u32
{
	None              = 0,
	ResolveTimestamps = 1 << 0,
};
RUSH_IMPLEMENT_FLAG_OPERATORS(GfxFinishFlags, u32)

// device

GfxDevice* Gfx_CreateDevice(Window* window, const GfxConfig& cfg);
void       Gfx_Release(GfxDevice* dev);

void                 Gfx_BeginFrame();
void                 Gfx_EndFrame();
GfxProgressId        Gfx_Present();                // submit commands + present swapchain, return progress ID
void                 Gfx_SetPresentInterval(u32 interval);
const GfxCapability& Gfx_GetCapability();

// GPU progress tracking: Submit/Present return a GfxProgressId.
// QueryProgress can poll (no flags), wait for a specific ID (Wait), or drain all work (Idle).
// ResolveTimestamps requires the relevant submit to have completed.
GfxProgressId        Gfx_Submit();                // flush pending commands to GPU, return progress ID
GfxProgressId        Gfx_GetPendingProgressId();  // return the most recently submitted progress ID
GfxProgressStatus    Gfx_QueryProgress(GfxProgressId id, GfxProgressFlags flags = GfxProgressFlags::None);
void                 Gfx_ResolveTimestamps();     // read back GPU timestamp query results

// Submit and wait for all GPU work to complete
inline void Gfx_Finish(GfxFinishFlags flags = GfxFinishFlags::None)
{
	GfxProgressId id = Gfx_Submit();
	Gfx_QueryProgress(id, GfxProgressFlags::Idle);
	if (!!(flags & GfxFinishFlags::ResolveTimestamps))
	{
		Gfx_ResolveTimestamps();
	}
}

const GfxStats& Gfx_Stats();
void            Gfx_ResetStats();

GfxOwn<GfxVertexFormat>      Gfx_CreateVertexFormat(const GfxVertexFormatDesc& fmt);
GfxOwn<GfxVertexShader>      Gfx_CreateVertexShader(const GfxShaderSource& code);
GfxOwn<GfxPixelShader>       Gfx_CreatePixelShader(const GfxShaderSource& code);
GfxOwn<GfxGeometryShader>    Gfx_CreateGeometryShader(const GfxShaderSource& code);
GfxOwn<GfxComputeShader>     Gfx_CreateComputeShader(const GfxShaderSource& code);
GfxOwn<GfxMeshShader>        Gfx_CreateMeshShader(const GfxShaderSource& code);
GfxOwn<GfxTechnique>         Gfx_CreateTechnique(const GfxTechniqueDesc& desc);
GfxOwn<GfxTexture>           Gfx_CreateTexture(const GfxTextureDesc& tex, const GfxTextureData* data = nullptr, u32 count = 0, const void* texels = nullptr);
GfxOwn<GfxBlendState>        Gfx_CreateBlendState(const GfxBlendStateDesc& desc);
GfxOwn<GfxSampler>           Gfx_CreateSamplerState(const GfxSamplerDesc& desc);
GfxOwn<GfxDepthStencilState> Gfx_CreateDepthStencilState(const GfxDepthStencilDesc& desc);
GfxOwn<GfxRasterizerState>   Gfx_CreateRasterizerState(const GfxRasterizerDesc& desc);
GfxOwn<GfxBuffer>            Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data = nullptr);

#ifdef RUSH_RENDER_SUPPORT_QUERY
GfxOwn<GfxQueryPool> Gfx_CreateQueryPool(const GfxQueryPoolDesc& desc);
void                 Gfx_ResetQuery(GfxContext* ctx, GfxQueryPool pool, u32 index, u32 count);
void                 Gfx_BeginQuery(
                    GfxContext* ctx, GfxQueryPool pool, u32 index, GfxQueryControlFlags flags = GfxQueryControlFlags::None);
void Gfx_EndQuery(GfxContext* ctx, GfxQueryPool pool, u32 index);
bool Gfx_GetQueryResults(
    GfxQueryPool pool, u32 index, u32 count, size_t dataSize, void* outData, u32 stride, GfxQueryResultFlags flags);
#endif // RUSH_RENDER_SUPPORT_QUERY

GfxOwn<GfxRayTracingPipeline>    Gfx_CreateRayTracingPipeline(const GfxRayTracingPipelineDesc& desc);
GfxOwn<GfxAccelerationStructure> Gfx_CreateAccelerationStructure(const GfxAccelerationStructureDesc& desc);
const u8*                        Gfx_GetRayTracingShaderHandle(GfxRayTracingPipelineArg h, GfxRayTracingShaderType type, u32 index);
u64                              Gfx_GetAccelerationStructureHandle(GfxAccelerationStructureArg h);
void                             Gfx_BuildAccelerationStructure(GfxContext* ctx, GfxAccelerationStructureArg h, GfxBufferArg instanceBuffer = InvalidResourceHandle());
void                             Gfx_SetAccelerationStructure(GfxContext* ctx, u32 idx, GfxAccelerationStructureArg h);
void                             Gfx_TraceRays(GfxContext* ctx, GfxRayTracingPipelineArg pipeline, GfxBufferArg hitGroups, u32 width, u32 height = 1, u32 depth = 1);

void Gfx_Retain(GfxDevice* dev);
void Gfx_Retain(GfxContext* rc);

#define RUSH_GFX_RETAIN_DECL(descType, handleType, memberName) void Gfx_Retain(handleType h);
RUSH_GFX_RESOURCE_LIST(RUSH_GFX_RETAIN_DECL)
#undef RUSH_GFX_RETAIN_DECL

#define RUSH_GFX_RELEASE_DECL(descType, handleType, memberName) void Gfx_Release(handleType h);
RUSH_GFX_RESOURCE_LIST(RUSH_GFX_RELEASE_DECL)
#undef RUSH_GFX_RELEASE_DECL

#ifdef RUSH_RENDER_SUPPORT_DESCRIPTOR_SETS
GfxOwn<GfxDescriptorSet> Gfx_CreateDescriptorSet(const GfxDescriptorSetDesc& desc);
void Gfx_SetDescriptors(GfxContext* rc, u32 index, GfxDescriptorSetArg h);
void Gfx_UpdateDescriptorSet(GfxDescriptorSetArg d,
	const GfxBuffer* constantBuffers = nullptr,
	const GfxSampler* samplers = nullptr,
	const GfxTexture* textures = nullptr,
	const GfxTexture* storageImages = nullptr,
	const GfxBuffer* storageBuffers = nullptr,
	const GfxAccelerationStructure* accelStructures = nullptr);
#endif // RUSH_RENDER_SUPPORT_DESCRIPTOR_SETS

const GfxTextureDesc&  Gfx_GetTextureDesc(GfxTextureArg h);

GfxMappedBuffer Gfx_MapBuffer(GfxBufferArg h, u32 offset = 0, u32 size = 0);
void            Gfx_UnmapBuffer(GfxMappedBuffer& lock);
void            Gfx_UpdateBuffer(GfxContext* rc, GfxBufferArg h, const void* data, u32 size = 0);
void*           Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferArg h, u32 size);
void            Gfx_EndUpdateBuffer(GfxContext* rc, GfxBufferArg h);

#ifdef RUSH_RENDER_SUPPORT_BUFFER_ADDRESS
u64 Gfx_GetBufferAddress(GfxBufferArg h);
#else // RUSH_RENDER_SUPPORT_BUFFER_ADDRESS
inline u64 Gfx_GetBufferAddress(GfxBufferArg) { return 0; }
#endif // RUSH_RENDER_SUPPORT_BUFFER_ADDRESS

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
void Gfx_SetIndexStream(GfxContext* rc, u32 offset, GfxFormat format, GfxBufferArg h);
void Gfx_SetIndexStream(GfxContext* rc, GfxBufferArg h);
void Gfx_SetVertexStream(GfxContext* rc, u32 idx, u32 offset, u32 stride, GfxBufferArg h);
void Gfx_SetVertexStream(GfxContext* rc, u32 idx, GfxBufferArg h);
void Gfx_SetTexture(GfxContext* rc, u32 idx, GfxTextureArg h);
void Gfx_SetSampler(GfxContext* rc, u32 idx, GfxSamplerArg h);
void Gfx_SetStorageImage(GfxContext* rc, u32 idx, GfxTextureArg h);
void Gfx_SetStorageBuffer(GfxContext* rc, u32 idx, GfxBufferArg h);
void Gfx_SetBlendState(GfxContext* rc, GfxBlendStateArg nextState);
void Gfx_SetDepthStencilState(GfxContext* rc, GfxDepthStencilStateArg nextState);
void Gfx_SetRasterizerState(GfxContext* rc, GfxRasterizerStateArg nextState);
void Gfx_SetConstantBuffer(GfxContext* rc, u32 index, GfxBufferArg h, size_t offset = 0);
void Gfx_FlushBarriers(GfxContext* rc);
void Gfx_AddFullPipelineBarrier(GfxContext* rc);
void Gfx_AddImageBarrier(GfxContext* rc, GfxTextureArg textureHandle, GfxResourceState desiredState,
    GfxSubresourceRange* subresourceRange = nullptr);
void Gfx_ResolveImage(GfxContext* rc, GfxTextureArg src, GfxTextureArg dst);

GfxImageCopyInfo Gfx_GetImageCopyInfo(GfxFormat format, Tuple3u size);

GfxImageCopyInfo Gfx_CopyTextureToBuffer(
    GfxContext*           ctx,
    GfxTextureArg         src,
    const GfxImageRegion& srcRegion,
    GfxBufferArg          dst,
    u64                   dstOffset = 0);

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

void Gfx_DrawMesh(GfxContext* rc, u32 taskCount, u32 firstTask, const void* pushConstants, u32 pushConstantsSize);

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

inline void Gfx_SetVertexStream(GfxContext* rc, u32 idx, GfxBufferArg h) { Gfx_SetVertexStream(rc, idx, 0, ~0u, h); }
inline void Gfx_SetIndexStream(GfxContext* rc, GfxBufferArg h) { Gfx_SetIndexStream(rc, 0, GfxFormat_Unknown, h); }

inline GfxOwn<GfxBuffer> Gfx_CreateBuffer(GfxBufferFlags flags, GfxFormat format, u32 count = 0, u32 stride = 0, const void* data = nullptr)
{
	return Gfx_CreateBuffer(GfxBufferDesc(flags, format, count, stride), data);
}

inline GfxOwn<GfxBuffer> Gfx_CreateBuffer(GfxBufferFlags flags, u32 count = 0, u32 stride = 0, const void* data = nullptr)
{
	return Gfx_CreateBuffer(GfxBufferDesc(flags, count, stride), data);
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

template <typename T> inline T* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferArg h, u32 arrayCount=1)
{
	return reinterpret_cast<T*>(Gfx_BeginUpdateBuffer(rc, h, u32(sizeof(T)* arrayCount)));
}

#ifndef RUSH_RENDER_SUPPORT_BARRIERS
inline void Gfx_FlushBarriers(GfxContext*) {};
inline void Gfx_AddFullPipelineBarrier(GfxContext*) {};
inline void Gfx_AddImageBarrier(
    GfxContext* rc, GfxTextureArg textureHandle, GfxResourceState desiredState, GfxSubresourceRange* subresourceRange)
{
}
#endif // RUSH_RENDER_SUPPORT_BARRIERS

#ifndef RUSH_RENDER_SUPPORT_ASYNC_COMPUTE
inline GfxContext* Gfx_BeginAsyncCompute(GfxContext*) { return nullptr; }
inline void        Gfx_EndAsyncCompute(GfxContext*, GfxContext*) {}
#endif //RUSH_RENDER_SUPPORT_ASYNC_COMPUTE

#ifndef RUSH_RENDER_SUPPORT_MESH_SHADER
inline GfxOwn<GfxMeshShader> Gfx_CreateMeshShader(const GfxShaderSource& code) { return InvalidResourceHandle(); };
inline void Gfx_DrawMesh(GfxContext* rc, u32 taskCount, u32 firstTask, const void* pushConstants, u32 pushConstantsSize) {};
#endif // RUSH_RENDER_SUPPORT_MESH_SHADER

#ifndef RUSH_RENDER_SUPPORT_RAY_TRACING
inline GfxOwn<GfxRayTracingPipeline> Gfx_CreateRayTracingPipeline(const GfxRayTracingPipelineDesc& desc) { return {}; }
inline GfxOwn<GfxAccelerationStructure> Gfx_CreateAccelerationStructure(const GfxAccelerationStructureDesc& desc) { return {}; }
inline const u8* Gfx_GetRayTracingShaderHandle(GfxRayTracingPipelineArg h, GfxRayTracingShaderType type, u32 index) { return {}; }
inline u64  Gfx_GetAccelerationStructureHandle(GfxAccelerationStructureArg h) { return 0; }
inline void Gfx_BuildAccelerationStructure(GfxContext* ctx, GfxAccelerationStructureArg h, GfxBufferArg instanceBuffer) {}
inline void Gfx_SetAccelerationStructure(GfxContext* ctx, u32 idx, GfxAccelerationStructureArg h) {}
inline void Gfx_TraceRays(GfxContext* ctx, GfxRayTracingPipelineArg pipeline, GfxBufferArg hitGroups, u32 width, u32 height, u32 depth) {}
#endif // RUSH_RENDER_SUPPORT_RAY_TRACING

// Null render API implementation

#if RUSH_RENDER_API == RUSH_RENDER_API_NULL
inline GfxDevice* Gfx_CreateDevice(Window* window, const GfxConfig& cfg) { return nullptr; }
inline void Gfx_Release(GfxDevice* dev) {}
inline void Gfx_BeginFrame() {}
inline void Gfx_EndFrame() {}
inline GfxProgressId Gfx_Present() { return {}; }
inline void Gfx_SetPresentInterval(u32 interval) {}
inline GfxProgressId Gfx_Submit() { return {}; }
inline GfxProgressId Gfx_GetPendingProgressId() { return {}; }
inline GfxProgressStatus Gfx_QueryProgress(GfxProgressId, GfxProgressFlags) { return GfxProgressStatus::Complete; }
inline void Gfx_ResolveTimestamps() {}
inline const GfxCapability& Gfx_GetCapability() { static const GfxCapability cap; return cap; }
inline const GfxStats& Gfx_Stats() { static const GfxStats stats; return stats; }
inline void Gfx_ResetStats() {}
inline GfxOwn<GfxVertexFormat> Gfx_CreateVertexFormat(const GfxVertexFormatDesc& fmt) { return {}; }
inline GfxOwn<GfxVertexShader> Gfx_CreateVertexShader(const GfxShaderSource& code) { return {}; }
inline GfxOwn<GfxPixelShader> Gfx_CreatePixelShader(const GfxShaderSource& code) { return {}; }
inline GfxOwn<GfxGeometryShader> Gfx_CreateGeometryShader(const GfxShaderSource& code) { return {}; }
inline GfxOwn<GfxComputeShader> Gfx_CreateComputeShader(const GfxShaderSource& code) { return {}; }
inline GfxOwn<GfxTechnique> Gfx_CreateTechnique(const GfxTechniqueDesc& desc) { return {}; }
inline GfxOwn<GfxTexture> Gfx_CreateTexture(const GfxTextureDesc& tex, const GfxTextureData* data, u32 count, const void* texels) { return {}; }
inline GfxOwn<GfxBlendState> Gfx_CreateBlendState(const GfxBlendStateDesc& desc) { return {}; }
inline GfxOwn<GfxSampler> Gfx_CreateSamplerState(const GfxSamplerDesc& desc) { return {}; }
inline GfxOwn<GfxDepthStencilState> Gfx_CreateDepthStencilState(const GfxDepthStencilDesc& desc) { return {}; }
inline GfxOwn<GfxRasterizerState> Gfx_CreateRasterizerState(const GfxRasterizerDesc& desc) { return {}; }
inline GfxOwn<GfxBuffer> Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data) { return {}; }
inline void Gfx_Retain(GfxDevice* dev) {}
inline void Gfx_Retain(GfxContext* rc) {}
#define RUSH_GFX_NULL_RETAIN(descType, handleType, memberName) inline void Gfx_Retain(handleType h) {}
RUSH_GFX_RESOURCE_LIST(RUSH_GFX_NULL_RETAIN)
#undef RUSH_GFX_NULL_RETAIN
#define RUSH_GFX_NULL_RELEASE(descType, handleType, memberName) inline void Gfx_Release(handleType h) {}
RUSH_GFX_RESOURCE_LIST(RUSH_GFX_NULL_RELEASE)
#undef RUSH_GFX_NULL_RELEASE
inline const GfxTextureDesc& Gfx_GetTextureDesc(GfxTextureArg h) { static const GfxTextureDesc desc; return desc; }
inline GfxMappedBuffer Gfx_MapBuffer(GfxBufferArg h, u32 offset, u32 size) { return {}; }
inline void Gfx_UnmapBuffer(GfxMappedBuffer& lock) {}
inline void Gfx_UpdateBuffer(GfxContext* rc, GfxBufferArg h, const void* data, u32 size) {}
inline void* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferArg h, u32 size) { return {}; }
inline void Gfx_EndUpdateBuffer(GfxContext* rc, GfxBufferArg h) {}
inline GfxContext* Gfx_AcquireContext() { return {}; }
inline void Gfx_Release(GfxContext* rc) {}
inline void Gfx_BeginPass(GfxContext* rc, const GfxPassDesc& desc) {}
inline void Gfx_EndPass(GfxContext* rc) {}
inline void Gfx_Clear(GfxContext* rc, ColorRGBA8 color, GfxClearFlags clearFlags, float depth, u32 stencil) {}
inline void Gfx_SetViewport(GfxContext* rc, const GfxViewport& _viewport) {}
inline void Gfx_SetScissorRect(GfxContext* rc, const GfxRect& rect) {}
inline void Gfx_SetTechnique(GfxContext* rc, GfxTechniqueArg h) {}
inline void Gfx_SetPrimitive(GfxContext* rc, GfxPrimitive type) {}
inline void Gfx_SetIndexStream(GfxContext* rc, u32 offset, GfxFormat format, GfxBufferArg h) {}
inline void Gfx_SetVertexStream(GfxContext* rc, u32 idx, u32 offset, u32 stride, GfxBufferArg h) {}
inline void Gfx_SetTexture(GfxContext* rc, u32 idx, GfxTextureArg h) {}
inline void Gfx_SetSampler(GfxContext* rc, u32 idx, GfxSamplerArg h) {}
inline void Gfx_SetStorageImage(GfxContext* rc, u32 idx, GfxTextureArg h) {}
inline void Gfx_SetStorageBuffer(GfxContext* rc, u32 idx, GfxBufferArg h) {}
inline void Gfx_SetBlendState(GfxContext* rc, GfxBlendStateArg nextState) {}
inline void Gfx_SetDepthStencilState(GfxContext* rc, GfxDepthStencilStateArg nextState) {}
inline void Gfx_SetRasterizerState(GfxContext* rc, GfxRasterizerStateArg nextState) {}
inline void Gfx_SetConstantBuffer(GfxContext* rc, u32 index, GfxBufferArg h, size_t offset) {}
inline void Gfx_ResolveImage(GfxContext* rc, GfxTextureArg src, GfxTextureArg dst) {}
inline GfxImageCopyInfo Gfx_GetImageCopyInfo(GfxFormat, Tuple3u) { return {}; }
inline GfxImageCopyInfo Gfx_CopyTextureToBuffer(GfxContext*, GfxTextureArg, const GfxImageRegion&, GfxBufferArg, u64) { return {}; }
inline void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ) {}
inline void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ, const void* pushConstants, u32 pushConstantsSize) {}
inline void Gfx_Draw(GfxContext* rc, u32 firstVertex, u32 vertexCount) {}
inline void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount) {}
inline void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount, const void* pushConstants, u32 pushConstantsSize) {}
inline void Gfx_DrawIndexedInstanced(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount, u32 instanceCount, u32 instanceOffset) {}
inline void Gfx_DrawIndexedIndirect(GfxContext* rc, GfxBufferArg argsBuffer, size_t argsBufferOffset, u32 drawCount) {}
inline void Gfx_DispatchIndirect(GfxContext* rc, GfxBufferArg argsBuffer, size_t argsBufferOffset, const void* pushConstants, u32 pushConstantsSize) {}
inline void Gfx_PushMarker(GfxContext* rc, const char* marker) {}
inline void Gfx_PopMarker(GfxContext* rc) {}
inline void Gfx_BeginTimer(GfxContext* rc, u32 timestampId) {}
inline void Gfx_EndTimer(GfxContext* rc, u32 timestampId) {}
inline void Gfx_RequestScreenshot(GfxScreenshotCallback callback, void* userData) {};
inline GfxOwn<GfxDescriptorSet> Gfx_CreateDescriptorSet(const GfxDescriptorSetDesc& desc) { return {}; }
inline void Gfx_SetDescriptors(GfxContext* rc, u32 index, GfxDescriptorSetArg h) {}
inline void Gfx_UpdateDescriptorSet(GfxDescriptorSetArg d,
	const GfxBuffer* constantBuffers = nullptr,
	const GfxSampler* samplers = nullptr,
	const GfxTexture* textures = nullptr,
	const GfxTexture* storageImages = nullptr,
	const GfxBuffer* storageBuffers = nullptr,
	const GfxAccelerationStructure* accelStructures = nullptr) {}
#endif // RUSH_RENDER_API == RUSH_RENDER_API_NULL

}
