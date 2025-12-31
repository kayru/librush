#pragma once

#include "GfxDevice.h"

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL

#include "UtilResourcePool.h"
#include "Window.h"

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

struct DescriptorSetMTL : GfxRefCount
{
	u32                      uniqueId = 0;
	GfxDescriptorSetDesc     desc;
	DynamicArray<u64>        constantBufferOffsets;
	DynamicArray<GfxBuffer>  constantBuffers;
	DynamicArray<GfxTexture> textures;
	DynamicArray<GfxSampler> samplers;
	DynamicArray<GfxTexture> storageImages;
	DynamicArray<GfxBuffer>  storageBuffers;

	id<MTLArgumentEncoder> encoder = nil; // #todo: pool argument encoders by descriptor set desc
	id<MTLBuffer> argBuffer = nil;
	u64 argBufferOffset = 0; // #todo: pool and sub-allocate arg buffers
	u64 argBufferSize = 0;

	void destroy();
};

struct TechniqueMTL : GfxRefCount
{
	u32 uniqueId = 0;

	GfxTechniqueDesc desc;

	GfxRef<GfxVertexFormat> vf;
	GfxRef<GfxVertexShader> vs;
	GfxRef<GfxPixelShader> ps;

	id<MTLComputePipelineState> computePipeline = nil;
	
	u32 constantBufferOffset = 0;
	u32 samplerOffset = 0;
	u32 sampledImageOffset = 0;
	u32 storageImageOffset = 0;
	u32 storageBufferOffset = 0;
	u32 descriptorSetCount = 0;

	Tuple3<u16> workGroupSize = {};

	DescriptorSetMTL defaultDescriptorSet;
	
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

	void beginFrame();
	void createDefaultDepthBuffer(u32 width, u32 height);

	id<MTLDevice> m_metalDevice = nil;
	id<MTLCommandQueue> m_commandQueue = nil;
	CAMetalLayer* m_metalLayer = nil;

	struct Resources
	{
		ResourcePool<ShaderMTL, UntypedResourceHandle> shaders;
		ResourcePool<VertexFormatMTL, GfxVertexFormat> vertexFormats;
		ResourcePool<BufferMTL, GfxBuffer> buffers;
		ResourcePool<TechniqueMTL, GfxTechnique> techniques;
		ResourcePool<DepthStencilStateMTL, GfxDepthStencilState> depthStencilStates;
		ResourcePool<RasterizerStateMTL, GfxRasterizerState> rasterizerStates;
		ResourcePool<TextureMTL, GfxTexture> textures;
		ResourcePool<BlendStateMTL, GfxBlendState> blendStates;
		ResourcePool<SamplerMTL, GfxSampler> samplers;
		ResourcePool<DescriptorSetMTL, GfxDescriptorSet> descriptorSets;
	};

	Resources m_resources;

	template <typename HandleType>
	static GfxOwn<HandleType> makeOwn(HandleType h) { return GfxOwn<HandleType>(h); }

	Window*             m_window = nullptr;
	WindowEventListener m_resizeEvents;

	GfxCapability m_caps;
	GfxStats m_stats;

	u32 m_uniqueResourceCounter = 1;

	id<CAMetalDrawable> m_drawable = nil;
	id<MTLTexture> m_backBufferTexture = nil;
	MTLPixelFormat m_backBufferPixelFormat = MTLPixelFormatInvalid;
	id<MTLCommandBuffer> m_commandBuffer = nil;
	struct PendingScreenshot
	{
		id<MTLBuffer> buffer = nil;
		GfxScreenshotCallback callback = nullptr;
		void* userData = nullptr;
		u32 width = 0;
		u32 height = 0;
	};

	PendingScreenshot m_pendingScreenshot;

	GfxRef<GfxTexture> m_defaultDepthBuffer;
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
		MaxDescriptorSets = 4,
		MaxAccelerationStructures = 0, // TODO: implement basic ray tracing
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
		DirtyStateFlag_DescriptorSet = 1 << 12,

		DirtyStateFlag_Descriptors = DirtyStateFlag_ConstantBuffer | DirtyStateFlag_Texture | DirtyStateFlag_Sampler | DirtyStateFlag_StorageImage | DirtyStateFlag_StorageBuffer,

		DirtyStateFlag_Pipeline = DirtyStateFlag_Technique | DirtyStateFlag_PrimitiveType | DirtyStateFlag_BlendState | DirtyStateFlag_DepthStencilState | DirtyStateFlag_RasterizerState,
	};

	u32 m_dirtyState = 0xFFFFFFFF;

	id<MTLRenderCommandEncoder> m_commandEncoder = nil;
	id<MTLComputeCommandEncoder> m_computeCommandEncoder = nil;
	id<MTLDepthStencilState> m_depthStencilState = nil;

	GfxRef<GfxTechnique> m_pendingTechnique;
	GfxRef<GfxBlendState> m_pendingBlendState;
	GfxRef<GfxRasterizerState> m_pendingRasterizerState;
	GfxRef<GfxDepthStencilState> m_pendingDepthStencilState;
	GfxRef<GfxBuffer> m_constantBuffers[MaxConstantBuffers];
	u64 m_constantBufferOffsets[MaxConstantBuffers] = {};
	GfxRef<GfxSampler> m_samplers[MaxSamplers];
	GfxRef<GfxTexture> m_sampledImages[MaxSampledImages];
	GfxRef<GfxTexture> m_storageImages[MaxStorageImages];
	GfxRef<GfxBuffer> m_storageBuffers[MaxStorageBuffers];
	GfxRef<GfxDescriptorSet> m_descriptorSets[MaxDescriptorSets];
	GfxPassDesc m_passDesc;

	MTLIndexType m_indexType = MTLIndexTypeUInt32;
	u32 m_indexStride = 4;
	u32 m_indexBufferOffset = 0;
	id<MTLBuffer> m_indexBuffer = nil;

	MTLPrimitiveType m_primitiveType = MTLPrimitiveTypeTriangle;
	MTLPrimitiveTopologyClass m_primitiveTopology = MTLPrimitiveTopologyClassUnspecified;

};

}

#endif
