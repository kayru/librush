#include "GfxDeviceMtl.h"
#include "UtilLog.h"
#include "Window.h"
#include "Platform.h"
#include "UtilFile.h"
#include "UtilImage.h"

#include <cstring>

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL

#include "WindowMac.h"

namespace Rush
{

static DescriptorSetMTL createDescriptorSet(const GfxDescriptorSetDesc& desc);
static void updateDescriptorSet(DescriptorSetMTL& ds,
	const GfxBuffer* constantBuffers,
	const u64* constantBufferOffsets,
	const GfxSampler* samplers,
	const GfxTexture* textures,
	const GfxTexture* storageImages,
	const GfxBuffer* storageBuffers,
	const GfxAccelerationStructure* accelStructures);

static GfxDevice* g_device = nullptr;
static GfxContext* g_context = nullptr;
static id<MTLDevice> g_metalDevice = nil;
static constexpr u32 kPushConstantMaxSize = 4096;

static void setPushConstants(GfxContext* rc, const void* data, u32 size, GfxStageFlags stages, u32 bufferIndex)
{
	RUSH_ASSERT(data);
	RUSH_ASSERT(size > 0);
	RUSH_ASSERT(size <= kPushConstantMaxSize);

	if (!!(stages & GfxStageFlags::Vertex))
	{
		RUSH_ASSERT(rc->m_commandEncoder);
		[rc->m_commandEncoder setVertexBytes:data length:size atIndex:bufferIndex];
	}

	if (!!(stages & GfxStageFlags::Pixel))
	{
		RUSH_ASSERT(rc->m_commandEncoder);
		[rc->m_commandEncoder setFragmentBytes:data length:size atIndex:bufferIndex];
	}

	if (!!(stages & GfxStageFlags::Compute))
	{
		RUSH_ASSERT(rc->m_computeCommandEncoder);
		[rc->m_computeCommandEncoder setBytes:data length:size atIndex:bufferIndex];
	}
}

template <typename HandleType, typename ObjectType, typename PoolHandleType, typename ObjectTypeDeduced>
HandleType retainResourceT(
	ResourcePool<ObjectType, PoolHandleType>& pool,
	ObjectTypeDeduced&& object)
{
	RUSH_ASSERT(object.uniqueId != 0);
	auto handle = pool.push(std::forward<ObjectType>(object));
	Gfx_Retain(HandleType(handle));
	Gfx_Retain(g_device);
	return HandleType(handle);
}

template <typename ObjectType, typename HandleType, typename ObjectTypeDeduced>
HandleType retainResource(
	ResourcePool<ObjectType, HandleType>& pool,
	ObjectTypeDeduced&& object)
{
	return retainResourceT<HandleType>(pool, std::forward<ObjectType>(object));
}

template <typename ObjectType, typename PoolHandleType, typename HandleType>
void releaseResource(
	ResourcePool<ObjectType, PoolHandleType>& pool,
	HandleType handle)
{
	if (!handle.valid())
		return;

	auto& t = pool[handle];
	if (t.removeReference() > 1)
		return;

	t.destroy();

	pool.remove(handle);

	Gfx_Release(g_device);
}

static MTLCompareFunction convertCompareFunc(GfxCompareFunc compareFunc)
{
	switch (compareFunc)
	{
		default:
			Log::error("Unexpected compare function");
		case GfxCompareFunc::Never:
			return MTLCompareFunctionNever;
		case GfxCompareFunc::Less:
			return MTLCompareFunctionLess;
		case GfxCompareFunc::Equal:
			return MTLCompareFunctionEqual;
		case GfxCompareFunc::LessEqual:
			return MTLCompareFunctionLessEqual;
		case GfxCompareFunc::Greater:
			return MTLCompareFunctionGreater;
		case GfxCompareFunc::NotEqual:
			return MTLCompareFunctionNotEqual;
		case GfxCompareFunc::GreaterEqual:
			return MTLCompareFunctionGreaterEqual;
		case GfxCompareFunc::Always:
			return MTLCompareFunctionAlways;
	}
}

static MTLPrimitiveTopologyClass convertPrimitiveTopology(GfxPrimitive primitiveType)
{
	switch (primitiveType)
	{
	default:
		Log::error("Unexpected primitive type");
	case GfxPrimitive::PointList:
		return MTLPrimitiveTopologyClassPoint;
	case GfxPrimitive::LineList:
		return MTLPrimitiveTopologyClassLine;
	case GfxPrimitive::LineStrip:
		return MTLPrimitiveTopologyClassLine;
	case GfxPrimitive::TriangleList:
		return MTLPrimitiveTopologyClassTriangle;
	case GfxPrimitive::TriangleStrip:
		return MTLPrimitiveTopologyClassTriangle;
	}
}

static MTLPrimitiveType convertPrimitiveType(GfxPrimitive primitiveType)
{
	switch (primitiveType)
	{
	default:
		Log::error("Unexpected primitive type");
	case GfxPrimitive::PointList:
		return MTLPrimitiveTypePoint;
	case GfxPrimitive::LineList:
		return MTLPrimitiveTypeLine;
	case GfxPrimitive::LineStrip:
		return MTLPrimitiveTypeLineStrip;
	case GfxPrimitive::TriangleList:
		return MTLPrimitiveTypeTriangle;
	case GfxPrimitive::TriangleStrip:
		return MTLPrimitiveTypeTriangleStrip;
	}
}

static MTLAttributeFormat convertRayTracingVertexFormat(GfxFormat format)
{
	switch (format)
	{
	default:
		Log::error("Unsupported ray tracing vertex format");
		return MTLAttributeFormatInvalid;
	case GfxFormat_RGB32_Float:
		return MTLAttributeFormatFloat3;
	case GfxFormat_RGBA32_Float:
		return MTLAttributeFormatFloat4;
	}
}

static MTLIndexType convertRayTracingIndexType(GfxFormat format)
{
	switch (format)
	{
	default:
		Log::error("Unsupported ray tracing index format");
		return MTLIndexTypeUInt32;
	case GfxFormat_R32_Uint:
		return MTLIndexTypeUInt32;
	case GfxFormat_R16_Uint:
		return MTLIndexTypeUInt16;
	}
}

GfxDevice::GfxDevice(Window* _window, const GfxConfig& cfg)
{
	auto window = static_cast<WindowMac*>(_window);

	m_window = window;
	m_window->retain();

	m_resizeEvents.mask = WindowEventMask_Resize;
	m_resizeEvents.setOwner(window);

	g_device = this;
	m_refs = 1;

	m_metalDevice = MTLCreateSystemDefaultDevice();
	m_commandQueue = [m_metalDevice newCommandQueue];

	g_metalDevice = m_metalDevice;

	m_metalLayer = window->getMetalLayer();
	m_metalLayer.device = m_metalDevice;
	m_metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;

	// default resources

	m_resources.blendStates[InvalidResourceHandle()].desc = GfxBlendStateDesc::makeOpaque();

	createDefaultDepthBuffer(cfg.backBufferWidth, cfg.backBufferHeight);

	// init caps

	m_caps.shaderTypeMask |= 1 << GfxShaderSourceType_MSL;
	m_caps.shaderTypeMask |= 1 << GfxShaderSourceType_MSL_BIN;
	m_caps.deviceNearDepth = 0.0f;
	m_caps.deviceFarDepth = 1.0f;
	m_caps.compute = true;
	m_caps.debugOutput = false;
	m_caps.debugMarkers = true;
	m_caps.constantBufferAlignment = 256;
	m_caps.pushConstants = true;
	m_caps.instancing = true;
	m_caps.drawIndirect = true;
	if ([m_metalDevice respondsToSelector:@selector(supportsRaytracing)])
	{
		const bool supportsRaytracing = [m_metalDevice supportsRaytracing];
		m_caps.rayTracingPipeline = supportsRaytracing;
		m_caps.rayTracingInline = supportsRaytracing;
	}
	else
	{
		m_caps.rayTracingPipeline = false;
		m_caps.rayTracingInline = false;
	}
	m_caps.rayTracing = m_caps.rayTracingPipeline || m_caps.rayTracingInline;

	m_caps.colorSampleCounts = 1;
	m_caps.depthSampleCounts = 1;
	{
		const u32 sampleCounts[] = {1, 2, 4, 8, 16};
		u32 colorMask = 0;
		u32 depthMask = 0;
		for (u32 samples : sampleCounts)
		{
			if ([m_metalDevice supportsTextureSampleCount:samples])
			{
				colorMask |= samples;
				depthMask |= samples;
			}
		}
		if (colorMask != 0)
		{
			m_caps.colorSampleCounts = colorMask;
		}
		if (depthMask != 0)
		{
			m_caps.depthSampleCounts = depthMask;
		}
	}

	m_caps.apiName = "Metal";
}

GfxDevice::~GfxDevice()
{
	g_metalDevice = nil;

	[m_commandBuffer release];
	[m_commandQueue release];
	[m_metalDevice release];

	m_resizeEvents.setOwner(nullptr);

	m_window->release();
	m_window = nullptr;
}

u32 GfxDevice::generateId()
{
	return m_uniqueResourceCounter++;
}

// device
GfxDevice* Gfx_CreateDevice(Window* window, const GfxConfig& cfg)
{
	RUSH_ASSERT_MSG(g_device == nullptr, "Only a single graphics device can be created per process.");

	GfxDevice* dev = new GfxDevice(window, cfg);
	RUSH_ASSERT(dev == g_device);

	return dev;
}

void Gfx_Release(GfxDevice* dev)
{
	if (dev->removeReference() > 1)
		return;

	delete dev;

	g_device = nullptr;
}

void GfxDevice::createDefaultDepthBuffer(u32 width, u32 height)
{
	GfxTextureDesc defaultDepthBufferDesc;
	defaultDepthBufferDesc.width = width;
	defaultDepthBufferDesc.height = height;
	defaultDepthBufferDesc.depth = 1;
	defaultDepthBufferDesc.mips = 1;
	defaultDepthBufferDesc.format = GfxFormat_D32_Float;
	defaultDepthBufferDesc.usage = GfxUsageFlags::DepthStencil;
	m_defaultDepthBuffer.retain(Gfx_CreateTexture(defaultDepthBufferDesc));
}

void GfxDevice::beginFrame()
{
	if (!m_resizeEvents.empty())
	{
		createDefaultDepthBuffer(
			m_window->getFramebufferWidth(),
			m_window->getFramebufferHeight());

		CGSize nextDrawableSize = { 
			(CGFloat) m_window->getFramebufferWidth(), 
			(CGFloat) m_window->getFramebufferHeight() };
		m_metalLayer.drawableSize = nextDrawableSize;

		m_resizeEvents.clear();
	}

	// FIXME: handle nextDrawable returning nil (resize/minimize) before using drawable/texture.
	m_drawable = [m_metalLayer nextDrawable];
	[m_drawable retain];

	m_backBufferTexture = [m_drawable texture];
	[m_backBufferTexture retain];

	m_backBufferPixelFormat = [m_metalLayer pixelFormat];

	m_commandBuffer = [m_commandQueue commandBuffer];
	[m_commandBuffer retain];
}

void Gfx_BeginFrame()
{
	g_device->beginFrame();
}

void Gfx_EndFrame()
{
	if (g_device->m_pendingScreenshot.callback && g_device->m_backBufferTexture)
	{
		if (g_context && g_context->m_computeCommandEncoder)
		{
			[g_context->m_computeCommandEncoder endEncoding];
			[g_context->m_computeCommandEncoder release];
			g_context->m_computeCommandEncoder = nil;
		}

		if (g_device->m_pendingScreenshot.buffer)
		{
			[g_device->m_pendingScreenshot.buffer release];
			g_device->m_pendingScreenshot.buffer = nil;
		}

		const u32 width = (u32)[g_device->m_backBufferTexture width];
		const u32 height = (u32)[g_device->m_backBufferTexture height];
		const u32 bytesPerRow = width * 4;
		const u32 alignedBytesPerRow = (bytesPerRow + 0xFF) & ~0xFFu;
		const u32 bufferSize = alignedBytesPerRow * height;

		g_device->m_pendingScreenshot.width = width;
		g_device->m_pendingScreenshot.height = height;
		g_device->m_pendingScreenshot.buffer =
		    [g_device->m_metalDevice newBufferWithLength:bufferSize options:MTLResourceStorageModeShared];

		id<MTLBlitCommandEncoder> blit = [g_device->m_commandBuffer blitCommandEncoder];
		[blit copyFromTexture:g_device->m_backBufferTexture
		          sourceSlice:0
		          sourceLevel:0
		         sourceOrigin:MTLOriginMake(0, 0, 0)
		           sourceSize:MTLSizeMake(width, height, 1)
		             toBuffer:g_device->m_pendingScreenshot.buffer
		    destinationOffset:0
	   destinationBytesPerRow:alignedBytesPerRow
	 destinationBytesPerImage:bufferSize];
		[blit endEncoding];
	}
}

void Gfx_Present()
{
	RUSH_ASSERT(g_device->m_commandBuffer);
	RUSH_ASSERT(g_device->m_drawable);

	// TODO: deal with multiple contexts
	if (g_context->m_computeCommandEncoder)
	{
		[g_context->m_computeCommandEncoder endEncoding];
		[g_context->m_computeCommandEncoder release];
		g_context->m_computeCommandEncoder = nil;
	}

	[g_device->m_commandBuffer presentDrawable:g_device->m_drawable];
	[g_device->m_commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
		if (buffer.GPUEndTime > buffer.GPUStartTime)
		{
			g_device->m_stats.lastFrameGpuTime = buffer.GPUEndTime - buffer.GPUStartTime;
		}
	}];
	[g_device->m_commandBuffer commit];
	if (g_device->m_pendingScreenshot.callback)
	{
		[g_device->m_commandBuffer waitUntilCompleted];
		if (g_device->m_pendingScreenshot.buffer)
		{
			const u8* src = reinterpret_cast<const u8*>([g_device->m_pendingScreenshot.buffer contents]);
			const u32 width = g_device->m_pendingScreenshot.width;
			const u32 height = g_device->m_pendingScreenshot.height;
			if (src && width && height)
			{
				const u32 bytesPerRow = width * 4;
				const u32 alignedBytesPerRow = (bytesPerRow + 0xFF) & ~0xFFu;
				const size_t pixelCount = static_cast<size_t>(width) * height;
				DynamicArray<ColorRGBA8> pixels(pixelCount);
				ImageView imageView;
				imageView.data = src;
				imageView.width = width;
				imageView.height = height;
				imageView.bytesPerRow = alignedBytesPerRow;
				imageView.format = GfxFormat_BGRA8_Unorm;
				convertToRGBA8(imageView, ArrayView<ColorRGBA8>(pixels));

				g_device->m_pendingScreenshot.callback(
				    pixels.data(),
				    Tuple2u{width, height},
				    g_device->m_pendingScreenshot.userData);
			}
		}

		if (g_device->m_pendingScreenshot.buffer)
		{
			[g_device->m_pendingScreenshot.buffer release];
			g_device->m_pendingScreenshot.buffer = nil;
		}
		g_device->m_pendingScreenshot.callback = nullptr;
		g_device->m_pendingScreenshot.userData = nullptr;
		g_device->m_pendingScreenshot.width = 0;
		g_device->m_pendingScreenshot.height = 0;
	}
	[g_device->m_commandBuffer release];
	g_device->m_commandBuffer = nil;
	
