#include "GfxDeviceMtl.h"
#include "UtilLog.h"
#include "Window.h"
#include "Platform.h"
#include "UtilFile.h"

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL

namespace Rush
{

static GfxDevice* g_device = nullptr;
static GfxContext* g_context = nullptr;
static id<MTLDevice> g_metalDevice = nil;

template <typename ObjectType, typename HandleType>
HandleType retainResource(
	ResourcePool<ObjectType, HandleType>& pool,
	const ObjectType& object)
{
	RUSH_ASSERT(object.uniqueId != 0);
	auto handle = pool.push(object);
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

GfxDevice::GfxDevice(Window* window, const GfxConfig& cfg)
{
	g_device = this;
	m_refs = 1;

	m_metalDevice = MTLCreateSystemDefaultDevice();
	m_commandQueue = [m_metalDevice newCommandQueue];

	g_metalDevice = m_metalDevice;

	NSWindow* nsWindow = (NSWindow*)window->nativeHandle();
	[nsWindow.contentView setWantsLayer:YES];
	m_metalLayer = [CAMetalLayer layer];
	[nsWindow.contentView setLayer:m_metalLayer];

	m_metalLayer.device = m_metalDevice;
	m_metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;

	// default resources

	m_blendStates[InvalidResourceHandle()].desc = GfxBlendStateDesc::makeOpaque();

	GfxTextureDesc defaultDepthBufferDesc;
	defaultDepthBufferDesc.width = cfg.backBufferWidth;
	defaultDepthBufferDesc.height = cfg.backBufferHeight;
	defaultDepthBufferDesc.depth = 1;
	defaultDepthBufferDesc.mips = 1;
	defaultDepthBufferDesc.format = GfxFormat_D32_Float;
	defaultDepthBufferDesc.usage = GfxUsageFlags::DepthStencil;
	m_defaultDepthBuffer.retain(Gfx_CreateTexture(defaultDepthBufferDesc));

	// init caps

	m_caps.looseConstants = false;
	m_caps.constantBuffers = true;

	m_caps.deviceTopLeft = Vec2(-1.0f, 1.0f);
	m_caps.textureTopLeft = Vec2(0.0f, 0.0f);
	m_caps.shaderTypeMask |= 1 << GfxShaderSourceType_MSL;
	m_caps.projectionFlags = ProjectionFlags::Default;
	m_caps.deviceNearDepth = 0.0f;
	m_caps.deviceFarDepth = 1.0f;
	m_caps.compute = true;
	m_caps.debugOutput = false;
	m_caps.debugMarkers = true;
	m_caps.interopOpenCL = false;

	m_caps.apiName = "Metal";
}

GfxDevice::~GfxDevice()
{
	g_metalDevice = nil;

	[m_commandBuffer release];
	[m_commandQueue release];
	[m_metalDevice release];
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

void Gfx_BeginFrame()
{
	g_device->m_drawable = [g_device->m_metalLayer nextDrawable];
	[g_device->m_drawable retain];

	g_device->m_backBufferTexture = [g_device->m_drawable texture];
	[g_device->m_backBufferTexture retain];

	g_device->m_backBufferPixelFormat = [g_device->m_metalLayer pixelFormat];

	g_device->m_commandBuffer = [g_device->m_commandQueue commandBuffer];
	[g_device->m_commandBuffer retain];
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
	if (interval != 1)
	{
		Log::error("Not implemented");
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

	return GfxDevice::makeOwn(retainResource(g_device->m_vertexFormats, format));
}

void Gfx_Release(GfxVertexFormat h)
{
	releaseResource(g_device->m_vertexFormats, h);
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
	return GfxDevice::makeOwn(retainResource(g_device->m_computeShaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxComputeShader h)
{
	releaseResource(g_device->m_computeShaders, h);
}

// vertex shader

GfxOwn<GfxVertexShader> Gfx_CreateVertexShader(const GfxShaderSource& code)
{
	return GfxDevice::makeOwn(retainResource(g_device->m_vertexShaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxVertexShader h)
{
	releaseResource(g_device->m_vertexShaders, h);
}

// pixel shader
GfxOwn<GfxPixelShader> Gfx_CreatePixelShader(const GfxShaderSource& code)
{
	return GfxDevice::makeOwn(retainResource(g_device->m_pixelShaders, ShaderMTL::create(code)));
}

void Gfx_Release(GfxPixelShader h)
{
	releaseResource(g_device->m_pixelShaders, h);
}

// technique

void TechniqueMTL::destroy()
{
	vs.reset();
	ps.reset();
	[computePipeline release];
}

GfxOwn<GfxTechnique> Gfx_CreateTechnique(const GfxTechniqueDesc& desc)
{
	RUSH_ASSERT(desc.vs.valid() || desc.cs.valid());

	TechniqueMTL result;

	result.uniqueId = g_device->generateId();

	if (desc.cs.valid())
	{
		id <MTLFunction> computeShader = g_device->m_computeShaders[desc.cs].function;
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

	u32 offset = 0;

	result.constantBufferOffset = 0;
	offset += desc.bindings.constantBuffers;

	result.samplerOffset = offset;
	offset += desc.bindings.samplers;

	result.sampledImageOffset = offset;
	offset += desc.bindings.textures;

	result.storageImageOffset = offset;
	offset += desc.bindings.rwImages;

	result.storageBufferOffset = offset;
	offset += desc.bindings.rwBuffers;

	return GfxDevice::makeOwn(retainResource(g_device->m_techniques, result));
}

void Gfx_Release(GfxTechnique h)
{
	releaseResource(g_device->m_techniques, h);
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
	return GfxDevice::makeOwn(retainResource(g_device->m_textures, TextureMTL::create(desc, data, count, pixels)));
}

const GfxTextureDesc& Gfx_GetTextureDesc(GfxTextureArg h)
{
	if (h.valid())
	{
		return g_device->m_textures[h].desc;
	}
	else
	{
		static const GfxTextureDesc desc = GfxTextureDesc::make2D(1, 1, GfxFormat_Unknown);
		return desc;
	}

}

void Gfx_Release(GfxTexture h)
{
	releaseResource(g_device->m_textures, h);
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
	return GfxDevice::makeOwn(retainResource(g_device->m_blendStates, result));
}

void Gfx_Release(GfxBlendState h)
{
	releaseResource(g_device->m_blendStates, h);
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
	
	samplerDescriptor.sAddressMode = convertSamplerAddressMode(desc.wrapU);
	samplerDescriptor.tAddressMode = convertSamplerAddressMode(desc.wrapV);
	samplerDescriptor.rAddressMode = convertSamplerAddressMode(desc.wrapW);

	samplerDescriptor.minFilter = convertFilter(desc.filterMin);
	samplerDescriptor.magFilter = convertFilter(desc.filterMag);
	samplerDescriptor.mipFilter = convertMipFilter(desc.filterMip);

	samplerDescriptor.maxAnisotropy = (int)desc.anisotropy;

	SamplerMTL result;
	
	result.uniqueId = g_device->generateId();
	result.native = [g_metalDevice newSamplerStateWithDescriptor:samplerDescriptor];

	[samplerDescriptor release];

	return GfxDevice::makeOwn(retainResource(g_device->m_samplers, result));
}

void Gfx_Release(GfxSampler h)
{
	releaseResource(g_device->m_samplers, h);
}

// depth stencil state

void DepthStencilStateMTL::destroy()
{
	[native release];
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

GfxOwn<GfxDepthStencilState> Gfx_CreateDepthStencilState(const GfxDepthStencilDesc& desc)
{
	DepthStencilStateMTL result;
	result.uniqueId = g_device->generateId();

	MTLDepthStencilDescriptor* descriptor = [MTLDepthStencilDescriptor new];

	descriptor.depthCompareFunction = desc.enable ? convertCompareFunc(desc.compareFunc) : MTLCompareFunctionAlways;
	descriptor.depthWriteEnabled = desc.enable ? desc.writeEnable : false;

	result.native = [g_metalDevice newDepthStencilStateWithDescriptor:descriptor];

	[descriptor release];

	return GfxDevice::makeOwn(retainResource(g_device->m_depthStencilStates, result));
}

void Gfx_Release(GfxDepthStencilState h)
{
	releaseResource(g_device->m_depthStencilStates, h);
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
	return GfxDevice::makeOwn(retainResource(g_device->m_rasterizerStates, result));
}

void Gfx_Release(GfxRasterizerState h)
{
	releaseResource(g_device->m_rasterizerStates, h);
}

// buffers

void BufferMTL::destroy()
{
	[native release];
}

GfxOwn<GfxBuffer> Gfx_CreateBuffer(const GfxBufferDesc& desc, const void* data)
{
	const u32 bufferSize = alignCeiling(desc.count * desc.stride, 16);

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

	return GfxDevice::makeOwn(retainResource(g_device->m_buffers, res));
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

	BufferMTL& buffer = g_device->m_buffers[h];

	const u32 bufferSize = alignCeiling(size, 16);

	[buffer.native release]; // TODO: queue release maybe?
	buffer.native = [g_metalDevice newBufferWithBytes:data length:bufferSize options:0];

	// TODO: re-bind buffer if necessary
}

void Gfx_Release(GfxBuffer h)
{
	releaseResource(g_device->m_buffers, h);
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


void GfxContext::applyState()
{
	// TODO: cache pipelines

	const auto& technique = g_device->m_techniques[m_pendingTechnique.get()];

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

		const auto& vertexFormat = g_device->m_vertexFormats[technique.vf.get()];
		const auto& vertexShader = g_device->m_vertexShaders[technique.vs.get()];
		const auto& pixelShader = g_device->m_pixelShaders[technique.ps.get()];

		pipelineDescriptor.inputPrimitiveTopology = m_primitiveTopology;
		pipelineDescriptor.vertexDescriptor = vertexFormat.native;
		pipelineDescriptor.vertexFunction = vertexShader.function;
		pipelineDescriptor.fragmentFunction = pixelShader.function;

		// TODO: color-only rendering
		pipelineDescriptor.depthAttachmentPixelFormat = [g_device->m_textures[g_device->m_defaultDepthBuffer.get()].native pixelFormat];

		const auto& blendState = g_device->m_blendStates[m_pendingBlendState.get()].desc;

		const bool useBackBuffer = !m_passDesc.color[0].valid() && !m_passDesc.depth.valid();

		for (u32 i = 0; i < GfxPassDesc::MaxTargets; ++i)
		{
			if ((!useBackBuffer || i!=0) && !m_passDesc.color[i].valid())
			{
				break;
			}

			auto colorTarget = m_passDesc.color[i].valid() ? g_device->m_textures[m_passDesc.color[i]].native : g_device->m_backBufferTexture;

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
			const auto& state = g_device->m_depthStencilStates[m_pendingDepthStencilState.get()];
			[m_commandEncoder setDepthStencilState:state.native];
		}
		else
		{
			[m_commandEncoder setDepthStencilState:m_depthStencilState];
		}

		if (m_pendingRasterizerState.valid())
		{
			const auto& desc = g_device->m_rasterizerStates[m_pendingRasterizerState.get()].desc;
			MTLCullMode cullMode =  desc.cullMode == GfxCullMode::None ? MTLCullModeNone : MTLCullModeBack;
			[m_commandEncoder setCullMode:cullMode];
			[m_commandEncoder setFrontFacingWinding:desc.cullMode == GfxCullMode::CCW ? MTLWindingCounterClockwise : MTLWindingClockwise];
			[m_commandEncoder setTriangleFillMode:desc.fillMode == GfxFillMode::Solid ? MTLTriangleFillModeFill : MTLTriangleFillModeLines];
			[m_commandEncoder setDepthBias:desc.depthBias slopeScale:desc.depthBiasSlopeScale clamp:1.0f];
		}

		[pipelineState release];
		[pipelineDescriptor release];
	}

	if (m_commandEncoder)
	{
		if (m_dirtyState & DirtyStateFlag_ConstantBuffer)
		{
			for (u32 i=0; i<GfxContext::MaxConstantBuffers; ++i)
			{
				u32 slot = technique.constantBufferOffset + i;
				id resource = g_device->m_buffers[m_constantBuffers[i].get()].native;
				size_t offset = m_constantBufferOffsets[i];
				[m_commandEncoder setVertexBuffer:resource offset:offset atIndex:slot];
				[m_commandEncoder setFragmentBuffer:resource offset:offset atIndex:slot];

			}
		}

		if (m_dirtyState & DirtyStateFlag_Texture)
		{
			for (u32 i=0; i<GfxContext::MaxSampledImages; ++i)
			{
				u32 slot = technique.sampledImageOffset + i;
				id resource = g_device->m_textures[m_sampledImages[u32(GfxStage::Pixel)][i].get()].native;
				[m_commandEncoder setFragmentTexture:resource atIndex:slot];
			}
		}

		if (m_dirtyState & DirtyStateFlag_Sampler)
		{
			for (u32 i=0; i<GfxContext::MaxSamplers; ++i)
			{
				u32 slot = technique.samplerOffset + i;
				id resource = g_device->m_samplers[m_samplers[u32(GfxStage::Pixel)][i].get()].native;
				[m_commandEncoder setFragmentSamplerState:resource atIndex:slot];
			}
		}
	}

	if (m_computeCommandEncoder)
	{
		const u32 stagIdx = u32(GfxStage::Compute);

		if (m_dirtyState & DirtyStateFlag_ConstantBuffer)
		{
			for (u32 i=0; i<GfxContext::MaxConstantBuffers; ++i)
			{
				u32 slot = technique.constantBufferOffset + i;
				id resource = g_device->m_buffers[m_constantBuffers[i].get()].native;
				[m_computeCommandEncoder setBuffer:resource offset:m_constantBufferOffsets[i] atIndex:slot];
			}
		}

		if (m_dirtyState & DirtyStateFlag_Texture)
		{
			for (u32 i=0; i<GfxContext::MaxSampledImages; ++i)
			{
				u32 slot = technique.sampledImageOffset + i;
				id resource = g_device->m_textures[m_sampledImages[stagIdx][i].get()].native;
				[m_computeCommandEncoder setTexture:resource atIndex:slot];
			}
		}

		if (m_dirtyState & DirtyStateFlag_Sampler)
		{
			for (u32 i=0; i<GfxContext::MaxSamplers; ++i)
			{
				u32 slot = technique.samplerOffset + i;
				id resource = g_device->m_samplers[m_samplers[stagIdx][i].get()].native;
				[m_computeCommandEncoder setSamplerState:resource atIndex:slot];
			}
		}

		// TODO: use dirty flags
		for (u32 i=0; i<GfxContext::MaxStorageImages; ++i)
		{
			id<MTLTexture> texture = g_device->m_textures[m_storageImages[i].get()].native;
			u32 slot = technique.storageImageOffset + i;
			[m_computeCommandEncoder setTexture:texture atIndex:slot];
		}

		// TODO: use dirty flags
		for (u32 i=0; i<GfxContext::MaxStorageBuffers; ++i)
		{
			id<MTLBuffer> buffer = g_device->m_buffers[m_storageBuffers[i].get()].native;
			u32 slot = technique.storageBufferOffset + i;
			[m_computeCommandEncoder setBuffer:buffer offset:0 atIndex:slot];
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

		passDescriptor.colorAttachments[i].texture = desc.color[i].valid() ? g_device->m_textures[desc.color[i]].native : g_device->m_backBufferTexture;
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
	passDescriptor.depthAttachment.texture = g_device->m_textures[depthBuffer].native;

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
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_Technique
					 | GfxContext::DirtyStateFlag_VertexBuffer
					 | GfxContext::DirtyStateFlag_ConstantBuffer
					 | GfxContext::DirtyStateFlag_Texture
					 | GfxContext::DirtyStateFlag_Sampler;
}

void Gfx_SetPrimitive(GfxContext* rc, GfxPrimitive type)
{
	rc->m_primitiveType = convertPrimitiveType(type);
	rc->m_primitiveTopology = convertPrimitiveTopology(type);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_Technique;
}

void Gfx_SetIndexStream(GfxContext* rc, GfxBufferArg h)
{
	[rc->m_indexBuffer release];

	rc->m_indexType = g_device->m_buffers[h].indexType;
	rc->m_indexStride = g_device->m_buffers[h].stride;
	rc->m_indexBuffer = g_device->m_buffers[h].native;
	[rc->m_indexBuffer retain];
}

void Gfx_SetVertexStream(GfxContext* rc, u32 idx, GfxBufferArg h)
{
	[rc->m_commandEncoder setVertexBuffer:g_device->m_buffers[h].native offset:0 atIndex:(GfxContext::MaxConstantBuffers+idx)];
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

void Gfx_SetTexture(GfxContext* rc, GfxStage stage, u32 idx, GfxTextureArg h)
{
	RUSH_ASSERT(idx < GfxContext::MaxSampledImages);
	RUSH_ASSERT(stage == GfxStage::Pixel || stage == GfxStage::Compute); // other stages not implemented

	rc->m_sampledImages[u32(stage)][idx].retain(h);
	rc->m_dirtyState |= GfxContext::DirtyStateFlag_Texture;
}

void Gfx_SetSampler(GfxContext* rc, GfxStage stage, u32 idx, GfxSamplerArg h)
{
	RUSH_ASSERT(idx < GfxContext::MaxSamplers);
	RUSH_ASSERT(stage == GfxStage::Pixel || stage == GfxStage::Compute); // other stages not implemented

	rc->m_samplers[u32(stage)][idx].retain(h);
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

	const auto& workGroupSize = g_device->m_techniques[m_pendingTechnique.get()].workGroupSize;

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
		indexBufferOffset:firstIndex * rc->m_indexStride];

	g_device->m_stats.vertices += indexCount;
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
	g_device->m_vertexFormats[h].addReference();
}

void Gfx_Retain(GfxVertexShader h)
{
	g_device->m_vertexShaders[h].addReference();
}

void Gfx_Retain(GfxPixelShader h)
{
	g_device->m_pixelShaders[h].addReference();
}

void Gfx_Retain(GfxComputeShader h)
{
	g_device->m_computeShaders[h].addReference();
}

void Gfx_Retain(GfxTechnique h)
{
	g_device->m_techniques[h].addReference();
}

void Gfx_Retain(GfxTexture h)
{
	g_device->m_textures[h].addReference();
}

void Gfx_Retain(GfxBlendState h)
{
	g_device->m_blendStates[h].addReference();
}

void Gfx_Retain(GfxSampler h)
{
	g_device->m_samplers[h].addReference();
}

void Gfx_Retain(GfxDepthStencilState h)
{
	g_device->m_depthStencilStates[h].addReference();
}

void Gfx_Retain(GfxRasterizerState h)
{
	g_device->m_rasterizerStates[h].addReference();
}

void Gfx_Retain(GfxBuffer h)
{
	g_device->m_buffers[h].addReference();
}

}

#else // RUSH_RENDER_API==RUSH_RENDER_API_MTL
char _GfxDeviceMtl_mm; // suppress linker warning
#endif // RUSH_RENDER_API==RUSH_RENDER_API_MTL
