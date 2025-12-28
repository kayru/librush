#include "GfxDeviceMtl.h"
#include "UtilLog.h"
#include "Window.h"
#include "Platform.h"
#include "UtilFile.h"

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

template <typename ObjectType, typename HandleType, typename ObjectTypeDeduced>
HandleType retainResource(
	ResourcePool<ObjectType, HandleType>& pool,
	ObjectTypeDeduced&& object)
{
	RUSH_ASSERT(object.uniqueId != 0);
	auto handle = pool.push(std::forward<ObjectType>(object));
	Gfx_Retain(handle);
	Gfx_Retain(g_device);
	return handle;
}

template <typename ObjectType, typename HandleType>
void releaseResource(
	ResourcePool<ObjectType, HandleType>& pool,
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
	m_caps.deviceNearDepth = 0.0f;
	m_caps.deviceFarDepth = 1.0f;
	m_caps.compute = true;
	m_caps.debugOutput = false;
	m_caps.debugMarkers = true;
	m_caps.constantBufferAlignment = 256;
	m_caps.pushConstants = false;
	m_caps.instancing = true;
	m_caps.drawIndirect = true;

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
	[g_device->m_commandBuffer commit];
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

void Gfx_Finish()
{
	Log::error("Not implemented");
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
	RUSH_ASSERT(code.type == GfxShaderSourceType_MSL);
	const char* entryName = code.entry;
	if (!strcmp(entryName, "main"))
	{
		entryName = "main0";
	}

	const char* sourceText = code.data();
	RUSH_ASSERT(sourceText);

	ShaderMTL result;
	result.uniqueId = g_device->generateId();

	NSError* error = nullptr;
	result.library = [g_metalDevice newLibraryWithSource:@(sourceText) options:nil error:&error];
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
	return GfxDevice::makeOwn(retainResource(g_device->m_resources.computeShaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxComputeShader h)
{
	releaseResource(g_device->m_resources.computeShaders, h);
}

// vertex shader

GfxOwn<GfxVertexShader> Gfx_CreateVertexShader(const GfxShaderSource& code)
{
	return GfxDevice::makeOwn(retainResource(g_device->m_resources.vertexShaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxVertexShader h)
{
	releaseResource(g_device->m_resources.vertexShaders, h);
}

// pixel shader
GfxOwn<GfxPixelShader> Gfx_CreatePixelShader(const GfxShaderSource& code)
{
	return GfxDevice::makeOwn(retainResource(g_device->m_resources.pixelShaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxPixelShader h)
{
	releaseResource(g_device->m_resources.pixelShaders, h);
}

// technique

void TechniqueMTL::destroy()
{
	defaultDescriptorSet.destroy();
	vs.reset();
	ps.reset();
	[computePipeline release];
}

GfxOwn<GfxTechnique> Gfx_CreateTechnique(const GfxTechniqueDesc& desc)
{
	RUSH_ASSERT(desc.vs.valid() || desc.cs.valid());

	TechniqueMTL result;

	result.uniqueId = g_device->generateId();
	result.desc = desc;

	if (desc.cs.valid())
	{
		id <MTLFunction> computeShader = g_device->m_resources.computeShaders[desc.cs].function;
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

void Gfx_Release(GfxTechnique h)
{
	releaseResource(g_device->m_resources.techniques, h);
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
		case GfxFormat_BGRA8_Unorm:
			return MTLPixelFormatBGRA8Unorm;
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
	textureDescriptor.mipmapLevelCount = desc.mips;
	textureDescriptor.sampleCount = 1; // TODO
	textureDescriptor.arrayLength = desc.isArray() ? desc.depth : 1;
	textureDescriptor.textureType = isCube ? MTLTextureTypeCube : MTLTextureType2D;

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

GfxOwn<GfxBuffer> Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data)
{
	const u32 bufferSize = desc.count * desc.stride;

	BufferMTL res;
	res.uniqueId = g_device->generateId();
	if (data)
	{
		res.native = [g_metalDevice newBufferWithBytes:data length:bufferSize options:0];
	}
	else
	{
		res.native = [g_metalDevice newBufferWithLength:bufferSize options:0];
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

GfxMappedBuffer Gfx_MapBuffer(GfxBuffer vb, u32 offset, u32 size)
{
	Log::error("Not implemented");
	return GfxMappedBuffer();
}

void Gfx_UnmapBuffer(GfxMappedBuffer& lock)
{
	Log::error("Not implemented");
}

void Gfx_UpdateBuffer(GfxContext* rc, GfxBufferArg h, const void* data, u32 size)
{
	if (!h.valid())
	{
		return;
	}

	BufferMTL& buffer = g_device->m_resources.buffers[h];

	[buffer.native release];
	buffer.native = [g_metalDevice newBufferWithBytes:data length:size options:0];

	// TODO: re-bind buffer if necessary
}

void* Gfx_BeginUpdateBuffer(GfxContext* rc, GfxBufferArg h, u32 size)
{
	Log::error("Not implemented");
	return nullptr;
}

void Gfx_EndUpdateBuffer(GfxContext* rc, GfxBufferArg h)
{
	Log::error("Not implemented");
}

void Gfx_Release(GfxBuffer h)
{
	releaseResource(g_device->m_resources.buffers, h);
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
		 usage:MTLResourceUsageWrite];
	}

	for (u64 j=0; j<ds.storageBuffers.size(); ++j)
	{
		[commandEncoder
		 useResource:g_device->m_resources.buffers[ds.storageBuffers[j]].native
		 usage:MTLResourceUsageWrite];
	}
}

void GfxContext::applyState()
{
	// TODO: cache pipelines

	TechniqueMTL& technique = g_device->m_resources.techniques[m_pendingTechnique.get()];

	if ((m_dirtyState & DirtyStateFlag_Pipeline) && technique.computePipeline)
	{
		if (!m_computeCommandEncoder)
		{
			m_computeCommandEncoder = [g_device->m_commandBuffer computeCommandEncoder];
			[m_computeCommandEncoder retain];
		}

		[m_computeCommandEncoder setComputePipelineState:technique.computePipeline];
	}
	else if (m_dirtyState & DirtyStateFlag_Pipeline)
	{
		MTLRenderPipelineDescriptor* pipelineDescriptor = [MTLRenderPipelineDescriptor new];

		const auto& vertexFormat = g_device->m_resources.vertexFormats[technique.vf.get()];
		const auto& vertexShader = g_device->m_resources.vertexShaders[technique.vs.get()];
		const auto& pixelShader = g_device->m_resources.pixelShaders[technique.ps.get()];

		pipelineDescriptor.inputPrimitiveTopology = m_primitiveTopology;
		pipelineDescriptor.vertexDescriptor = vertexFormat.native;
		pipelineDescriptor.vertexFunction = vertexShader.function;
		pipelineDescriptor.fragmentFunction = pixelShader.function;

		// TODO: color-only rendering
		pipelineDescriptor.depthAttachmentPixelFormat = [g_device->m_resources.textures[g_device->m_defaultDepthBuffer.get()].native pixelFormat];

		const auto& blendState = g_device->m_resources.blendStates[m_pendingBlendState.get()].desc;

		const bool useBackBuffer = !m_passDesc.color[0].valid() && !m_passDesc.depth.valid();

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

	if (m_dirtyState & DirtyStateFlag_Descriptors)
	{
		GfxBuffer constantBuffers[RUSH_COUNTOF(m_constantBuffers)];
		GfxSampler samplers[RUSH_COUNTOF(m_samplers)];
		GfxTexture sampledImages[RUSH_COUNTOF(m_sampledImages)];
		GfxTexture storageImages[RUSH_COUNTOF(m_storageImages)];
		GfxBuffer storageBuffers[RUSH_COUNTOF(m_storageBuffers)];

		const auto& dsetDesc = technique.desc.bindings.descriptorSets[0];
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
		for(u32 i=0; i<dsetDesc.rwTypedBuffers; ++i)
		{
			//TODO: bind typed storage buffers
		}

		updateDescriptorSet(technique.defaultDescriptorSet,
							constantBuffers,
							m_constantBufferOffsets,
							samplers,
							sampledImages,
							storageImages,
							storageBuffers,
							nullptr);

		auto& ds = technique.defaultDescriptorSet;

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
			u32 firstDescriptorSet = technique.desc.bindings.useDefaultDescriptorSet ? 1 : 0;
			for (u32 i=firstDescriptorSet; i<technique.descriptorSetCount; ++i)
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
			u32 firstDescriptorSet = technique.desc.bindings.useDefaultDescriptorSet ? 1 : 0;
			for (u32 i=firstDescriptorSet; i<technique.descriptorSetCount; ++i)
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

	GfxTexture depthBuffer = desc.depth.valid() ? desc.depth : g_device->m_defaultDepthBuffer.get();
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

	RUSH_ASSERT(g_device->m_commandBuffer);
	rc->m_commandEncoder = [g_device->m_commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
	[rc->m_commandEncoder retain];

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
	Log::error("Not implemented");
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
	Log::fatal("Gfx_DrawIndexed with push constants is not implemented");
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
	g_device->m_resources.vertexShaders[h].addReference();
}

void Gfx_Retain(GfxPixelShader h)
{
	g_device->m_resources.pixelShaders[h].addReference();
}

void Gfx_Retain(GfxComputeShader h)
{
	g_device->m_resources.computeShaders[h].addReference();
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

	[ds.argBuffer release];

	ds.argBuffer = [g_metalDevice newBufferWithLength:ds.argBufferSize options:0];

	[ds.encoder setArgumentBuffer:ds.argBuffer offset:0];

	u32 idxOffset = 0;

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

	for(u32 i=0; i<desc.textures; ++i)
	{
		TextureMTL& tex = g_device->m_resources.textures[textures[i]];
		//RUSH_ASSERT(tex.desc.type == TextureType::Tex2D); // only 2D textures are currently supported
		ds.textures[i] = textures[i];
		[ds.encoder setTexture:tex.native atIndex:idxOffset+i];
	}
	idxOffset += desc.textures;

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