	[g_device->m_backBufferTexture release];
	g_device->m_backBufferTexture = nil;
	
	[g_device->m_drawable release];
	g_device->m_drawable = nil;
}

void Gfx_SetPresentInterval(u32 interval)
{
	static bool warningReported = false;
	if (interval != 1 && !warningReported)
	{
		Log::warning("Present interval != 1 is not implemented");
		warningReported = true;
	}
}

void Gfx_RequestScreenshot(GfxScreenshotCallback callback, void* userData)
{
	g_device->m_pendingScreenshot.callback = callback;
	g_device->m_pendingScreenshot.userData = userData;
}

void Gfx_Finish()
{
	if (!g_device || !g_device->m_commandBuffer)
	{
		return;
	}

	// Ensure any active encoders are closed before committing.
	if (g_context)
	{
		if (g_context->m_commandEncoder)
		{
			[g_context->m_commandEncoder endEncoding];
			[g_context->m_commandEncoder release];
			g_context->m_commandEncoder = nil;
		}
		if (g_context->m_computeCommandEncoder)
		{
			[g_context->m_computeCommandEncoder endEncoding];
			[g_context->m_computeCommandEncoder release];
			g_context->m_computeCommandEncoder = nil;
		}
	}

	[g_device->m_commandBuffer commit];
	[g_device->m_commandBuffer waitUntilCompleted];
	[g_device->m_commandBuffer release];
	g_device->m_commandBuffer = [g_device->m_commandQueue commandBuffer];
	[g_device->m_commandBuffer retain];
}

const GfxCapability& Gfx_GetCapability()
{
	RUSH_ASSERT(g_device);
	return g_device->m_caps;
}

const GfxStats& Gfx_Stats()
{
	return g_device->m_stats;
}

void Gfx_ResetStats()
{
	g_device->m_stats = GfxStats();
}

// vertex format

void VertexFormatMTL::destroy()
{
	[native release];
}

static MTLVertexFormat convertVertexFormat(const GfxVertexFormatDesc::Element& vertexElement)
{
	switch (vertexElement.type)
	{
	case GfxVertexFormatDesc::DataType::Float1:
		return MTLVertexFormatFloat;
	case GfxVertexFormatDesc::DataType::Float2:
		return MTLVertexFormatFloat2;
	case GfxVertexFormatDesc::DataType::Float3:
		return MTLVertexFormatFloat3;
	case GfxVertexFormatDesc::DataType::Float4:
		return MTLVertexFormatFloat4;
	case GfxVertexFormatDesc::DataType::Color:
		return MTLVertexFormatUChar4Normalized;
	case GfxVertexFormatDesc::DataType::Short2N:
		return MTLVertexFormatShort2Normalized;
	case GfxVertexFormatDesc::DataType::UInt:
		return MTLVertexFormatUInt;
	default:
		Log::error("Unsupported vertex element format type");
		return MTLVertexFormatInvalid;
	}
}

GfxOwn<GfxVertexFormat> Gfx_CreateVertexFormat(const GfxVertexFormatDesc& desc)
{
	VertexFormatMTL format;
	format.uniqueId = g_device->generateId();
	format.desc = desc;

	format.native = [MTLVertexDescriptor new];

	u32 usedStreamMask = 0;
	for (u32 i = 0; i < (u32)desc.elementCount(); ++i)
	{
		const auto& element = desc.element(i);
		format.native.attributes[i].format = convertVertexFormat(element);
		format.native.attributes[i].bufferIndex = GfxContext::MaxConstantBuffers + element.stream;
		format.native.attributes[i].offset = element.offset;
		usedStreamMask |= 1 << element.stream;
	}

	for (u32 streamIndex = 0; usedStreamMask && streamIndex < 32; ++streamIndex)
	{
		if (usedStreamMask & 1)
		{
			format.native.layouts[GfxContext::MaxConstantBuffers + streamIndex].stride = desc.streamStride(streamIndex);
			format.native.layouts[GfxContext::MaxConstantBuffers + streamIndex].stepFunction = MTLVertexStepFunctionPerVertex;
		}

		usedStreamMask = usedStreamMask >> 1;
	}

	return GfxDevice::makeOwn(retainResource(g_device->m_resources.vertexFormats, format));
}

void Gfx_Release(GfxVertexFormat h)
{
	releaseResource(g_device->m_resources.vertexFormats, h);
}

// shader

