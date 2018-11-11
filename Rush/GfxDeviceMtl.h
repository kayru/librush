#pragma once

#include "GfxDevice.h"
#include "GfxRef.h"
#include "UtilResourcePool.h"

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

namespace Rush
{

struct ShaderMTL : GfxRefCount
{
	u32 uniqueId = 0;

	id<MTLLibrary> library = nil;
	id<MTLFunction> function = nil;

	static ShaderMTL create(const GfxShaderSource& code);
	void destroy();
};

struct VertexFormatMTL : GfxRefCount
{
	u32 uniqueId = 0;
	GfxVertexFormatDesc desc;
	MTLVertexDescriptor* native = nil;
	void destroy();
};

struct BufferMTL : GfxRefCount
{
	u32 uniqueId = 0;
	id<MTLBuffer> native = nil;
	u32 stride = 0;
	MTLIndexType indexType = MTLIndexTypeUInt32;

	void destroy();
};

struct TechniqueMTL : GfxRefCount
{
	u32 uniqueId = 0;

	GfxVertexFormatRef vf;
	GfxVertexShaderRef vs;
	GfxPixelShaderRef ps;

	id<MTLComputePipelineState> computePipeline = nil;
	
	u32 constantBufferOffset = 0;
	u32 samplerOffset = 0;
	u32 sampledImageOffset = 0;
	u32 storageImageOffset = 0;
	u32 storageBufferOffset = 0;
	
	void destroy();
};

struct DepthStencilStateMTL : GfxRefCount
{
	u32 uniqueId = 0;
	id<MTLDepthStencilState> native = nil;
	void destroy();
};

struct RasterizerStateMTL : GfxRefCount
{
	u32 uniqueId = 0;
	GfxRasterizerDesc desc;
	void destroy();
};

struct TextureMTL : GfxRefCount
{
	u32 uniqueId = 0;
	id<MTLTexture> native = nil;
	GfxTextureDesc desc;
	static TextureMTL create(const GfxTextureDesc& desc, const GfxTextureData* data, u32 count, const void* pixels);
	void destroy();
};

struct BlendStateMTL : GfxRefCount
{
	u32 uniqueId = 0;
	GfxBlendStateDesc desc;
	void destroy();
};

struct SamplerMTL : GfxRefCount
{
	u32 uniqueId = 0;
	id<MTLSamplerState> native = nil;
	void destroy();
};

class GfxDevice : public GfxRefCount
{
public:

	GfxDevice(Window* window, const GfxConfig& cfg);
	~GfxDevice();

	u32 generateId();

	id<MTLDevice> m_metalDevice = nil;
	id<MTLCommandQueue> m_commandQueue = nil;
	CAMetalLayer* m_metalLayer = nil;

	ResourcePool<ShaderMTL, GfxVertexShader> m_vertexShaders;
	ResourcePool<ShaderMTL, GfxPixelShader> m_pixelShaders;
	ResourcePool<ShaderMTL, GfxComputeShader> m_computeShaders;
	ResourcePool<VertexFormatMTL, GfxVertexFormat> m_vertexFormats;
	ResourcePool<BufferMTL, GfxBuffer> m_buffers;
	ResourcePool<TechniqueMTL, GfxTechnique> m_techniques;
	ResourcePool<DepthStencilStateMTL, GfxDepthStencilState> m_depthStencilStates;
	ResourcePool<RasterizerStateMTL, GfxRasterizerState> m_rasterizerStates;
	ResourcePool<TextureMTL, GfxTexture> m_textures;
	ResourcePool<BlendStateMTL, GfxBlendState> m_blendStates;
	ResourcePool<SamplerMTL, GfxSampler> m_samplers;

	GfxCapability m_caps;
	GfxStats m_stats;

	u32 m_uniqueResourceCounter = 1;

	id<CAMetalDrawable> m_drawable = nil;
	id<MTLTexture> m_backBufferTexture = nil;
	MTLPixelFormat m_backBufferPixelFormat = MTLPixelFormatInvalid;
	id<MTLCommandBuffer> m_commandBuffer = nil;

	GfxTextureRef m_defaultDepthBuffer;
};

class GfxContext : public GfxRefCount
{
public:

	enum
	{
		MaxSampledImages = 16,
		MaxStorageImages = 8,
		MaxVertexStreams = 8,
		MaxConstantBuffers = 4,
		MaxStorageBuffers = 4,
		MaxSamplers = 4,
	};

	GfxContext();
	~GfxContext();

	void applyState();

	enum DirtyStateFlag
	{
		DirtyStateFlag_Technique = 1 << 0,
		DirtyStateFlag_PrimitiveType = 1 << 1,
		DirtyStateFlag_VertexBuffer = 1 << 2,
		DirtyStateFlag_IndexBuffer = 1 << 3,
		DirtyStateFlag_Texture = 1 << 4,
		DirtyStateFlag_BlendState = 1 << 5,
		DirtyStateFlag_DepthStencilState = 1 << 6,
		DirtyStateFlag_RasterizerState = 1 << 7,
		DirtyStateFlag_Sampler = 1 << 8,
		DirtyStateFlag_ConstantBuffer = 1 << 9,
		DirtyStateFlag_StorageImage = 1 << 10,
		DirtyStateFlag_StorageBuffer = 1 << 11,

		DirtyStateFlag_Descriptors = DirtyStateFlag_ConstantBuffer | DirtyStateFlag_Texture | DirtyStateFlag_Sampler | DirtyStateFlag_StorageImage | DirtyStateFlag_StorageBuffer,

		DirtyStateFlag_Pipeline = DirtyStateFlag_Technique | DirtyStateFlag_PrimitiveType | DirtyStateFlag_BlendState | DirtyStateFlag_DepthStencilState | DirtyStateFlag_RasterizerState,
	};

	u32 m_dirtyState = 0xFFFFFFFF;

	id<MTLRenderCommandEncoder> m_commandEncoder = nil;
	id<MTLComputeCommandEncoder> m_computeCommandEncoder = nil;
	id<MTLDepthStencilState> m_depthStencilState = nil;

	GfxTechniqueRef m_pendingTechnique;
	GfxBlendStateRef m_pendingBlendState;
	GfxRasterizerStateRef m_pendingRasterizerState;
	GfxDepthStencilStateRef m_pendingDepthStencilState;
	GfxBufferRef m_constantBuffers[MaxConstantBuffers];
	size_t m_constantBufferOffsets[MaxConstantBuffers] = {};
	GfxSamplerRef m_samplers[u32(GfxStage::count)][MaxSamplers];
	GfxTextureRef m_sampledImages[u32(GfxStage::count)][MaxSampledImages];
	GfxTextureRef m_storageImages[MaxStorageImages];
	GfxBufferRef m_storageBuffers[MaxStorageBuffers];
	GfxPassDesc m_passDesc;

	MTLIndexType m_indexType = MTLIndexTypeUInt32;
	u32 m_indexStride = 4;
	id<MTLBuffer> m_indexBuffer = nil;

	MTLPrimitiveType m_primitiveType = MTLPrimitiveTypeTriangle;
	MTLPrimitiveTopologyClass m_primitiveTopology = MTLPrimitiveTopologyClassUnspecified;

};

}

#endif