ShaderMTL ShaderMTL::create(const GfxShaderSource& code)
{
	RUSH_ASSERT(code.type == GfxShaderSourceType_MSL || code.type == GfxShaderSourceType_MSL_BIN);
	const char* entryName = code.entry;
	if (entryName == nullptr)
	{
		entryName = "main";
	}

	ShaderMTL result;
	result.uniqueId = g_device->generateId();

	NSError* error = nullptr;
	if (code.type == GfxShaderSourceType_MSL)
	{
		const char* sourceText = code.data();
		RUSH_ASSERT(sourceText);
		result.library = [g_metalDevice newLibraryWithSource:@(sourceText) options:nil error:&error];
	}
	else
	{
		RUSH_ASSERT(code.size() > 0);
		dispatch_data_t libraryData = dispatch_data_create(code.data(), code.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
		result.library = [g_metalDevice newLibraryWithData:libraryData error:&error];
#if !OS_OBJECT_USE_OBJC
		dispatch_release(libraryData);
#endif
	}
	if(error)
	{
		if (error.code == MTLLibraryErrorCompileWarning)
		{
			Log::warning("Shader compile warning: %s", [error.localizedDescription cStringUsingEncoding:NSASCIIStringEncoding]);
		}
		else
		{
			Log::error("Shader compile error: %s", [error.localizedDescription cStringUsingEncoding:NSASCIIStringEncoding]);
		}
	}

	result.function = [result.library newFunctionWithName:@(entryName)];

	if(!result.function)
	{
		Log::error("Can't create shader entry point '%s'", entryName);
	}

	return result;
}

void ShaderMTL::destroy()
{
	[function release];
	[library release];
}

// Compute shader

GfxOwn<GfxComputeShader> Gfx_CreateComputeShader(const GfxShaderSource& code)
{
	return GfxDevice::makeOwn(retainResourceT<GfxComputeShader>(g_device->m_resources.shaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxComputeShader h)
{
	releaseResource(g_device->m_resources.shaders, h);
}

// vertex shader

GfxOwn<GfxVertexShader> Gfx_CreateVertexShader(const GfxShaderSource& code)
{
	return GfxDevice::makeOwn(retainResourceT<GfxVertexShader>(g_device->m_resources.shaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxVertexShader h)
{
	releaseResource(g_device->m_resources.shaders, h);
}

// pixel shader
GfxOwn<GfxPixelShader> Gfx_CreatePixelShader(const GfxShaderSource& code)
{
	return GfxDevice::makeOwn(retainResourceT<GfxPixelShader>(g_device->m_resources.shaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxPixelShader h)
{
	releaseResource(g_device->m_resources.shaders, h);
}

// technique

void TechniqueMTL::destroy()
{
	defaultDescriptorSet.destroy();
	vs.reset();
	ps.reset();
	[computePipeline release];
}

void RayTracingPipelineMTL::destroy()
{
	defaultDescriptorSet.destroy();
	[rayGenPipeline release];
	rayGenPipeline = nil;
	rayGen.destroy();
	miss.destroy();
	closestHit.destroy();
	anyHit.destroy();
}

GfxOwn<GfxTechnique> Gfx_CreateTechnique(const GfxTechniqueDesc& desc)
{
	RUSH_ASSERT(desc.vs.valid() || desc.cs.valid());

	TechniqueMTL result;

	result.uniqueId = g_device->generateId();
	result.desc = desc;

	if (desc.cs.valid())
	{
		id <MTLFunction> computeShader = g_device->m_resources.shaders[desc.cs].function;
		NSError* error = nil;
		id<MTLComputePipelineState> pipeline = [g_metalDevice newComputePipelineStateWithFunction:computeShader error:&error];
		RUSH_ASSERT(pipeline);
		[error release];

		result.computePipeline = pipeline;
		result.workGroupSize = desc.workGroupSize;
	}
	else
	{
		result.vf.retain(desc.vf);
		result.vs.retain(desc.vs);
		result.ps.retain(desc.ps);
	}

	const auto& dsetDesc = desc.bindings.descriptorSets[0];

	u32 offset = 0;

	result.constantBufferOffset = 0;
	offset += dsetDesc.constantBuffers;

	result.samplerOffset = offset;
	offset += dsetDesc.samplers;

	result.sampledImageOffset = offset;
	offset += dsetDesc.textures;

	result.storageImageOffset = offset;
	offset += dsetDesc.rwImages;

	result.storageBufferOffset = offset;
	offset += dsetDesc.rwBuffers;

	for(u32 i=0; i<GfxShaderBindingDesc::MaxDescriptorSets; ++i)
	{
		if(!desc.bindings.descriptorSets[i].isEmpty())
		{
			result.descriptorSetCount = i+1;
		}
	}

	result.defaultDescriptorSet = createDescriptorSet(dsetDesc);

	return GfxDevice::makeOwn(retainResource(g_device->m_resources.techniques, result));
}

GfxOwn<GfxRayTracingPipeline> Gfx_CreateRayTracingPipeline(const GfxRayTracingPipelineDesc& desc)
{
	if (desc.rayGen.empty())
	{
		Log::error("Ray generation shader must be provided");
		return InvalidResourceHandle();
	}

	RayTracingPipelineMTL result;
	result.uniqueId = g_device->generateId();
	result.bindings = desc.bindings;
	result.maxRecursionDepth = desc.maxRecursionDepth;

	result.rayGen = ShaderMTL::create(desc.rayGen);
	if (!result.rayGen.function)
	{
		result.rayGen.destroy();
		return InvalidResourceHandle();
	}

	if (!desc.miss.empty())
	{
		result.miss = ShaderMTL::create(desc.miss);
	}

	if (!desc.closestHit.empty())
	{
		result.closestHit = ShaderMTL::create(desc.closestHit);
	}

	if (!desc.anyHit.empty())
	{
		result.anyHit = ShaderMTL::create(desc.anyHit);
	}

	MTLComputePipelineDescriptor* pipelineDesc = [MTLComputePipelineDescriptor new];
	pipelineDesc.computeFunction = result.rayGen.function;
	if ([pipelineDesc respondsToSelector:@selector(setMaxCallStackDepth:)])
	{
		pipelineDesc.maxCallStackDepth = desc.maxRecursionDepth;
	}

	NSMutableArray<id<MTLFunction>>* linkedFunctions = [NSMutableArray new];
	if (result.miss.function)
	{
		[linkedFunctions addObject:result.miss.function];
	}
	if (result.closestHit.function)
	{
		[linkedFunctions addObject:result.closestHit.function];
	}
	if (result.anyHit.function)
	{
		[linkedFunctions addObject:result.anyHit.function];
	}

	if ([linkedFunctions count] > 0)
	{
		MTLLinkedFunctions* linked = [MTLLinkedFunctions linkedFunctions];
		linked.functions = linkedFunctions;
		pipelineDesc.linkedFunctions = linked;
	}
	[linkedFunctions release];

	NSError* error = nullptr;
	result.rayGenPipeline = [g_metalDevice newComputePipelineStateWithDescriptor:pipelineDesc
		options:MTLPipelineOptionNone
		reflection:nil
		error:&error];
	[pipelineDesc release];

	if (error)
	{
		Log::error("Failed to create ray tracing pipeline state: %s",
			[error.localizedDescription cStringUsingEncoding:NSASCIIStringEncoding]);
		result.destroy();
		return InvalidResourceHandle();
	}

	const auto& dsetDesc = desc.bindings.descriptorSets[0];
	for (u32 i = 0; i < GfxShaderBindingDesc::MaxDescriptorSets; ++i)
	{
		if (!desc.bindings.descriptorSets[i].isEmpty())
		{
			result.descriptorSetCount = i + 1;
		}
	}
	result.defaultDescriptorSet = createDescriptorSet(dsetDesc);

	return GfxDevice::makeOwn(retainResource(g_device->m_resources.rayTracingPipelines, result));
}

const u8* Gfx_GetRayTracingShaderHandle(GfxRayTracingPipelineArg, GfxRayTracingShaderType, u32)
{
	Log::error("Ray tracing shader handles are not supported on Metal");
	return nullptr;
}

void Gfx_Release(GfxTechnique h)
{
	releaseResource(g_device->m_resources.techniques, h);
}

void Gfx_Retain(GfxRayTracingPipeline h)
{
	g_device->m_resources.rayTracingPipelines[h].addReference();
}

void Gfx_Release(GfxRayTracingPipeline h)
{
	releaseResource(g_device->m_resources.rayTracingPipelines, h);
}

void Gfx_TraceRays(GfxContext* rc, GfxRayTracingPipelineArg pipelineHandle, GfxBufferArg hitGroups, u32 width, u32 height, u32 depth)
{
	RUSH_ASSERT_MSG(rc->m_commandEncoder == nil, "Can't execute ray tracing inside graphics render pass!");
	(void)hitGroups;

	if (rc->m_pendingRayTracingPipeline != pipelineHandle)
	{
		rc->m_pendingTechnique.reset();
		rc->m_pendingRayTracingPipeline.retain(pipelineHandle);
		rc->m_dirtyState |= GfxContext::DirtyStateFlag_Pipeline
			| GfxContext::DirtyStateFlag_Descriptors
			| GfxContext::DirtyStateFlag_DescriptorSet;
	}

	rc->applyState();

	[rc->m_computeCommandEncoder
		dispatchThreadgroups:MTLSizeMake(width, height, depth)
		threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
}

// texture

static MTLPixelFormat convertPixelFormat(GfxFormat format)
{
	switch(format)
	{
		default:
		case GfxFormat_RGB8_Unorm:
			Log::error("Unsupported pixel format");
			return MTLPixelFormatInvalid;
		case GfxFormat_D24_Unorm_S8_Uint:
			return MTLPixelFormatDepth24Unorm_Stencil8;
		case GfxFormat_D24_Unorm_X8:
			return MTLPixelFormatDepth24Unorm_Stencil8;
		case GfxFormat_D32_Float:
			return MTLPixelFormatDepth32Float;
		case GfxFormat_D32_Float_S8_Uint:
			return MTLPixelFormatDepth32Float_Stencil8;
		case GfxFormat_R8_Unorm:
			return MTLPixelFormatR8Unorm;
		case GfxFormat_R16_Float:
			return MTLPixelFormatR16Float;
		case GfxFormat_R16_Uint:
			return MTLPixelFormatR16Uint;
		case GfxFormat_R32_Float:
			return MTLPixelFormatR32Float;
		case GfxFormat_R32_Uint:
			return MTLPixelFormatR32Uint;
		case GfxFormat_RG16_Float:
			return MTLPixelFormatRG16Float;
		case GfxFormat_RG32_Float:
			return MTLPixelFormatRG32Float;
		case GfxFormat_RGBA16_Float:
			return MTLPixelFormatRGBA16Float;
		case GfxFormat_RGBA16_Unorm:
			return MTLPixelFormatRGBA16Unorm;
		case GfxFormat_RGBA32_Float:
			return MTLPixelFormatRGBA32Float;
		case GfxFormat_RGBA8_Unorm:
			return MTLPixelFormatRGBA8Unorm;
		case GfxFormat_RGBA8_sRGB:
			return MTLPixelFormatRGBA8Unorm_sRGB;
		case GfxFormat_BGRA8_Unorm:
			return MTLPixelFormatBGRA8Unorm;
		case GfxFormat_BGRA8_sRGB:
			return MTLPixelFormatBGRA8Unorm_sRGB;
		case GfxFormat_BC1_Unorm:
			return MTLPixelFormatBC1_RGBA;
		case GfxFormat_BC1_Unorm_sRGB:
			return MTLPixelFormatBC1_RGBA_sRGB;
		case GfxFormat_BC3_Unorm:
			return MTLPixelFormatBC3_RGBA;
		case GfxFormat_BC3_Unorm_sRGB:
			return MTLPixelFormatBC3_RGBA_sRGB;
		case GfxFormat_BC5_Unorm:
			return MTLPixelFormatBC5_RGUnorm;
		case GfxFormat_BC6H_SFloat:
			return MTLPixelFormatBC6H_RGBFloat;
		case GfxFormat_BC6H_UFloat:
			return MTLPixelFormatBC6H_RGBUfloat;
		case GfxFormat_BC7_Unorm:
			return MTLPixelFormatBC7_RGBAUnorm;
		case GfxFormat_BC7_Unorm_sRGB:
			return MTLPixelFormatBC7_RGBAUnorm_sRGB;
	};
}

TextureMTL TextureMTL::create(const GfxTextureDesc& desc, const GfxTextureData* data, u32 count, const void* pixels)
{
	const bool isRenderTarget = !!(desc.usage & GfxUsageFlags::RenderTarget);
	const bool isDepthStencil = !!(desc.usage & GfxUsageFlags::DepthStencil);
	const bool isCube = desc.type == TextureType::TexCube;
	const bool isBlockCompressed = isGfxFormatBlockCompressed(desc.format);
	const int blockDim = isBlockCompressed ? 4 : 1;

	TextureMTL result;
	result.uniqueId = g_device->generateId();
	result.desc = desc;

	MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor new];

	textureDescriptor.width = desc.width;
	textureDescriptor.height = desc.height;
	textureDescriptor.depth = desc.isArray() ? 1 : desc.depth;
	textureDescriptor.pixelFormat = convertPixelFormat(desc.format);
	if (textureDescriptor.pixelFormat == MTLPixelFormatInvalid)
	{
		Log::error("Invalid pixel format %u for texture '%s'", u32(desc.format),
		    desc.debugName ? desc.debugName : "<unnamed>");
		[textureDescriptor release];
		return result;
	}
	textureDescriptor.mipmapLevelCount = desc.samples > 1 ? 1 : desc.mips;
	textureDescriptor.sampleCount = desc.samples > 0 ? desc.samples : 1;
	textureDescriptor.arrayLength = desc.isArray() ? desc.depth : 1;
	textureDescriptor.textureType = isCube ? MTLTextureTypeCube
		: (desc.samples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D);

	if (isRenderTarget || isDepthStencil)
	{
		textureDescriptor.storageMode = MTLStorageModePrivate;
		textureDescriptor.usage = MTLTextureUsageRenderTarget;
	}

	if (!!(desc.usage & GfxUsageFlags::ShaderResource))
	{
		textureDescriptor.usage |= MTLTextureUsageShaderRead;
	}

	if (!!(desc.usage & GfxUsageFlags::StorageImage))
	{
		textureDescriptor.usage |= MTLTextureUsageShaderWrite;
	}

	result.native = [g_metalDevice newTextureWithDescriptor:textureDescriptor];

	const u32 bitsPerPixel = getBitsPerPixel(desc.format);

	for (u32 i=0; i<count; ++i)
	{
		const u32 mipLevel = data[i].mip;
		const u32 mipWidth = max<u32>(1, (desc.width >> mipLevel));
		const u32 mipHeight = max<u32>(1, (desc.height >> mipLevel));
		const u32 mipDepth = max<u32>(1, (desc.depth >> mipLevel));

		const GfxTextureData& regionData = data[i];
		MTLRegion region = { { 0, 0, 0 }, { mipWidth, mipHeight, mipDepth } };

		const u32 widthInBlocks = divUp(mipWidth, blockDim);
		const u32 heightInBlocks = divUp(mipHeight, blockDim);

		//const u32 srcPitch = (getBitsPerPixel(desc.format) * mipWidth) / 8;

		const u8* srcPixels = reinterpret_cast<const u8*>(pixels) + data[i].offset;
		RUSH_ASSERT(srcPixels);

		const u32 rowSizeBytes = widthInBlocks * (blockDim*blockDim*bitsPerPixel) / 8;
		const u32 levelSizeBytes = widthInBlocks * heightInBlocks * (blockDim*blockDim*bitsPerPixel) / 8;

		[result.native replaceRegion:region
			mipmapLevel:mipLevel
			slice:regionData.slice
			withBytes:srcPixels
			bytesPerRow:rowSizeBytes
			bytesPerImage:levelSizeBytes];
	}

	[textureDescriptor release];
	return result;
}

void TextureMTL::destroy()
{
	[native release];
}

GfxOwn<GfxTexture> Gfx_CreateTexture(const GfxTextureDesc& desc, const GfxTextureData* data, u32 count, const void* pixels)
{
	return GfxDevice::makeOwn(retainResource(g_device->m_resources.textures, TextureMTL::create(desc, data, count, pixels)));
}

const GfxTextureDesc& Gfx_GetTextureDesc(GfxTextureArg h)
{
	if (h.valid())
	{
		return g_device->m_resources.textures[h].desc;
	}
	else
	{
		static const GfxTextureDesc desc = GfxTextureDesc::make2D(1, 1, GfxFormat_Unknown);
		return desc;
	}

}

void Gfx_Release(GfxTexture h)
{
	releaseResource(g_device->m_resources.textures, h);
}

// blend state

void BlendStateMTL::destroy()
{
}

GfxOwn<GfxBlendState> Gfx_CreateBlendState(const GfxBlendStateDesc& desc)
{
	BlendStateMTL result;
	result.uniqueId = g_device->generateId();
	result.desc = desc;
	return GfxDevice::makeOwn(retainResource(g_device->m_resources.blendStates, result));
}

void Gfx_Release(GfxBlendState h)
{
	releaseResource(g_device->m_resources.blendStates, h);
}

// sampler state

void SamplerMTL::destroy()
{
	[native release];
}

static MTLSamplerAddressMode convertSamplerAddressMode(GfxTextureWrap mode)
{
	switch (mode)
	{
	default:
		Log::error("Unexpected wrap mode");
	case GfxTextureWrap::Wrap:
		return MTLSamplerAddressModeRepeat;
	case GfxTextureWrap::Mirror:
		return MTLSamplerAddressModeMirrorRepeat;
	case GfxTextureWrap::Clamp:
		return MTLSamplerAddressModeClampToEdge;
	}
}

static MTLSamplerMinMagFilter convertFilter(GfxTextureFilter filter)
{
	switch (filter)
	{
	default:
		Log::error("Unexpected filter");
	case GfxTextureFilter::Point:
		return MTLSamplerMinMagFilterNearest;
	case GfxTextureFilter::Linear:
		return MTLSamplerMinMagFilterLinear;
	}
}

static MTLSamplerMipFilter convertMipFilter(GfxTextureFilter filter)
{
	switch (filter)
	{
	default:
		Log::error("Unexpected filter");
	case GfxTextureFilter::Point:
		return MTLSamplerMipFilterNearest;
	case GfxTextureFilter::Linear:
		return MTLSamplerMipFilterLinear;
	}
}

GfxOwn<GfxSampler> Gfx_CreateSamplerState(const GfxSamplerDesc& desc)
{
	MTLSamplerDescriptor* samplerDescriptor = [MTLSamplerDescriptor new];

	samplerDescriptor.supportArgumentBuffers = YES;

	samplerDescriptor.sAddressMode = convertSamplerAddressMode(desc.wrapU);
	samplerDescriptor.tAddressMode = convertSamplerAddressMode(desc.wrapV);
	samplerDescriptor.rAddressMode = convertSamplerAddressMode(desc.wrapW);

	samplerDescriptor.minFilter = convertFilter(desc.filterMin);
	samplerDescriptor.magFilter = convertFilter(desc.filterMag);
	samplerDescriptor.mipFilter = convertMipFilter(desc.filterMip);

	samplerDescriptor.maxAnisotropy = (int)desc.anisotropy;

	if (desc.compareEnable)
	{
		samplerDescriptor.compareFunction = convertCompareFunc(desc.compareFunc);
	}

	SamplerMTL result;
	
	result.uniqueId = g_device->generateId();
	result.native = [g_metalDevice newSamplerStateWithDescriptor:samplerDescriptor];

	[samplerDescriptor release];

	return GfxDevice::makeOwn(retainResource(g_device->m_resources.samplers, result));
}

void Gfx_Release(GfxSampler h)
{
	releaseResource(g_device->m_resources.samplers, h);
}

// depth stencil state

void DepthStencilStateMTL::destroy()
{
	[native release];
}

GfxOwn<GfxDepthStencilState> Gfx_CreateDepthStencilState(const GfxDepthStencilDesc& desc)
{
	DepthStencilStateMTL result;
	result.uniqueId = g_device->generateId();

	MTLDepthStencilDescriptor* descriptor = [MTLDepthStencilDescriptor new];

	descriptor.depthCompareFunction = desc.enable ? convertCompareFunc(desc.compareFunc) : MTLCompareFunctionAlways;
	descriptor.depthWriteEnabled = desc.enable ? desc.writeEnable : false;

	result.native = [g_metalDevice newDepthStencilStateWithDescriptor:descriptor];

	[descriptor release];

	return GfxDevice::makeOwn(retainResource(g_device->m_resources.depthStencilStates, result));
}

void Gfx_Release(GfxDepthStencilState h)
{
	releaseResource(g_device->m_resources.depthStencilStates, h);
}

// rasterizer state

void RasterizerStateMTL::destroy()
{
}

GfxOwn<GfxRasterizerState> Gfx_CreateRasterizerState(const GfxRasterizerDesc& desc)
{
	RasterizerStateMTL result;
	result.uniqueId = g_device->generateId();
	result.desc = desc;
	return GfxDevice::makeOwn(retainResource(g_device->m_resources.rasterizerStates, result));
}

void Gfx_Release(GfxRasterizerState h)
{
	releaseResource(g_device->m_resources.rasterizerStates, h);
}

// buffers

void BufferMTL::destroy()
{
	[native release];
}

void AccelerationStructureMTL::destroy()
{
	[instancedAccelerationStructures release];
	[instanceBuffer release];
	[scratchBuffer release];
	[native release];
}

GfxOwn<GfxBuffer> Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data)
{
	const u32 bufferSize = desc.count * desc.stride;

	BufferMTL res;
	res.uniqueId = g_device->generateId();

	MTLResourceOptions options = 0;
#if TARGET_OS_OSX
	if (!!(desc.flags & GfxBufferFlags::CpuRead))
	{
		options = MTLResourceStorageModeShared;
	}
#endif

	if (data)
	{
		res.native = [g_metalDevice newBufferWithBytes:data length:bufferSize options:options];
	}
	else
	{
		res.native = [g_metalDevice newBufferWithLength:bufferSize options:options];
	}

	if (!!(desc.flags & GfxBufferFlags::Index))
	{
		if(desc.format == GfxFormat_R32_Uint)
		{
			res.indexType = MTLIndexTypeUInt32;
			res.stride = 4;
		}
		else if(desc.format == GfxFormat_R16_Uint)
		{
			res.indexType = MTLIndexTypeUInt16;
			res.stride = 2;
		}
		else
		{
			Log::error("Index buffer format must be R32_Uint or R16_Uint");
		}
	}

	return GfxDevice::makeOwn(retainResource(g_device->m_resources.buffers, res));
}

GfxMappedBuffer Gfx_MapBuffer(GfxBufferArg vb, u32 offset, u32 size)
{
	if (!vb.valid())
	{
		return {};
	}

	BufferMTL& buffer = g_device->m_resources.buffers[vb];
	if ([buffer.native storageMode] == MTLStorageModeManaged)
	{
		id<MTLCommandBuffer> syncCommandBuffer = [g_device->m_commandQueue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [syncCommandBuffer blitCommandEncoder];
		[blit synchronizeResource:buffer.native];
		[blit endEncoding];
		[syncCommandBuffer commit];
		[syncCommandBuffer waitUntilCompleted];
	}
	const u32 bufferSize = (u32)[buffer.native length];
	if (offset > bufferSize)
	{
		Log::error("Gfx_MapBuffer: offset exceeds buffer length");
		return {};
	}

	if (size == 0)
	{
		size = bufferSize - offset;
	}
	else if (offset + size > bufferSize)
	{
		Log::error("Gfx_MapBuffer: range exceeds buffer length");
		return {};
	}

	void* base = [buffer.native contents];
	if (!base)
	{
		Log::error("Gfx_MapBuffer: buffer contents unavailable");
		return {};
	}

	GfxMappedBuffer result;
	result.data = static_cast<u8*>(base) + offset;
	result.size = size;
	result.handle = vb;
	return result;
}

void Gfx_UnmapBuffer(GfxMappedBuffer& lock)
{
	if (!lock.handle.valid())
	{
		return;
	}

	BufferMTL& buffer = g_device->m_resources.buffers[lock.handle];
	if (buffer.native)
	{
		[buffer.native didModifyRange:NSMakeRange(0, [buffer.native length])];
	}
}

void Gfx_UpdateBuffer(GfxContext* rc, GfxBufferArg h, const void* data, u32 size)
{
	if (!h.valid())
	{
		return;
	}

	BufferMTL& buffer = g_device->m_resources.buffers[h];

	// FIXME: recreating buffer invalidates existing bindings/in-flight usage; needs rebinding or update path.
	[buffer.native release];
	buffer.native = [g_metalDevice newBufferWithBytes:data length:size options:0];

	// TODO: re-bind buffer if necessary
}

void* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferArg h, u32 size)
{
	if (!h.valid())
	{
		return nullptr;
	}

	BufferMTL& buffer = g_device->m_resources.buffers[h];
	if (size == 0 && buffer.native)
	{
		size = (u32)[buffer.native length];
	}

	if (!buffer.native || (size > 0 && [buffer.native length] < size))
	{
		[buffer.native release];
		buffer.native = [g_metalDevice newBufferWithLength:size options:0];
	}

	void* base = buffer.native ? [buffer.native contents] : nullptr;
	if (!base)
	{
		Log::error("Gfx_BeginUpdateBuffer: buffer contents unavailable");
		return nullptr;
	}

	return base;
}

void Gfx_EndUpdateBuffer(GfxContext* rc, GfxBufferArg h)
{
	if (!h.valid())
	{
		return;
	}

	BufferMTL& buffer = g_device->m_resources.buffers[h];
	[buffer.native didModifyRange:NSMakeRange(0, [buffer.native length])];
}

void Gfx_Release(GfxBuffer h)
{
	releaseResource(g_device->m_resources.buffers, h);
}

static MTLPrimitiveAccelerationStructureDescriptor* createPrimitiveAccelerationStructureDescriptor(
    const DynamicArray<GfxRayTracingGeometryDesc>& geometries)
{
	MTLPrimitiveAccelerationStructureDescriptor* accelDesc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
	NSMutableArray<MTLAccelerationStructureGeometryDescriptor*>* geometryDescriptors =
	    [NSMutableArray arrayWithCapacity:geometries.size()];

	for (const auto& geometryDesc : geometries)
	{
		RUSH_ASSERT(geometryDesc.type == GfxRayTracingGeometryType::Triangles);
		RUSH_ASSERT(
		    geometryDesc.indexFormat == GfxFormat_R32_Uint || geometryDesc.indexFormat == GfxFormat_R16_Uint);

		BufferMTL& vertexBuffer = g_device->m_resources.buffers[geometryDesc.vertexBuffer];
		BufferMTL& indexBuffer = g_device->m_resources.buffers[geometryDesc.indexBuffer];

		MTLAccelerationStructureTriangleGeometryDescriptor* triangle =
		    [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
		triangle.vertexBuffer = vertexBuffer.native;
		triangle.vertexBufferOffset = geometryDesc.vertexBufferOffset;
		triangle.vertexFormat = convertRayTracingVertexFormat(geometryDesc.vertexFormat);
		triangle.vertexStride = geometryDesc.vertexStride;
		triangle.indexBuffer = indexBuffer.native;
		triangle.indexBufferOffset = geometryDesc.indexBufferOffset;
		triangle.indexType = convertRayTracingIndexType(geometryDesc.indexFormat);
		triangle.triangleCount = geometryDesc.indexCount / 3;
		triangle.opaque = geometryDesc.isOpaque ? YES : NO;

		if (geometryDesc.transformBuffer.valid())
		{
			BufferMTL& transformBuffer = g_device->m_resources.buffers[geometryDesc.transformBuffer];
			triangle.transformationMatrixBuffer = transformBuffer.native;
			triangle.transformationMatrixBufferOffset = geometryDesc.transformBufferOffset;
		}

		[geometryDescriptors addObject:triangle];
	}

	accelDesc.geometryDescriptors = geometryDescriptors;
	return accelDesc;
}

static void ensureAccelerationStructureResources(
	AccelerationStructureMTL& accel,
	MTLAccelerationStructureDescriptor* descriptor)
{
	MTLAccelerationStructureSizes sizes = [g_metalDevice accelerationStructureSizesWithDescriptor:descriptor];
	if (!accel.native || sizes.accelerationStructureSize > accel.accelerationStructureSize)
	{
		[accel.native release];
		accel.native = [g_metalDevice newAccelerationStructureWithSize:sizes.accelerationStructureSize];
		accel.accelerationStructureSize = sizes.accelerationStructureSize;
	}

	if (sizes.buildScratchBufferSize == 0)
	{
		[accel.scratchBuffer release];
		accel.scratchBuffer = nil;
		accel.scratchBufferSize = 0;
		return;
	}

	if (!accel.scratchBuffer || sizes.buildScratchBufferSize > accel.scratchBufferSize)
	{
		[accel.scratchBuffer release];
		accel.scratchBuffer = [g_metalDevice newBufferWithLength:sizes.buildScratchBufferSize
			options:MTLResourceStorageModePrivate];
		accel.scratchBufferSize = sizes.buildScratchBufferSize;
	}
}

static void convertInstanceTransform(const float* rowMajor3x4, MTLPackedFloat4x3& outMatrix)
{
	// Convert row-major 3x4 (Vulkan-style) to column-major 4x3 (Metal).
	const float m00 = rowMajor3x4[0];
	const float m01 = rowMajor3x4[1];
	const float m02 = rowMajor3x4[2];
	const float m03 = rowMajor3x4[3];
	const float m10 = rowMajor3x4[4];
	const float m11 = rowMajor3x4[5];
	const float m12 = rowMajor3x4[6];
	const float m13 = rowMajor3x4[7];
	const float m20 = rowMajor3x4[8];
	const float m21 = rowMajor3x4[9];
	const float m22 = rowMajor3x4[10];
	const float m23 = rowMajor3x4[11];

	outMatrix.columns[0].x = m00;
	outMatrix.columns[0].y = m10;
	outMatrix.columns[0].z = m20;
	outMatrix.columns[1].x = m01;
	outMatrix.columns[1].y = m11;
	outMatrix.columns[1].z = m21;
	outMatrix.columns[2].x = m02;
	outMatrix.columns[2].y = m12;
	outMatrix.columns[2].z = m22;
	outMatrix.columns[3].x = m03;
	outMatrix.columns[3].y = m13;
	outMatrix.columns[3].z = m23;
}

static bool findOrAppendInstanceAccelerationStructure(
	NSMutableArray<id<MTLAccelerationStructure>>* instances,
	DynamicArray<u64>& handles,
	u64 handle,
	u32& outIndex)
{
	if (handle == 0)
	{
		return false;
	}

	for (u32 i = 0; i < handles.size(); ++i)
	{
		if (handles[i] == handle)
		{
			outIndex = i;
			return true;
		}
	}

	handles.push_back(handle);
	outIndex = u32(handles.size() - 1);
	GfxAccelerationStructure accelHandle(UntypedResourceHandle(
	    static_cast<UntypedResourceHandle::IndexType>(handle)));
	AccelerationStructureMTL& blas = g_device->m_resources.accelerationStructures[accelHandle];
	[instances addObject:blas.native];
	return true;
}

GfxOwn<GfxAccelerationStructure> Gfx_CreateAccelerationStructure(const GfxAccelerationStructureDesc& desc)
{
	AccelerationStructureMTL result;
	result.uniqueId = g_device->generateId();
	result.type = desc.type;
	result.instanceCount = desc.instanceCount;

	if (desc.type == GfxAccelerationStructureType::BottomLevel)
	{
		result.geometries.resize(desc.geometyCount);
		for (u32 i = 0; i < desc.geometyCount; ++i)
		{
			result.geometries[i] = desc.geometries[i];
		}

		MTLPrimitiveAccelerationStructureDescriptor* accelDesc =
		    createPrimitiveAccelerationStructureDescriptor(result.geometries);
		ensureAccelerationStructureResources(result, accelDesc);
	}
	else if (desc.type == GfxAccelerationStructureType::TopLevel)
	{
		MTLInstanceAccelerationStructureDescriptor* accelDesc = [MTLInstanceAccelerationStructureDescriptor descriptor];
		accelDesc.instanceCount = desc.instanceCount;
		accelDesc.instanceDescriptorStride = sizeof(MTLAccelerationStructureInstanceDescriptor);
		accelDesc.instancedAccelerationStructures = [NSArray array];
		if ([accelDesc respondsToSelector:@selector(setInstanceDescriptorType:)])
		{
			accelDesc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeDefault;
		}
		ensureAccelerationStructureResources(result, accelDesc);
	}
	else
	{
		Log::error("Unexpected acceleration structure type");
	}

	return GfxDevice::makeOwn(retainResource(g_device->m_resources.accelerationStructures, result));
}

u64 Gfx_GetAccelerationStructureHandle(GfxAccelerationStructureArg h)
{
	GfxAccelerationStructure handle = h;
	return handle.valid() ? handle.index() : 0;
}

void Gfx_BuildAccelerationStructure(GfxContext* ctx, GfxAccelerationStructureArg h, GfxBufferArg instanceBuffer)
{
	AccelerationStructureMTL& accel = g_device->m_resources.accelerationStructures[h];

	if (accel.type == GfxAccelerationStructureType::BottomLevel)
	{
		MTLPrimitiveAccelerationStructureDescriptor* accelDesc =
		    createPrimitiveAccelerationStructureDescriptor(accel.geometries);
		ensureAccelerationStructureResources(accel, accelDesc);

		id<MTLAccelerationStructureCommandEncoder> encoder =
		    [g_device->m_commandBuffer accelerationStructureCommandEncoder];
		for (const auto& geometryDesc : accel.geometries)
		{
			if (geometryDesc.vertexBuffer.valid())
			{
				BufferMTL& vertexBuffer = g_device->m_resources.buffers[geometryDesc.vertexBuffer];
				[encoder useResource:vertexBuffer.native usage:MTLResourceUsageRead];
			}
			if (geometryDesc.indexBuffer.valid())
			{
				BufferMTL& indexBuffer = g_device->m_resources.buffers[geometryDesc.indexBuffer];
				[encoder useResource:indexBuffer.native usage:MTLResourceUsageRead];
			}
			if (geometryDesc.transformBuffer.valid())
			{
				BufferMTL& transformBuffer = g_device->m_resources.buffers[geometryDesc.transformBuffer];
				[encoder useResource:transformBuffer.native usage:MTLResourceUsageRead];
			}
		}
		[encoder buildAccelerationStructure:accel.native
		         descriptor:accelDesc
		       scratchBuffer:accel.scratchBuffer
		 scratchBufferOffset:0];
		[encoder endEncoding];
	}
	else if (accel.type == GfxAccelerationStructureType::TopLevel)
	{
		if (!instanceBuffer.valid())
		{
			Log::error("TLAS build requires an instance buffer");
			return;
		}

		BufferMTL& instanceBufferMTL = g_device->m_resources.buffers[instanceBuffer];
		const GfxRayTracingInstanceDesc* srcInstances =
		    static_cast<const GfxRayTracingInstanceDesc*>([instanceBufferMTL.native contents]);
		if (!srcInstances)
		{
			Log::error("Instance buffer contents unavailable");
			return;
		}

		DynamicArray<MTLAccelerationStructureInstanceDescriptor> instances;
		instances.resize(accel.instanceCount);

		DynamicArray<u64> uniqueHandles;
		uniqueHandles.reserve(accel.instanceCount);
		NSMutableArray<id<MTLAccelerationStructure>>* instancedAccelerationStructures = [NSMutableArray new];

	for (u32 i = 0; i < accel.instanceCount; ++i)
	{
		u64 handle = srcInstances[i].accelerationStructureHandle;
		u32 accelIndex = 0;
		if (!findOrAppendInstanceAccelerationStructure(
			instancedAccelerationStructures, uniqueHandles, handle, accelIndex))
		{
			Log::error("TLAS instance has invalid BLAS handle");
			continue;
		}

		MTLAccelerationStructureInstanceDescriptor& dst = instances[i];
			memset(&dst, 0, sizeof(dst));
			convertInstanceTransform(srcInstances[i].transform, dst.transformationMatrix);
			dst.options = MTLAccelerationStructureInstanceOptionNone;
			dst.mask = srcInstances[i].instanceMask;
			dst.intersectionFunctionTableOffset = srcInstances[i].instanceContributionToHitGroupIndex;
			dst.accelerationStructureIndex = accelIndex;
		}

		[accel.instanceBuffer release];
		accel.instanceBuffer = [g_metalDevice newBufferWithBytes:instances.data()
			length:instances.size() * sizeof(MTLAccelerationStructureInstanceDescriptor)
			options:0];

		[accel.instancedAccelerationStructures release];
		accel.instancedAccelerationStructures = [instancedAccelerationStructures copy];
		[instancedAccelerationStructures release];

		MTLInstanceAccelerationStructureDescriptor* accelDesc = [MTLInstanceAccelerationStructureDescriptor descriptor];
		accelDesc.instanceDescriptorBuffer = accel.instanceBuffer;
		accelDesc.instanceCount = accel.instanceCount;
		accelDesc.instanceDescriptorStride = sizeof(MTLAccelerationStructureInstanceDescriptor);
		accelDesc.instancedAccelerationStructures = accel.instancedAccelerationStructures;
		if ([accelDesc respondsToSelector:@selector(setInstanceDescriptorType:)])
		{
			accelDesc.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeDefault;
		}

		ensureAccelerationStructureResources(accel, accelDesc);

		id<MTLAccelerationStructureCommandEncoder> encoder =
		    [g_device->m_commandBuffer accelerationStructureCommandEncoder];
		[encoder useResource:accel.instanceBuffer usage:MTLResourceUsageRead];
		for (id<MTLAccelerationStructure> blas in accel.instancedAccelerationStructures)
		{
			[encoder useResource:blas usage:MTLResourceUsageRead];
		}
		[encoder buildAccelerationStructure:accel.native
		         descriptor:accelDesc
		       scratchBuffer:accel.scratchBuffer
		 scratchBufferOffset:0];
		[encoder endEncoding];
	}
}

void Gfx_Retain(GfxAccelerationStructure h)
{
	g_device->m_resources.accelerationStructures[h].addReference();
}

void Gfx_Release(GfxAccelerationStructure h)
{
	releaseResource(g_device->m_resources.accelerationStructures, h);
}

// context

GfxContext::GfxContext()
{
    MTLDepthStencilDescriptor* depthStencilDescriptor = [MTLDepthStencilDescriptor new];
    depthStencilDescriptor.depthCompareFunction = MTLCompareFunctionAlways;
    depthStencilDescriptor.depthWriteEnabled = NO;
    m_depthStencilState = [g_metalDevice newDepthStencilStateWithDescriptor:depthStencilDescriptor];
	[depthStencilDescriptor release];
}

GfxContext::~GfxContext()
{
	[m_depthStencilState release];
	[m_indexBuffer release];
	RUSH_ASSERT(m_commandEncoder == nil);
	RUSH_ASSERT(m_computeCommandEncoder == nil);
}

static MTLBlendOperation convertBlendOp(GfxBlendOp blendOp)
{
	switch (blendOp)
	{
	default:
		Log::error("Unexpected blend operation");
	case GfxBlendOp::Add:
		return MTLBlendOperationAdd;
	case GfxBlendOp::Subtract:
		return MTLBlendOperationSubtract;
	case GfxBlendOp::RevSubtract:
		return MTLBlendOperationReverseSubtract;
	case GfxBlendOp::Min:
		return MTLBlendOperationMin;
	case GfxBlendOp::Max:
		return MTLBlendOperationMax;
	}
}

static MTLBlendFactor convertBlendParam(GfxBlendParam blendParam)
{
	switch (blendParam)
	{
	default:
		Log::error("Unexpected blend factor");
	case GfxBlendParam::Zero:
		return MTLBlendFactorZero;
	case GfxBlendParam::One:
		return MTLBlendFactorOne;
	case GfxBlendParam::SrcColor:
		return MTLBlendFactorSourceColor;
	case GfxBlendParam::InvSrcColor:
		return MTLBlendFactorOneMinusSourceColor;
	case GfxBlendParam::SrcAlpha:
		return MTLBlendFactorSourceAlpha;
	case GfxBlendParam::InvSrcAlpha:
		return MTLBlendFactorOneMinusSourceAlpha;
	case GfxBlendParam::DestAlpha:
		return MTLBlendFactorDestinationAlpha;
	case GfxBlendParam::InvDestAlpha:
		return MTLBlendFactorOneMinusDestinationAlpha;
	case GfxBlendParam::DestColor:
		return MTLBlendFactorDestinationColor;
	case GfxBlendParam::InvDestColor:
		return MTLBlendFactorOneMinusDestinationColor;
	}
}

static void useResources(id commandEncoder, DescriptorSetMTL& ds)
{
	for (u64 j=0; j<ds.constantBuffers.size(); ++j)
	{
		[commandEncoder
		 useResource:g_device->m_resources.buffers[ds.constantBuffers[j]].native
		 usage:MTLResourceUsageRead];
	}

	for (u64 j=0; j<ds.textures.size(); ++j)
	{
		[commandEncoder
		 useResource:g_device->m_resources.textures[ds.textures[j]].native
		 usage:MTLResourceUsageRead];
	}

	for (u64 j=0; j<ds.storageImages.size(); ++j)
	{
		[commandEncoder
		 useResource:g_device->m_resources.textures[ds.storageImages[j]].native
		 usage:MTLResourceUsageRead | MTLResourceUsageWrite];
	}

	for (u64 j=0; j<ds.storageBuffers.size(); ++j)
	{
		[commandEncoder
		 useResource:g_device->m_resources.buffers[ds.storageBuffers[j]].native
		 usage:MTLResourceUsageRead | MTLResourceUsageWrite];
	}

	for (u64 j=0; j<ds.accelerationStructures.size(); ++j)
	{
		AccelerationStructureMTL& accel = g_device->m_resources.accelerationStructures[ds.accelerationStructures[j]];
		if (accel.native)
		{
			[commandEncoder useResource:accel.native usage:MTLResourceUsageRead];
		}
		if (accel.instanceBuffer)
		{
			[commandEncoder useResource:accel.instanceBuffer usage:MTLResourceUsageRead];
		}
		if (accel.instancedAccelerationStructures)
		{
			for (id<MTLAccelerationStructure> blas in accel.instancedAccelerationStructures)
			{
				[commandEncoder useResource:blas usage:MTLResourceUsageRead];
			}
		}
	}
}

void GfxContext::applyState()
{
	// TODO: cache pipelines
	if (m_dirtyState == 0)
	{
		return;
	}

	const bool useRayTracing = m_pendingRayTracingPipeline.valid();
	const bool useTechnique = m_pendingTechnique.valid();
	RUSH_ASSERT(useRayTracing || useTechnique);

	TechniqueMTL* technique = nullptr;
	RayTracingPipelineMTL* rayTracingPipeline = nullptr;
	const GfxShaderBindingDesc* bindingDesc = nullptr;
	DescriptorSetMTL* defaultDescriptorSet = nullptr;
	u32 descriptorSetCount = 0;

	if (useRayTracing)
	{
		rayTracingPipeline = &g_device->m_resources.rayTracingPipelines[m_pendingRayTracingPipeline.get()];
		bindingDesc = &rayTracingPipeline->bindings;
		defaultDescriptorSet = &rayTracingPipeline->defaultDescriptorSet;
		descriptorSetCount = rayTracingPipeline->descriptorSetCount;
	}
	else
	{
		technique = &g_device->m_resources.techniques[m_pendingTechnique.get()];
		bindingDesc = &technique->desc.bindings;
		defaultDescriptorSet = &technique->defaultDescriptorSet;
		descriptorSetCount = technique->descriptorSetCount;
	}

	if ((m_dirtyState & DirtyStateFlag_Pipeline) && bindingDesc->useDefaultDescriptorSet)
	{
		m_dirtyState |= DirtyStateFlag_Descriptors;
	}

	if (m_dirtyState & DirtyStateFlag_Pipeline)
	{
		if (useRayTracing)
		{
			if (!m_computeCommandEncoder)
			{
				m_computeCommandEncoder = [g_device->m_commandBuffer computeCommandEncoder];
				[m_computeCommandEncoder retain];
			}
			[m_computeCommandEncoder setComputePipelineState:rayTracingPipeline->rayGenPipeline];
		}
		else if (technique->computePipeline)
		{
			if (!m_computeCommandEncoder)
			{
				m_computeCommandEncoder = [g_device->m_commandBuffer computeCommandEncoder];
				[m_computeCommandEncoder retain];
			}

			[m_computeCommandEncoder setComputePipelineState:technique->computePipeline];
		}
		else
		{
			MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];

			const auto& vertexFormat = g_device->m_resources.vertexFormats[technique->vf.get()];
			const auto& vertexShader = g_device->m_resources.shaders[technique->vs.get()];
			const auto& pixelShader = g_device->m_resources.shaders[technique->ps.get()];

			pipelineDescriptor.inputPrimitiveTopology = m_primitiveTopology;
			pipelineDescriptor.vertexDescriptor = vertexFormat.native;
			pipelineDescriptor.vertexFunction = vertexShader.function;
			pipelineDescriptor.fragmentFunction = pixelShader.function;

			if (m_passDesc.depth.valid())
			{
				pipelineDescriptor.depthAttachmentPixelFormat =
					[g_device->m_resources.textures[m_passDesc.depth].native pixelFormat];
			}
			else
			{
				pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
			}

			const auto& blendState = g_device->m_resources.blendStates[m_pendingBlendState.get()].desc;

			const bool useBackBuffer = !m_passDesc.color[0].valid() && !m_passDesc.depth.valid();
			u32 sampleCount = 1;
			if (!useBackBuffer)
			{
				if (m_passDesc.color[0].valid())
				{
					sampleCount = g_device->m_resources.textures[m_passDesc.color[0]].desc.samples;
				}
				else if (m_passDesc.depth.valid())
				{
					sampleCount = g_device->m_resources.textures[m_passDesc.depth].desc.samples;
				}
			}
			const u32 rasterSamples = sampleCount > 0 ? sampleCount : 1;
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
			pipelineDescriptor.rasterSampleCount = rasterSamples;
#else
			pipelineDescriptor.sampleCount = rasterSamples;
#endif

			for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
			{
				if ((!useBackBuffer || i!=0) && !m_passDesc.color[i].valid())
				{
					break;
				}

				auto colorTarget = m_passDesc.color[i].valid() ? g_device->m_resources.textures[m_passDesc.color[i]].native : g_device->m_backBufferTexture;

				pipelineDescriptor.colorAttachments[i].pixelFormat = [colorTarget pixelFormat];
				// TODO: per-RT blend states
				pipelineDescriptor.colorAttachments[i].blendingEnabled = blendState.enable;
				pipelineDescriptor.colorAttachments[i].rgbBlendOperation = convertBlendOp(blendState.op);
				pipelineDescriptor.colorAttachments[i].sourceRGBBlendFactor = convertBlendParam(blendState.src);
				pipelineDescriptor.colorAttachments[i].destinationRGBBlendFactor = convertBlendParam(blendState.dst);
				if (blendState.alphaSeparate)
				{
					pipelineDescriptor.colorAttachments[i].alphaBlendOperation = convertBlendOp(blendState.alphaOp);
					pipelineDescriptor.colorAttachments[i].sourceAlphaBlendFactor = convertBlendParam(blendState.alphaSrc);
					pipelineDescriptor.colorAttachments[i].destinationAlphaBlendFactor = convertBlendParam(blendState.alphaDst);
				}
				else
				{
					pipelineDescriptor.colorAttachments[i].alphaBlendOperation = convertBlendOp(blendState.op);
					pipelineDescriptor.colorAttachments[i].sourceAlphaBlendFactor = convertBlendParam(blendState.src);
					pipelineDescriptor.colorAttachments[i].destinationAlphaBlendFactor = convertBlendParam(blendState.dst);
				}
			}

			NSError* error = nullptr;
			id<MTLRenderPipelineState> pipelineState = [g_metalDevice newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
			if (error)
			{
				Log::error("Failed to create render pipeline state: %s",
					[error.localizedDescription cStringUsingEncoding:NSASCIIStringEncoding]);
			}

			[m_commandEncoder setRenderPipelineState:pipelineState];
			if (m_pendingDepthStencilState.valid())
			{
				const auto& state = g_device->m_resources.depthStencilStates[m_pendingDepthStencilState.get()];
				[m_commandEncoder setDepthStencilState:state.native];
			}
			else
			{
				[m_commandEncoder setDepthStencilState:m_depthStencilState];
			}

			if (m_pendingRasterizerState.valid())
			{
				const auto& desc = g_device->m_resources.rasterizerStates[m_pendingRasterizerState.get()].desc;
				MTLCullMode cullMode = desc.cullMode == GfxCullMode::None ? MTLCullModeNone : MTLCullModeBack;
				[m_commandEncoder setCullMode:cullMode];
				[m_commandEncoder setFrontFacingWinding:desc.cullMode == GfxCullMode::CCW ? MTLWindingCounterClockwise : MTLWindingClockwise];
				[m_commandEncoder setTriangleFillMode:desc.fillMode == GfxFillMode::Solid ? MTLTriangleFillModeFill : MTLTriangleFillModeLines];
				[m_commandEncoder setDepthBias:desc.depthBias slopeScale:desc.depthBiasSlopeScale clamp:1.0f];
			}

			[pipelineState release];
			[pipelineDescriptor release];
		}
	}

	if (m_dirtyState & DirtyStateFlag_Descriptors)
	{
		GfxBuffer constantBuffers[RUSH_COUNTOF(m_constantBuffers)];
		GfxSampler samplers[RUSH_COUNTOF(m_samplers)];
		GfxTexture sampledImages[RUSH_COUNTOF(m_sampledImages)];
		GfxTexture storageImages[RUSH_COUNTOF(m_storageImages)];
		GfxBuffer storageBuffers[RUSH_COUNTOF(m_storageBuffers)];
		GfxAccelerationStructure accelStructures[RUSH_COUNTOF(m_accelerationStructures)];

		const auto& dsetDesc = bindingDesc->descriptorSets[0];
		for(u32 i=0; i<dsetDesc.constantBuffers; ++i)
		{
			constantBuffers[i] = m_constantBuffers[i].get();
		}
		for(u32 i=0; i<dsetDesc.samplers; ++i)
		{
			samplers[i] = m_samplers[i].get();
		}
		for(u32 i=0; i<dsetDesc.textures; ++i)
		{
			sampledImages[i] = m_sampledImages[i].get();
		}
		for(u32 i=0; i<dsetDesc.rwImages; ++i)
		{
			storageImages[i] = m_storageImages[i].get();
		}
		for(u32 i=0; i<dsetDesc.rwBuffers; ++i)
		{
			storageBuffers[i] = m_storageBuffers[i].get();
		}
		for(u32 i=0; i<dsetDesc.accelerationStructures; ++i)
		{
			accelStructures[i] = m_accelerationStructures[i].get();
		}
		for(u32 i=0; i<dsetDesc.rwTypedBuffers; ++i)
		{
			//TODO: bind typed storage buffers
		}

		updateDescriptorSet(*defaultDescriptorSet,
							constantBuffers,
							m_constantBufferOffsets,
							samplers,
							sampledImages,
							storageImages,
							storageBuffers,
							accelStructures);

		auto& ds = *defaultDescriptorSet;

		if (m_commandEncoder)
		{
			// TODO: set the buffers to stages present in the current PSO
			[m_commandEncoder setVertexBuffer:ds.argBuffer offset:ds.argBufferOffset atIndex:0];
			[m_commandEncoder setFragmentBuffer:ds.argBuffer offset:ds.argBufferOffset atIndex:0];
			useResources(m_commandEncoder, ds);
		}
		else if (m_computeCommandEncoder)
		{
			[m_computeCommandEncoder setBuffer:ds.argBuffer offset:ds.argBufferOffset atIndex:0];
			useResources(m_computeCommandEncoder, ds);
		}
	}

	if (m_commandEncoder)
	{
		if (m_dirtyState & DirtyStateFlag_DescriptorSet)
		{
			u32 firstDescriptorSet = bindingDesc->useDefaultDescriptorSet ? 1 : 0;
			for (u32 i=firstDescriptorSet; i<descriptorSetCount; ++i)
			{
				DescriptorSetMTL& ds = g_device->m_resources.descriptorSets[m_descriptorSets[i].get()];
				useResources(m_commandEncoder, ds);

				if(!!(ds.desc.stageFlags & GfxStageFlags::Vertex))
				{
					[m_commandEncoder setVertexBuffer:ds.argBuffer offset:ds.argBufferOffset atIndex:i];
				}

				if(!!(ds.desc.stageFlags & GfxStageFlags::Pixel))
				{
					[m_commandEncoder setFragmentBuffer:ds.argBuffer offset:ds.argBufferOffset atIndex:i];
				}
			}
		}
	}
	else if (m_computeCommandEncoder)
	{
		if (m_dirtyState & DirtyStateFlag_DescriptorSet)
		{
			u32 firstDescriptorSet = bindingDesc->useDefaultDescriptorSet ? 1 : 0;
			for (u32 i=firstDescriptorSet; i<descriptorSetCount; ++i)
			{
				DescriptorSetMTL& ds = g_device->m_resources.descriptorSets[m_descriptorSets[i].get()];
				useResources(m_computeCommandEncoder, ds);

				[m_computeCommandEncoder setBuffer:ds.argBuffer offset:ds.argBufferOffset atIndex:i];
			}
		}
	}

	m_dirtyState = 0;
}

GfxContext* Gfx_AcquireContext()
{
	if (g_context == nullptr)
	{
		g_context = new GfxContext;
		g_context->m_refs = 1;
	}
	else
	{
		Gfx_Retain(g_context);
	}
	return g_context;
}

void Gfx_Release(GfxContext* rc)
{
	if (rc->removeReference() > 1)
		return;

	if (rc == g_context)
	{
		g_context = nullptr;
	}

	delete rc;
}

void Gfx_BeginPass(GfxContext* rc, const GfxPassDesc& desc)
{
	if (rc->m_computeCommandEncoder)
	{
		[rc->m_computeCommandEncoder endEncoding];
		[rc->m_computeCommandEncoder release];
		rc->m_computeCommandEncoder = nil;
	}

	MTLRenderPassDescriptor* passDescriptor = [MTLRenderPassDescriptor new];

	rc->m_passDesc = desc;

	// TODO: color-only rendering (no depth buffer bound)
	// TODO: multiple render targets
	// TODO: off-screen render targets
	// TODO: stencil

	RUSH_ASSERT(g_device->m_backBufferTexture);

	const bool useBackBuffer = !desc.color[0].valid() && !desc.depth.valid();

	for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
	{
		if ((!useBackBuffer || i!=0) && !desc.color[i].valid())
		{
			break;
		}

		passDescriptor.colorAttachments[i].texture = desc.color[i].valid() ? g_device->m_resources.textures[desc.color[i]].native : g_device->m_backBufferTexture;
		if (!!(desc.flags & GfxPassFlags::ClearColor))
		{
			passDescriptor.colorAttachments[i].loadAction = MTLLoadActionClear;
		}
		else
		{
			passDescriptor.colorAttachments[i].loadAction = MTLLoadActionLoad; // TODO: use 'don't care' when appropriate
		}
		passDescriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
		passDescriptor.colorAttachments[i].clearColor = MTLClearColorMake(
			desc.clearColors[i].r,
			desc.clearColors[i].g,
			desc.clearColors[i].b,
			desc.clearColors[i].a);
	}

	if (desc.depth.valid())
	{
		GfxTexture depthBuffer = desc.depth;
		passDescriptor.depthAttachment.texture = g_device->m_resources.textures[depthBuffer].native;

		if (!!(desc.flags & GfxPassFlags::ClearDepthStencil))
		{
			passDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
			passDescriptor.depthAttachment.clearDepth = desc.clearDepth;
		}
		else
		{
			passDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
		}

		passDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
	}
	else
	{
		passDescriptor.depthAttachment.texture = nil;
		passDescriptor.depthAttachment.loadAction = MTLLoadActionDontCare;
		passDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
	}

	RUSH_ASSERT(g_device->m_commandBuffer);
	rc->m_commandEncoder = [g_device->m_commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
	[rc->m_commandEncoder retain];

	id<MTLTexture> viewportTexture = passDescriptor.colorAttachments[0].texture;
	if (!viewportTexture)
	{
		viewportTexture = passDescriptor.depthAttachment.texture;
	}
	if (viewportTexture)
	{
		const double width = static_cast<double>(viewportTexture.width);
		const double height = static_cast<double>(viewportTexture.height);
		MTLViewport metalViewport = { 0.0, 0.0, width, height, 0.0, 1.0 };
		[rc->m_commandEncoder setViewport:metalViewport];

		MTLScissorRect metalRect = { 0, 0, (NSUInteger)viewportTexture.width, (NSUInteger)viewportTexture.height };
		[rc->m_commandEncoder setScissorRect:metalRect];
	}

	[passDescriptor release];
}

void Gfx_EndPass(GfxContext* rc)
{
	[rc->m_commandEncoder endEncoding];
	[rc->m_commandEncoder release]; // TODO: when should this be released?
	rc->m_commandEncoder = nil;
}

void Gfx_ResolveImage(GfxContext* rc, GfxTextureArg src, GfxTextureArg dst)
{
	if (!src.valid() || !dst.valid())
	{
		return;
	}

	const TextureMTL& srcTexture = g_device->m_resources.textures[src];
	const TextureMTL& dstTexture = g_device->m_resources.textures[dst];

	if (srcTexture.desc.samples <= 1)
	{
		id<MTLBlitCommandEncoder> blit = [g_device->m_commandBuffer blitCommandEncoder];
		MTLOrigin origin = {0, 0, 0};
		MTLSize size = {srcTexture.desc.width, srcTexture.desc.height, 1};
		[blit copyFromTexture:srcTexture.native
			sourceSlice:0
			sourceLevel:0
			sourceOrigin:origin
			sourceSize:size
			toTexture:dstTexture.native
			destinationSlice:0
			destinationLevel:0
			destinationOrigin:origin];
		[blit endEncoding];
		return;
	}

	MTLRenderPassDescriptor* passDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	passDescriptor.colorAttachments[0].texture = srcTexture.native;
	passDescriptor.colorAttachments[0].resolveTexture = dstTexture.native;
	passDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
	passDescriptor.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;

	id<MTLRenderCommandEncoder> encoder = [g_device->m_commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
	[encoder endEncoding];
}


void Gfx_Clear(GfxContext* rc, ColorRGBA8 color, GfxClearFlags clearFlags, float depth, u32 stencil)
{
	Log::error("Not implemented");
}

void Gfx_SetViewport(GfxContext* rc, const GfxViewport& viewport)
{
	MTLViewport metalViewport = { viewport.x, viewport.y, viewport.w, viewport.h, viewport.depthMin, viewport.depthMax };
	[rc->m_commandEncoder setViewport:metalViewport];
}

void Gfx_SetScissorRect(GfxContext* rc, const GfxRect& rect)
{
	MTLScissorRect metalRect = { u32(rect.left), u32(rect.top), u32(rect.right-rect.left), u32(rect.bottom-rect.top) };
	[rc->m_commandEncoder setScissorRect:metalRect];
}

void Gfx_SetTechnique(GfxContext* rc, GfxTechniqueArg h)
{
	rc->m_pendingRayTracingPipeline.reset();
	rc->m_pendingTechnique.retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_Pipeline
					 | GfxContext::DirtyStateFlag_VertexBuffer
					 | GfxContext::DirtyStateFlag_Descriptors
					 | GfxContext::DirtyStateFlag_DescriptorSet;
}

void Gfx_SetPrimitive(GfxContext* rc, GfxPrimitive type)
{
	rc->m_primitiveType = convertPrimitiveType(type);
	rc->m_primitiveTopology = convertPrimitiveTopology(type);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_Technique;
}

void Gfx_SetIndexStream(GfxContext* rc, u32 offset, GfxFormat format, GfxBufferArg h)
{
	RUSH_ASSERT_MSG(offset==0, "Index buffer offset is not implemented");

	[rc->m_indexBuffer release];

	rc->m_indexType = g_device->m_resources.buffers[h].indexType;
	rc->m_indexStride = g_device->m_resources.buffers[h].stride;
	rc->m_indexBuffer = g_device->m_resources.buffers[h].native;
	rc->m_indexBufferOffset = offset;

	[rc->m_indexBuffer retain];
}

void Gfx_SetVertexStream(GfxContext* rc, u32 idx, u32 offset, u32 stride, GfxBufferArg h)
{
	// FIXME: binding only applies to active encoder; calls before BeginPass are dropped.
	[rc->m_commandEncoder setVertexBuffer:g_device->m_resources.buffers[h].native offset:offset atIndex:(GfxContext::MaxConstantBuffers+idx)];
}

void Gfx_SetStorageImage(GfxContext* rc, u32 idx, GfxTextureArg h)
{
	RUSH_ASSERT(idx < GfxContext::MaxStorageImages);

	rc->m_storageImages[idx].retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_StorageImage;
}

void Gfx_SetStorageBuffer(GfxContext* rc, u32 idx, GfxBufferArg h)
{
	RUSH_ASSERT(idx < GfxContext::MaxStorageBuffers);

	rc->m_storageBuffers[idx].retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_StorageBuffer;
}

void Gfx_SetAccelerationStructure(GfxContext* rc, u32 idx, GfxAccelerationStructureArg h)
{
	RUSH_ASSERT(idx < GfxContext::MaxAccelerationStructures);

	rc->m_accelerationStructures[idx].retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_AccelerationStructure;
}

void Gfx_SetTexture(GfxContext* rc, u32 idx, GfxTextureArg h)
{
	RUSH_ASSERT(idx < GfxContext::MaxSampledImages);

	rc->m_sampledImages[idx].retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_Texture;
}

void Gfx_SetSampler(GfxContext* rc, u32 idx, GfxSamplerArg h)
{
	RUSH_ASSERT(idx < GfxContext::MaxSamplers);

	rc->m_samplers[idx].retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_Sampler;
}

void Gfx_SetBlendState(GfxContext* rc, GfxBlendStateArg h)
{
	rc->m_pendingBlendState.retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_BlendState;
}

void Gfx_SetDepthStencilState(GfxContext* rc, GfxDepthStencilStateArg h)
{
	rc->m_pendingDepthStencilState.retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_DepthStencilState;
}

void Gfx_SetRasterizerState(GfxContext* rc, GfxRasterizerStateArg h)
{
	rc->m_pendingRasterizerState.retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_RasterizerState;
}

void Gfx_SetConstantBuffer(GfxContext* rc, u32 index, GfxBufferArg h, size_t offset)
{
	RUSH_ASSERT(index < GfxContext::MaxConstantBuffers);

	rc->m_constantBuffers[index].retain(h);
	rc->m_constantBufferOffsets[index] = offset;
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_ConstantBuffer;
}

void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ)
{
	RUSH_ASSERT_MSG(rc->m_commandEncoder == nil, "Can't execute compute inside graphics render pass!");

	rc->applyState();

	const auto& workGroupSize = g_device->m_resources.techniques[rc->m_pendingTechnique.get()].workGroupSize;

	[rc->m_computeCommandEncoder
		dispatchThreadgroups:MTLSizeMake(sizeX, sizeY, sizeZ)
		threadsPerThreadgroup:MTLSizeMake(workGroupSize.x, workGroupSize.y, workGroupSize.z)];
}

void Gfx_Dispatch(GfxContext* rc, u32 sizeX, u32 sizeY, u32 sizeZ, const void* pushConstants, u32 pushConstantsSize)
{
	RUSH_ASSERT_MSG(rc->m_commandEncoder == nil, "Can't execute compute inside graphics render pass!");

	rc->applyState();

	if (pushConstants)
	{
		RUSH_ASSERT(rc->m_pendingTechnique.valid());
		const auto& technique = g_device->m_resources.techniques[rc->m_pendingTechnique.get()];
		RUSH_ASSERT(technique.desc.bindings.pushConstantSize == pushConstantsSize);
		setPushConstants(rc, pushConstants, pushConstantsSize, technique.desc.bindings.pushConstantStageFlags,
			technique.descriptorSetCount);
	}

	const auto& workGroupSize = g_device->m_resources.techniques[rc->m_pendingTechnique.get()].workGroupSize;

	[rc->m_computeCommandEncoder
		dispatchThreadgroups:MTLSizeMake(sizeX, sizeY, sizeZ)
		threadsPerThreadgroup:MTLSizeMake(workGroupSize.x, workGroupSize.y, workGroupSize.z)];
}

void Gfx_Draw(GfxContext* rc, u32 firstVertex, u32 vertexCount)
{
	rc->applyState();
	[rc->m_commandEncoder
		drawPrimitives:rc->m_primitiveType
		vertexStart:firstVertex
		vertexCount:vertexCount];

	g_device->m_stats.vertices += vertexCount;
	g_device->m_stats.drawCalls++;
}

void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount)
{
	RUSH_ASSERT(rc->m_indexBuffer);

	rc->applyState();
	[rc->m_commandEncoder
	 drawIndexedPrimitives:rc->m_primitiveType
	 indexCount:indexCount
	 indexType:rc->m_indexType
	 indexBuffer:rc->m_indexBuffer
	 indexBufferOffset:firstIndex * rc->m_indexStride + rc->m_indexBufferOffset
	 instanceCount:1
	 baseVertex:baseVertex
	 baseInstance:0];

	g_device->m_stats.vertices += indexCount;
	g_device->m_stats.drawCalls++;
}

void Gfx_DrawIndexed(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
					 const void* pushConstants, u32 pushConstantsSize)
{
	RUSH_ASSERT(rc->m_indexBuffer);

	rc->applyState();

	if (pushConstants)
	{
		RUSH_ASSERT(rc->m_pendingTechnique.valid());
		const auto& technique = g_device->m_resources.techniques[rc->m_pendingTechnique.get()];
		RUSH_ASSERT(technique.desc.bindings.pushConstantSize == pushConstantsSize);
		setPushConstants(rc, pushConstants, pushConstantsSize, technique.desc.bindings.pushConstantStageFlags,
			technique.descriptorSetCount);
	}

	[rc->m_commandEncoder
	 drawIndexedPrimitives:rc->m_primitiveType
	 indexCount:indexCount
	 indexType:rc->m_indexType
	 indexBuffer:rc->m_indexBuffer
	 indexBufferOffset:firstIndex * rc->m_indexStride + rc->m_indexBufferOffset
	 instanceCount:1
	 baseVertex:baseVertex
	 baseInstance:0];

	g_device->m_stats.vertices += indexCount;
	g_device->m_stats.drawCalls++;
}

void Gfx_DrawIndexedInstanced(GfxContext* rc, u32 indexCount, u32 firstIndex, u32 baseVertex, u32 vertexCount,
							  u32 instanceCount, u32 instanceOffset)
{
	RUSH_ASSERT(rc->m_indexBuffer);

	rc->applyState();
	[rc->m_commandEncoder
	 drawIndexedPrimitives:rc->m_primitiveType
	 indexCount:indexCount
	 indexType:rc->m_indexType
	 indexBuffer:rc->m_indexBuffer
	 indexBufferOffset:firstIndex * rc->m_indexStride + rc->m_indexBufferOffset
	 instanceCount:instanceCount
	 baseVertex:baseVertex
	 baseInstance:instanceOffset];

	g_device->m_stats.vertices += indexCount * instanceCount;
	g_device->m_stats.drawCalls++;
}

void Gfx_DrawIndexedIndirect(GfxContext* rc, GfxBufferArg argsBuffer, size_t argsBufferOffset, u32 drawCount)
{
	RUSH_ASSERT(rc->m_indexBuffer);

	BufferMTL& buf = g_device->m_resources.buffers[argsBuffer];
	rc->applyState();

	// TODO: perhaps could use indirect command buffers to emulate multi-draw-indirect
	for (u32 i=0; i<drawCount; ++i)
	{
		[rc->m_commandEncoder
		 drawIndexedPrimitives:rc->m_primitiveType
		 indexType:rc->m_indexType
		 indexBuffer:rc->m_indexBuffer
		 indexBufferOffset:0
		 indirectBuffer:buf.native
		 indirectBufferOffset:argsBufferOffset + sizeof(GfxDrawIndexedArg) * i];
	}

	g_device->m_stats.drawCalls++;
}

void Gfx_PushMarker(GfxContext* rc, const char* marker)
{
	NSString* str = @(marker);

	if (rc->m_computeCommandEncoder)
	{
		[rc->m_computeCommandEncoder pushDebugGroup:str];
	}
	else
	{
		[rc->m_commandEncoder pushDebugGroup:str];
	}
}

void Gfx_PopMarker(GfxContext* rc)
{
	if (rc->m_computeCommandEncoder)
	{
		[rc->m_computeCommandEncoder popDebugGroup];
	}
	else
	{
		[rc->m_commandEncoder popDebugGroup];
	}
}

void Gfx_BeginTimer(GfxContext* rc, u32 timestampId)
{
	// TODO
}

void Gfx_EndTimer(GfxContext* rc, u32 timestampId)
{
	// TODO
}

void Gfx_Retain(GfxDevice* dev)
{
	dev->addReference();
}

void Gfx_Retain(GfxContext* rc)
{
	rc->addReference();
}

void Gfx_Retain(GfxVertexFormat h)
{
	g_device->m_resources.vertexFormats[h].addReference();
}

void Gfx_Retain(GfxVertexShader h)
{
	g_device->m_resources.shaders[h].addReference();
}

void Gfx_Retain(GfxPixelShader h)
{
	g_device->m_resources.shaders[h].addReference();
}

void Gfx_Retain(GfxComputeShader h)
{
	g_device->m_resources.shaders[h].addReference();
}

void Gfx_Retain(GfxTechnique h)
{
	g_device->m_resources.techniques[h].addReference();
}

void Gfx_Retain(GfxTexture h)
{
	g_device->m_resources.textures[h].addReference();
}

void Gfx_Retain(GfxBlendState h)
{
	g_device->m_resources.blendStates[h].addReference();
}

void Gfx_Retain(GfxSampler h)
{
	g_device->m_resources.samplers[h].addReference();
}

void Gfx_Retain(GfxDepthStencilState h)
{
	g_device->m_resources.depthStencilStates[h].addReference();
}

void Gfx_Retain(GfxRasterizerState h)
{
	g_device->m_resources.rasterizerStates[h].addReference();
}

void Gfx_Retain(GfxBuffer h)
{
	g_device->m_resources.buffers[h].addReference();
}

// Descriptor sets

static DescriptorSetMTL createDescriptorSet(const GfxDescriptorSetDesc& desc)
{
	DescriptorSetMTL res;

	res.uniqueId = g_device->generateId();
	res.desc = desc;

	u32 argumentIndex = 0;
	const bool isTextureArray = !!(desc.flags & GfxDescriptorSetFlags::TextureArray);

	NSMutableArray<MTLArgumentDescriptor *>* descriptors = [NSMutableArray<MTLArgumentDescriptor *> new];
	for(u32 i=0; i<desc.constantBuffers; ++i)
	{
		MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
		[descriptor setDataType: MTLDataTypePointer];
		[descriptor setIndex: argumentIndex];
		[descriptor setAccess: MTLBindingAccessReadOnly];
		[descriptors addObject: descriptor];
		++argumentIndex;
	}
	res.constantBuffers.resize(desc.constantBuffers);
	res.constantBufferOffsets.resize(desc.constantBuffers);

	for(u32 i=0; i<desc.samplers; ++i)
	{
		MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
		[descriptor setDataType: MTLDataTypeSampler];
		[descriptor setIndex: argumentIndex];
		[descriptor setAccess: MTLBindingAccessReadOnly];
		[descriptors addObject: descriptor];
		++argumentIndex;
	}
	res.samplers.resize(desc.samplers);

	if (isTextureArray && desc.textures)
	{
		MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
		[descriptor setDataType: MTLDataTypeTexture];
		[descriptor setIndex: argumentIndex];
		[descriptor setAccess: MTLBindingAccessReadOnly];
		[descriptor setTextureType:MTLTextureType2D]; // TODO: support other texture types
		[descriptor setArrayLength: desc.textures];
		[descriptors addObject: descriptor];
		argumentIndex += desc.textures;
	}
	else
	{
		for(u32 i=0; i<desc.textures; ++i)
		{
			MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
			[descriptor setDataType: MTLDataTypeTexture];
			[descriptor setIndex: argumentIndex];
			[descriptor setAccess: MTLBindingAccessReadOnly];
			[descriptor setTextureType:MTLTextureType2D]; // TODO: support other texture types
			[descriptors addObject: descriptor];
			++argumentIndex;
		}
	}
	res.textures.resize(desc.textures);

	for(u32 i=0; i<desc.rwImages; ++i)
	{
		MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
		[descriptor setDataType: MTLDataTypeTexture];
		[descriptor setIndex: argumentIndex];
		[descriptor setAccess: MTLBindingAccessReadWrite];
		[descriptor setTextureType:MTLTextureType2D]; // TODO: support other texture types
		[descriptors addObject: descriptor];
		++argumentIndex;
	}
	res.storageImages.resize(desc.rwImages);

	for(u32 i=0; i<desc.rwBuffers; ++i)
	{
		MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
		[descriptor setDataType: MTLDataTypePointer];
		[descriptor setAccess: MTLBindingAccessReadWrite];
		[descriptor setIndex: argumentIndex];
		[descriptors addObject: descriptor];
		++argumentIndex;
	}

	// FIXME: rwTypedBuffers should index storageBuffers with rwBuffers offset.
	for(u32 i=0; i<desc.rwTypedBuffers; ++i)
	{
		MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
		[descriptor setDataType: MTLDataTypePointer];
		[descriptor setAccess: MTLBindingAccessReadWrite];
		[descriptor setIndex: argumentIndex];
		[descriptors addObject: descriptor];
		++argumentIndex;
	}

	res.storageBuffers.resize(desc.rwBuffers + desc.rwTypedBuffers);

	for(u32 i=0; i<desc.accelerationStructures; ++i)
	{
		MTLArgumentDescriptor* descriptor = [MTLArgumentDescriptor new];
		[descriptor setDataType: MTLDataTypeInstanceAccelerationStructure];
		[descriptor setAccess: MTLBindingAccessReadOnly];
		[descriptor setIndex: argumentIndex];
		[descriptors addObject: descriptor];
		++argumentIndex;
	}
	res.accelerationStructures.resize(desc.accelerationStructures);

	res.encoder = [g_metalDevice newArgumentEncoderWithArguments:descriptors];
	res.argBufferSize = [res.encoder encodedLength];

	for(id obj in descriptors)
	{
		[obj release];
	}
	[descriptors release];

	return res;
}

GfxOwn<GfxDescriptorSet> Gfx_CreateDescriptorSet(const GfxDescriptorSetDesc& desc)
{
	return GfxDevice::makeOwn(
	  retainResource(g_device->m_resources.descriptorSets,
					 createDescriptorSet(desc)));
}

void Gfx_Retain(GfxDescriptorSet h)
{
	g_device->m_resources.descriptorSets[h].addReference();
}

void Gfx_Release(GfxDescriptorSet h)
{
	releaseResource(g_device->m_resources.descriptorSets, h);
}

void Gfx_SetDescriptors(GfxContext* rc, u32 index, GfxDescriptorSetArg h)
{
	rc->m_descriptorSets[index].retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_DescriptorSet;
}

static void updateDescriptorSet(DescriptorSetMTL& ds,
	 const GfxBuffer* constantBuffers,
	 const u64* constantBufferOffsets,
	 const GfxSampler* samplers,
	 const GfxTexture* textures,
	 const GfxTexture* storageImages,
	 const GfxBuffer* storageBuffers,
     const GfxAccelerationStructure* accelStructures)
{
	const GfxDescriptorSetDesc& desc = ds.desc;

	if (!ds.argBuffer || [ds.argBuffer length] < ds.argBufferSize)
	{
		[ds.argBuffer release];
		ds.argBuffer = [g_metalDevice newBufferWithLength:ds.argBufferSize options:0];
	}

	[ds.encoder setArgumentBuffer:ds.argBuffer offset:0];

	u32 idxOffset = 0;
	const bool isTextureArray = !!(desc.flags & GfxDescriptorSetFlags::TextureArray);

	for(u32 i=0; i<desc.constantBuffers; ++i)
	{
		BufferMTL& buf = g_device->m_resources.buffers[constantBuffers[i]];
		u64 offset = constantBufferOffsets ? constantBufferOffsets[i] : 0;
		ds.constantBufferOffsets[i] = offset;
		ds.constantBuffers[i] = constantBuffers[i];
		[ds.encoder setBuffer:buf.native offset:offset atIndex:idxOffset+i];
	}
	idxOffset += desc.constantBuffers;

	for(u32 i=0; i<desc.samplers; ++i)
	{
		SamplerMTL& smp = g_device->m_resources.samplers[samplers[i]];
		ds.samplers[i] = samplers[i];
		[ds.encoder setSamplerState:smp.native atIndex:idxOffset + i];
	}
	idxOffset += desc.samplers;

	if (isTextureArray && desc.textures)
	{
		for(u32 i=0; i<desc.textures; ++i)
		{
			TextureMTL& tex = g_device->m_resources.textures[textures[i]];
			//RUSH_ASSERT(tex.desc.type == TextureType::Tex2D); // only 2D textures are currently supported
			ds.textures[i] = textures[i];
			[ds.encoder setTexture:tex.native atIndex:idxOffset + i];
		}
		idxOffset += desc.textures;
	}
	else
	{
		for(u32 i=0; i<desc.textures; ++i)
		{
			TextureMTL& tex = g_device->m_resources.textures[textures[i]];
			//RUSH_ASSERT(tex.desc.type == TextureType::Tex2D); // only 2D textures are currently supported
			ds.textures[i] = textures[i];
			[ds.encoder setTexture:tex.native atIndex:idxOffset+i];
		}
		idxOffset += desc.textures;
	}

	for(u32 i=0; i<desc.rwImages; ++i)
	{
		TextureMTL& tex = g_device->m_resources.textures[storageImages[i]];
		//RUSH_ASSERT(tex.desc.type == TextureType::Tex2D); // only 2D textures are currently supported
		ds.storageImages[i] = storageImages[i];
		[ds.encoder setTexture:tex.native atIndex:idxOffset+i];
	}
	idxOffset += desc.rwImages;

	for(u32 i=0; i<desc.rwBuffers; ++i)
	{
		BufferMTL& buf = g_device->m_resources.buffers[storageBuffers[i]];
		ds.storageBuffers[i] = storageBuffers[i];
		[ds.encoder setBuffer:buf.native offset:0 atIndex:idxOffset+i];
	}
	idxOffset += desc.rwBuffers;

	for(u32 i=0; i<desc.rwTypedBuffers; ++i)
	{
		BufferMTL& buf = g_device->m_resources.buffers[storageBuffers[i]];
		ds.storageBuffers[i] = storageBuffers[i];
		[ds.encoder setBuffer:buf.native offset:0 atIndex:idxOffset+i];
	}
	idxOffset += desc.rwTypedBuffers;

	for(u32 i=0; i<desc.accelerationStructures; ++i)
	{
		RUSH_ASSERT(accelStructures);
		AccelerationStructureMTL& accel = g_device->m_resources.accelerationStructures[accelStructures[i]];
		ds.accelerationStructures[i] = accelStructures[i];
		[ds.encoder setAccelerationStructure:accel.native atIndex:idxOffset+i];
	}
	idxOffset += desc.accelerationStructures;
}

void Gfx_UpdateDescriptorSet(GfxDescriptorSetArg d,
	 const GfxBuffer* constantBuffers,
	 const GfxSampler* samplers,
	 const GfxTexture* textures,
	 const GfxTexture* storageImages,
	 const GfxBuffer* storageBuffers,
	 const GfxAccelerationStructure* accelStructures)
{
	DescriptorSetMTL& ds = g_device->m_resources.descriptorSets[d];
	updateDescriptorSet(ds, constantBuffers, 0, samplers, textures, storageImages, storageBuffers, accelStructures);
}

void DescriptorSetMTL::destroy()
{
	[encoder release];
	[argBuffer release];
}

}

#else // RUSH_RENDER_API==RUSH_RENDER_API_MTL
char _GfxDeviceMtl_mm; // suppress linker warning
#endif // RUSH_RENDER_API==RUSH_RENDER_API_MTL
