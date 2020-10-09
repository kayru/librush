#pragma once

#include "Rush.h"
#include "RushC.h"

#include "MathTypes.h"
#include "UtilResourcePool.h"
#include "UtilString.h"
#include "UtilTuple.h"

#include <initializer_list>

// clang-format off

#define RUSH_RENDER_API_NULL	0
#define RUSH_RENDER_API_EXTERN	1
#define RUSH_RENDER_API_DX11	2
#define RUSH_RENDER_API_DX12	3
#define RUSH_RENDER_API_GL		4
#define RUSH_RENDER_API_GLES2	5
#define RUSH_RENDER_API_MTL		6
#define RUSH_RENDER_API_VK		7

// Use default API on some platforms (if not explicitly set)

#ifndef RUSH_RENDER_API
#	ifdef RUSH_PLATFORM_EMSCRIPTEN
#		define RUSH_RENDER_API RUSH_RENDER_API_GLES2
#	endif // RUSH_PLATFORM_EMSCRIPTEN
#	ifdef RUSH_PLATFORM_IOS
#		define RUSH_RENDER_API RUSH_RENDER_API_GLES2
#	endif // RUSH_PLATFORM_IOS
#	ifdef RUSH_PLATFORM_RPI
#		define RUSH_RENDER_API RUSH_RENDER_API_GLES2
#	endif // RUSH_PLATFORM_RPI
#	ifdef RUSH_PLATFORM_MAC
#		define RUSH_RENDER_API RUSH_RENDER_API_MTL
#	endif // RUSH_PLATFORM_MAC
#	ifdef RUSH_PLATFORM_LINUX
#		define RUSH_RENDER_API RUSH_RENDER_API_VK
#	endif // RUSH_PLATFORM_LINUX
#	ifdef RUSH_PLATFORM_WINDOWS
#		define RUSH_RENDER_API RUSH_RENDER_API_VK
#	endif // RUSH_PLATFORM_WINDOWS
#endif //! RUSH_RENDER_API

// clang-format on

// If we got here and did not select an API, select some reasonable default

#ifndef RUSH_RENDER_API
#define RUSH_RENDER_API RUSH_RENDER_API_NULL
#endif // RUSH_RENDER_API

#if RUSH_RENDER_API == RUSH_RENDER_API_NULL
#define RUSH_RENDER_API_NAME "Null"
#elif RUSH_RENDER_API == RUSH_RENDER_API_DX11
#define RUSH_RENDER_API_NAME "DirectX 11"
#elif RUSH_RENDER_API == RUSH_RENDER_API_DX12
#define RUSH_RENDER_API_NAME "DirectX 12"
#elif RUSH_RENDER_API == RUSH_RENDER_API_GL
#define RUSH_RENDER_API_NAME "OpenGL"
#elif RUSH_RENDER_API == RUSH_RENDER_API_GLES2
#define RUSH_RENDER_API_NAME "OpenGL ES2"
#elif RUSH_RENDER_API == RUSH_RENDER_API_MTL
#define RUSH_RENDER_API_NAME "Metal"
#define RUSH_RENDER_SUPPORT_DESCRIPTOR_SETS 1
#elif RUSH_RENDER_API == RUSH_RENDER_API_VK
#define RUSH_RENDER_API_NAME "Vulkan"
#define RUSH_RENDER_SUPPORT_IMAGE_BARRIERS  1
#define RUSH_RENDER_SUPPORT_ASYNC_COMPUTE   1
#define RUSH_RENDER_SUPPORT_MESH_SHADER     1
#define RUSH_RENDER_SUPPORT_DESCRIPTOR_SETS 1
#define RUSH_RENDER_SUPPORT_RAY_TRACING     1
#define RUSH_RENDER_SUPPORT_BUFFER_ADDRESS  1
#else // RUSH_RENDER_API_EXTERNAL
#define RUSH_RENDER_API_NAME "Unknown"
#endif

namespace Rush
{
class Window;
class GfxDevice;
class GfxContext;

struct Vec2;
struct Vec3;
struct Vec4;
struct Mat4;

class GfxVertexFormatDesc;
struct GfxVertexShaderDesc;
struct GfxPixelShaderDesc;
struct GfxGeometryShaderDesc;
struct GfxComputeShaderDesc;
struct GfxMeshShaderDesc;
struct GfxTextureDesc;
struct GfxBufferDesc;
struct GfxSamplerDesc;
struct GfxBlendStateDesc;
struct GfxTechniqueDesc;
struct GfxDepthStencilDesc;
struct GfxRasterizerDesc;
struct GfxDescriptorSetDesc;
struct GfxRayTracingPipelineDesc;
struct GfxAccelerationStructureDesc;

typedef ResourceHandle<GfxVertexFormatDesc>          GfxVertexFormat;
typedef ResourceHandle<GfxVertexShaderDesc>          GfxVertexShader;
typedef ResourceHandle<GfxPixelShaderDesc>           GfxPixelShader;
typedef ResourceHandle<GfxGeometryShaderDesc>        GfxGeometryShader;
typedef ResourceHandle<GfxComputeShaderDesc>         GfxComputeShader;
typedef ResourceHandle<GfxMeshShaderDesc>            GfxMeshShader;
typedef ResourceHandle<GfxRayTracingPipelineDesc>    GfxRayTracingPipeline;
typedef ResourceHandle<GfxAccelerationStructureDesc> GfxAccelerationStructure;
typedef ResourceHandle<GfxTextureDesc>               GfxTexture;
typedef ResourceHandle<GfxBufferDesc>                GfxBuffer;
typedef ResourceHandle<GfxSamplerDesc>               GfxSampler;
typedef ResourceHandle<GfxBlendStateDesc>            GfxBlendState;
typedef ResourceHandle<GfxDepthStencilDesc>          GfxDepthStencilState;
typedef ResourceHandle<GfxRasterizerDesc>            GfxRasterizerState;
typedef ResourceHandle<GfxTechniqueDesc>             GfxTechnique;
typedef ResourceHandle<GfxDescriptorSetDesc>         GfxDescriptorSet;

u32 Gfx_GenerateUniqueId();

struct GfxRefCount
{
	void addReference() { m_refs++; }
	u32  removeReference()
	{
		RUSH_ASSERT(m_refs != 0);
		return m_refs--;
	}
	u32 m_refs = 0;
};

struct GfxResourceBase : GfxRefCount
{
	GfxResourceBase() = default;
	GfxResourceBase(GfxResourceBase&&) noexcept = default;
	GfxResourceBase& operator = (GfxResourceBase&&) = default;
	GfxResourceBase(const GfxResourceBase&) = delete;
	~GfxResourceBase() = default;

	u32 getId() const { return m_id; }

private:
	u32 m_id = Gfx_GenerateUniqueId();
};

template <typename T> class GfxOwn
{
public:
	friend GfxDevice;

	GfxOwn(const GfxOwn& rhs) = delete;
	GfxOwn& operator=(const GfxOwn<T>& other) = delete;

	GfxOwn() : m_handle(InvalidResourceHandle()) {}
	GfxOwn(InvalidResourceHandle) : m_handle(InvalidResourceHandle()) {}
	GfxOwn(GfxOwn&& rhs) noexcept : m_handle(rhs.m_handle) { rhs.m_handle = InvalidResourceHandle(); }
	GfxOwn& operator=(GfxOwn<T>&& rhs) noexcept
	{
		if (m_handle != rhs.m_handle)
		{
			reset();
			m_handle     = rhs.m_handle;
			rhs.m_handle = InvalidResourceHandle();
		}

		return *this;
	}
	~GfxOwn() { reset(); }

	T    get() const { return m_handle; }
	bool valid() const { return m_handle.valid(); }

	T detach()
	{
		T result = m_handle;
		m_handle = InvalidResourceHandle();
		return result;
	}

	void reset()
	{
		if (m_handle.valid())
		{
			Gfx_Release(m_handle);
		}
		m_handle = InvalidResourceHandle();
	}

private:
	GfxOwn(T h) : m_handle(h) {}

	T m_handle;
};

template <typename T> class GfxArg;

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

	GfxRef(GfxRef&& rhs) noexcept : m_handle(rhs.m_handle) { rhs.m_handle = InvalidResourceHandle(); }
	~GfxRef() { if(m_handle.valid()) Gfx_Release(m_handle); }

	GfxRef& operator=(GfxRef<T>&& rhs)
	{
		if (m_handle != rhs.m_handle)
		{
			reset();
			m_handle     = rhs.m_handle;
			rhs.m_handle = InvalidResourceHandle();
		}

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

	T    get() const { return m_handle; }
	bool valid() const { return m_handle.valid(); }

	void reset()
	{
		if (m_handle.valid())
		{
			Gfx_Release(m_handle);
		}
		m_handle = InvalidResourceHandle();
	}

	void retain(GfxArg<T> h);

private:
	T m_handle;
};

template <typename T> class GfxArg
{
public:
	friend GfxDevice;
	friend GfxContext;

	RUSH_FORCEINLINE GfxArg(T h) : m_handle(h){};
	RUSH_FORCEINLINE GfxArg(InvalidResourceHandle) : m_handle(InvalidResourceHandle()){};
	RUSH_FORCEINLINE GfxArg(const GfxOwn<T>& h) : m_handle(h.get()){};
	RUSH_FORCEINLINE GfxArg(const GfxRef<T>& h) : m_handle(h.get()){};

	RUSH_FORCEINLINE      operator T() const { return m_handle; }
	RUSH_FORCEINLINE bool valid() const { return m_handle.valid(); }

private:
	T m_handle;
};

typedef GfxArg<GfxVertexFormat>          GfxVertexFormatArg;
typedef GfxArg<GfxVertexShader>          GfxVertexShaderArg;
typedef GfxArg<GfxPixelShader>           GfxPixelShaderArg;
typedef GfxArg<GfxGeometryShader>        GfxGeometryShaderArg;
typedef GfxArg<GfxComputeShader>         GfxComputeShaderArg;
typedef GfxArg<GfxMeshShader>            GfxMeshShaderArg;
typedef GfxArg<GfxTexture>               GfxTextureArg;
typedef GfxArg<GfxBuffer>                GfxBufferArg;
typedef GfxArg<GfxSampler>               GfxSamplerArg;
typedef GfxArg<GfxBlendState>            GfxBlendStateArg;
typedef GfxArg<GfxDepthStencilState>     GfxDepthStencilStateArg;
typedef GfxArg<GfxRasterizerState>       GfxRasterizerStateArg;
typedef GfxArg<GfxTechnique>             GfxTechniqueArg;
typedef GfxArg<GfxDescriptorSet>         GfxDescriptorSetArg;
typedef GfxArg<GfxRayTracingPipeline>    GfxRayTracingPipelineArg;
typedef GfxArg<GfxAccelerationStructure> GfxAccelerationStructureArg;

enum class GfxContextType : u8
{
	Graphics, // Universal
	Compute,  // Async compute
	Transfer, // Async copy / DMA
	count
};

enum class TextureType : u8
{
	Tex1D = RUSH_GFX_TEXTURE_TYPE_1D,
	Tex1DArray = RUSH_GFX_TEXTURE_TYPE_1D_ARRAY,
	Tex2D = RUSH_GFX_TEXTURE_TYPE_2D,
	Tex2DArray = RUSH_GFX_TEXTURE_TYPE_2D_ARRAY,
	Tex3D = RUSH_GFX_TEXTURE_TYPE_3D,
	TexCube = RUSH_GFX_TEXTURE_TYPE_CUBE,
	TexCubeArray = RUSH_GFX_TEXTURE_TYPE_CUBE_ARRAY,
};

inline bool isCubeTexture(TextureType type)
{
	return type == TextureType::TexCube || type == TextureType::TexCubeArray;
}

inline bool isArrayTexture(TextureType type)
{
	return type == TextureType::Tex1DArray || type == TextureType::Tex2DArray || type == TextureType::TexCubeArray;
}

enum GfxShaderSourceType : u8
{
	GfxShaderSourceType_Unknown = RUSH_GFX_SHADER_SOURCE_UNKNOWN,
	GfxShaderSourceType_SPV     = RUSH_GFX_SHADER_SOURCE_SPV,  // binary
	GfxShaderSourceType_GLSL    = RUSH_GFX_SHADER_SOURCE_GLSL, // text
	GfxShaderSourceType_HLSL    = RUSH_GFX_SHADER_SOURCE_HLSL, // text
	GfxShaderSourceType_DXBC    = RUSH_GFX_SHADER_SOURCE_DXBC, // binary
	GfxShaderSourceType_DXIL    = RUSH_GFX_SHADER_SOURCE_DXIL, // binary
	GfxShaderSourceType_MSL     = RUSH_GFX_SHADER_SOURCE_MSL,  // text
};

enum GfxFormatType : u8
{
	GfxFormatType_Unknown        = 0,
	GfxFormatType_Float          = 1,
	GfxFormatType_Float_Typeless = 2,
	GfxFormatType_Float_Uint     = 3,
	GfxFormatType_Sint           = 4,
	GfxFormatType_Snorm          = 5,
	GfxFormatType_sRGB           = 6,
	GfxFormatType_Typeless       = 7,
	GfxFormatType_Typeless_Uint  = 8, // e.g. X24_TYPELESS_G8_UINT
	GfxFormatType_Ufloat         = 9,
	GfxFormatType_Uint           = 10,
	GfxFormatType_Unorm          = 11,
	GfxFormatType_Unorm_Typeless = 12, // e.g. R24_UNORM_X8_TYPELESS
	GfxFormatType_Unorm_Uint     = 13, // e.g. R24_UNORM_G8_UINT
	GfxFormatType_UFloat         = 14, // unsigned float (BC6H_UF16)
};

enum GfxFormatStorage : u32
{
	GfxFormatStorage_Unknown  = 0,
	GfxFormatStorage_BC1      = 1 << 8,
	GfxFormatStorage_BC2      = 2 << 8,
	GfxFormatStorage_BC3      = 3 << 8,
	GfxFormatStorage_BC4      = 4 << 8,
	GfxFormatStorage_BC5      = 5 << 8,
	GfxFormatStorage_BC6H     = 6 << 8,
	GfxFormatStorage_BC7      = 7 << 8,
	GfxFormatStorage_R16      = 8 << 8,
	GfxFormatStorage_R24G8    = 9 << 8,
	GfxFormatStorage_R32      = 10 << 8,
	GfxFormatStorage_R32G8X24 = 11 << 8,
	GfxFormatStorage_R8       = 12 << 8,
	GfxFormatStorage_RG16     = 13 << 8,
	GfxFormatStorage_RG32     = 14 << 8,
	GfxFormatStorage_RG8      = 15 << 8,
	GfxFormatStorage_RGB16    = 16 << 8,
	GfxFormatStorage_RGB32    = 17 << 8,
	GfxFormatStorage_RGB8     = 18 << 8,
	GfxFormatStorage_RGBA16   = 19 << 8,
	GfxFormatStorage_RGBA32   = 20 << 8,
	GfxFormatStorage_RGBA8    = 21 << 8,
	GfxFormatStorage_BGRA8    = 22 << 8,
};

enum GfxFormatComponent : u32
{
	GfxFormatComponent_Unknown = 0,
	GfxFormatComponent_R       = 1 << 16,
	GfxFormatComponent_G       = 1 << 17,
	GfxFormatComponent_B       = 1 << 18,
	GfxFormatComponent_A       = 1 << 19,
	GfxFormatComponent_Depth   = 1 << 20,
	GfxFormatComponent_Stencil = 1 << 21,

	GfxFormatComponent_RG           = GfxFormatComponent_R | GfxFormatComponent_G,
	GfxFormatComponent_RGB          = GfxFormatComponent_RG | GfxFormatComponent_B,
	GfxFormatComponent_RGBA         = GfxFormatComponent_RGB | GfxFormatComponent_A,
	GfxFormatComponent_DepthStencil = GfxFormatComponent_Depth | GfxFormatComponent_Stencil,
};

enum GfxFormat : u32
{
	GfxFormat_Unknown = GfxFormatComponent_Unknown | GfxFormatStorage_Unknown | GfxFormatType_Unknown,

	GfxFormat_D24_Unorm_S8_Uint = GfxFormatStorage_R24G8 | GfxFormatType_Unorm_Uint | GfxFormatComponent_DepthStencil,
	GfxFormat_D24_Unorm_X8      = GfxFormatStorage_R24G8 | GfxFormatType_Unorm_Typeless | GfxFormatComponent_Depth,
	GfxFormat_D32_Float         = GfxFormatStorage_R32 | GfxFormatType_Float | GfxFormatComponent_Depth,
	GfxFormat_D32_Float_S8_Uint =
	    GfxFormatStorage_R32G8X24 | GfxFormatType_Float_Uint | GfxFormatComponent_DepthStencil,
	GfxFormat_R8_Unorm     = GfxFormatStorage_R8 | GfxFormatType_Unorm | GfxFormatComponent_R,
	GfxFormat_R16_Float    = GfxFormatStorage_R16 | GfxFormatType_Float | GfxFormatComponent_R,
	GfxFormat_R16_Uint     = GfxFormatStorage_R16 | GfxFormatType_Uint | GfxFormatComponent_R,
	GfxFormat_R32_Float    = GfxFormatStorage_R32 | GfxFormatType_Float | GfxFormatComponent_R,
	GfxFormat_R32_Uint     = GfxFormatStorage_R32 | GfxFormatType_Uint | GfxFormatComponent_R,
	GfxFormat_RG8_Unorm    = GfxFormatStorage_RG8 | GfxFormatType_Unorm | GfxFormatComponent_RG,
	GfxFormat_RG16_Float   = GfxFormatStorage_RG16 | GfxFormatType_Float | GfxFormatComponent_RG,
	GfxFormat_RG32_Float   = GfxFormatStorage_RG32 | GfxFormatType_Float | GfxFormatComponent_RG,
	GfxFormat_RGB32_Float  = GfxFormatStorage_RGB32 | GfxFormatType_Float | GfxFormatComponent_RGB,
	GfxFormat_RGB8_Unorm   = GfxFormatStorage_RGB8 | GfxFormatType_Unorm | GfxFormatComponent_RGB,
	GfxFormat_RGBA16_Float = GfxFormatStorage_RGBA16 | GfxFormatType_Float | GfxFormatComponent_RGBA,
	GfxFormat_RGBA16_Unorm = GfxFormatStorage_RGB16 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_RGBA32_Float = GfxFormatStorage_RGBA32 | GfxFormatType_Float | GfxFormatComponent_RGBA,
	GfxFormat_RGBA8_Unorm  = GfxFormatStorage_RGBA8 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_RGBA8_sRGB   = GfxFormatStorage_RGBA8 | GfxFormatType_sRGB | GfxFormatComponent_RGBA,
	GfxFormat_BGRA8_Unorm  = GfxFormatStorage_BGRA8 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_BGRA8_sRGB   = GfxFormatStorage_BGRA8 | GfxFormatType_sRGB | GfxFormatComponent_RGBA,

	// Block-compressed formats
	GfxFormat_BC1_Unorm      = GfxFormatStorage_BC1 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_BC1_Unorm_sRGB = GfxFormatStorage_BC1 | GfxFormatType_sRGB | GfxFormatComponent_RGBA,
	GfxFormat_BC3_Unorm      = GfxFormatStorage_BC3 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_BC3_Unorm_sRGB = GfxFormatStorage_BC3 | GfxFormatType_sRGB | GfxFormatComponent_RGBA,
	GfxFormat_BC4_Unorm      = GfxFormatStorage_BC4 | GfxFormatType_Unorm | GfxFormatComponent_R,
	GfxFormat_BC5_Unorm      = GfxFormatStorage_BC5 | GfxFormatType_Unorm | GfxFormatComponent_RG,
	GfxFormat_BC6H_UFloat    = GfxFormatStorage_BC6H | GfxFormatType_UFloat | GfxFormatComponent_RGB,
	GfxFormat_BC6H_SFloat    = GfxFormatStorage_BC6H | GfxFormatType_Float | GfxFormatComponent_RGB,
	GfxFormat_BC7_Unorm      = GfxFormatStorage_BC7 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_BC7_Unorm_sRGB = GfxFormatStorage_BC7 | GfxFormatType_sRGB | GfxFormatComponent_RGBA,
};

enum class GfxUsageFlags : u8
{
	None = 0,

	ShaderResource = RUSH_GFX_USAGE_SHADER_RESOURCE,
	RenderTarget   = RUSH_GFX_USAGE_RENDER_TARGET,
	DepthStencil   = RUSH_GFX_USAGE_DEPTH_STENCIL,
	StorageImage   = RUSH_GFX_USAGE_STORAGE_IMAGE,
	TransferSrc    = RUSH_GFX_USAGE_TRANSFER_SRC,
	TransferDst    = RUSH_GFX_USAGE_TRANSFER_DST,

	RenderTarget_ShaderResource = ShaderResource | RenderTarget,
	DepthStencil_ShaderResource = ShaderResource | DepthStencil,
	StorageImage_ShaderResource = ShaderResource | StorageImage,
};

RUSH_IMPLEMENT_FLAG_OPERATORS(GfxUsageFlags, u8);

enum class GfxStage : u8
{
	Vertex      = RUSH_GFX_STAGE_VERTEX,
	Geometry    = RUSH_GFX_STAGE_GEOMETRY,
	Pixel       = RUSH_GFX_STAGE_PIXEL,
	Hull        = RUSH_GFX_STAGE_HULL,
	Domain      = RUSH_GFX_STAGE_DOMAIN,
	Compute     = RUSH_GFX_STAGE_COMPUTE,
	Mesh        = RUSH_GFX_STAGE_MESH,
	RayTracing  = RUSH_GFX_STAGE_RAYTRACING,
	count
};

enum class GfxStageFlags : u8
{
	None = RUSH_GFX_STAGE_FLAG_NONE,

	Vertex     = RUSH_GFX_STAGE_FLAG_VERTEX,
	Geometry   = RUSH_GFX_STAGE_FLAG_GEOMETRY,
	Pixel      = RUSH_GFX_STAGE_FLAG_PIXEL,
	Hull       = RUSH_GFX_STAGE_FLAG_HULL,
	Domain     = RUSH_GFX_STAGE_FLAG_DOMAIN,
	Compute    = RUSH_GFX_STAGE_FLAG_COMPUTE,
	Mesh       = RUSH_GFX_STAGE_FLAG_MESH,
	RayTracing = RUSH_GFX_STAGE_FLAG_RAYTRACING,

	VertexPixel = Vertex | Pixel,
	All         = 0xFF,
};

RUSH_IMPLEMENT_FLAG_OPERATORS(GfxStageFlags, u8);

enum class GfxPrimitive : u8
{
	PointList = RUSH_GFX_PRIMITIVE_POINT_LIST,
	LineList = RUSH_GFX_PRIMITIVE_LINE_LIST,
	LineStrip = RUSH_GFX_PRIMITIVE_LINE_STRIP,
	TriangleList = RUSH_GFX_PRIMITIVE_TRIANGLE_LIST,
	TriangleStrip = RUSH_GFX_PRIMITIVE_TRIANGLE_STRIP,
	count
};

enum class GfxClearFlags : u8
{
	None = 0,

	Color   = 1 << 0,
	Depth   = 1 << 1,
	Stencil = 1 << 2,

	DepthStencil      = Depth | Stencil,
	StencilColor      = Stencil | Color,
	ColorDepth        = Color | Depth,
	ColorDepthStencil = Color | Depth | Stencil,

	All = 0xFF
};

RUSH_IMPLEMENT_FLAG_OPERATORS(GfxClearFlags, u8);

enum class GfxBufferFlags : u32
{
	None         = RUSH_GFX_BUFFER_FLAG_NONE,
	Vertex       = RUSH_GFX_BUFFER_FLAG_VERTEX,
	Index        = RUSH_GFX_BUFFER_FLAG_INDEX,
	Constant     = RUSH_GFX_BUFFER_FLAG_CONSTANT,
	Storage      = RUSH_GFX_BUFFER_FLAG_STORAGE,
	Texel        = RUSH_GFX_BUFFER_FLAG_TEXEL,
	IndirectArgs = RUSH_GFX_BUFFER_FLAG_INDIRECT_ARGS,
	RayTracing   = RUSH_GFX_BUFFER_FLAG_RAYTRACING,

	Transient    = RUSH_GFX_BUFFER_FLAG_TRANSIENT,

	TransientVertex   = Transient | Vertex,
	TransientIndex    = Transient | Index,
	TransientConstant = Transient | Constant,

	TypeMask = Vertex | Index | Constant | Storage | Texel | IndirectArgs,
};

RUSH_IMPLEMENT_FLAG_OPERATORS(GfxBufferFlags, u32);

enum class GfxShaderType : u8
{
	Vertex,
	Geometry,
	Pixel,
	Hull,
	Domain,
	Compute
};

enum class GfxBlendParam : u8
{
	Zero = RUSH_GFX_BLEND_PARAM_ZERO,
	One = RUSH_GFX_BLEND_PARAM_ONE,
	SrcColor = RUSH_GFX_BLEND_PARAM_SRC_COLOR,
	InvSrcColor = RUSH_GFX_BLEND_PARAM_INV_SRC_COLOR,
	SrcAlpha = RUSH_GFX_BLEND_PARAM_SRC_ALPHA,
	InvSrcAlpha = RUSH_GFX_BLEND_PARAM_INV_SRC_ALPHA,
	DestAlpha = RUSH_GFX_BLEND_PARAM_DEST_ALPHA,
	InvDestAlpha = RUSH_GFX_BLEND_PARAM_INV_DEST_ALPHA,
	DestColor = RUSH_GFX_BLEND_PARAM_DEST_COLOR,
	InvDestColor = RUSH_GFX_BLEND_PARAM_INV_DEST_COLOR,

	count
};

enum class GfxBlendOp : u8
{
	Add = RUSH_GFX_BLEND_OP_ADD,
	Subtract = RUSH_GFX_BLEND_OP_SUBTRACT,
	RevSubtract = RUSH_GFX_BLEND_OP_REV_SUBTRACT,
	Min = RUSH_GFX_BLEND_OP_MIN,
	Max = RUSH_GFX_BLEND_OP_MAX,

	count
};

enum class GfxTextureFilter : u8
{
	Point = RUSH_GFX_TEXTURE_FILTER_POINT,
	Linear = RUSH_GFX_TEXTURE_FILTER_LINEAR,
	Anisotropic = RUSH_GFX_TEXTURE_FILTER_ANISOTROPIC,

	count
};

enum class GfxTextureWrap : u8
{
	Wrap = RUSH_GFX_TEXTURE_WRAP_REPEAT,
	Mirror = RUSH_GFX_TEXTURE_WRAP_MIRROR,
	Clamp = RUSH_GFX_TEXTURE_WRAP_CLAMP,

	count
};

enum class GfxCompareFunc : u8
{
	Never = RUSH_GFX_COMPARE_FUNC_NEVER,
	Less = RUSH_GFX_COMPARE_FUNC_LESS,
	Equal = RUSH_GFX_COMPARE_FUNC_EQUAL,
	LessEqual = RUSH_GFX_COMPARE_FUNC_LESS_EQUAL,
	Greater = RUSH_GFX_COMPARE_FUNC_GREATER,
	NotEqual = RUSH_GFX_COMPARE_FUNC_NOT_EQUAL,
	GreaterEqual = RUSH_GFX_COMPARE_FUNC_GREATER_EQUAL,
	Always = RUSH_GFX_COMPARE_FUNC_ALWAYS,

	count
};

enum class GfxFillMode : u8
{
	Solid = RUSH_GFX_FILL_MODE_SOLID,
	Wireframe = RUSH_GFX_FILL_MODE_WIREFRAME,
};

enum class GfxCullMode : u8
{
	None = RUSH_GFX_CULL_MODE_NONE,
	CW = RUSH_GFX_CULL_MODE_CW,
	CCW = RUSH_GFX_CULL_MODE_CCW,
};

inline GfxFormatType      getGfxFormatType(GfxFormat fmt) { return (GfxFormatType)(fmt & 0x000000FF); }
inline GfxFormatStorage   getGfxFormatStorage(GfxFormat fmt) { return (GfxFormatStorage)(fmt & 0x0000FF00); }
inline GfxFormatComponent getGfxFormatComponent(GfxFormat fmt) { return (GfxFormatComponent)(fmt & 0x00FF0000); }
inline bool               isGfxFormatDepth(GfxFormat fmt) { return (fmt & GfxFormatComponent_Depth) != 0; }
inline bool               isGfxFormatStencil(GfxFormat fmt) { return (fmt & GfxFormatComponent_Stencil) != 0; }

inline bool isGfxFormatBlockCompressed(GfxFormat fmt)
{
	switch (getGfxFormatStorage(fmt))
	{
	default: return false;
	case GfxFormatStorage_BC1:
	case GfxFormatStorage_BC2:
	case GfxFormatStorage_BC3:
	case GfxFormatStorage_BC4:
	case GfxFormatStorage_BC5:
	case GfxFormatStorage_BC6H:
	case GfxFormatStorage_BC7: return true;
	}
}

u32 getBitsPerPixel(GfxFormat fmt);
u32 getBitsPerPixel(GfxFormatStorage fmt);

const char* toString(GfxFormatStorage storage);
const char* toString(GfxFormatType type);
const char* toString(GfxFormat fmt);
const char* toStringShort(GfxFormat fmt);

struct GfxViewport
{
	GfxViewport();
	GfxViewport(Tuple2i size);
	GfxViewport(Tuple2u size);
	GfxViewport(const Box2& bounds, float _depthMin = 0, float _depthMax = 1);
	GfxViewport(float _x, float _y, float _w, float _h, float _depthMin = 0, float _depthMax = 1);

	bool operator==(const GfxViewport& rhs) const;
	bool operator!=(const GfxViewport& rhs) const;

	float x; // top left x
	float y; // top left y
	float w; // width
	float h; // height
	float depthMin;
	float depthMax;
};

struct GfxRect
{
	int left, top, right, bottom;
};

struct GfxBufferDesc
{
	GfxBufferDesc() = default;
	GfxBufferDesc(GfxBufferFlags _flags, GfxFormat _format, u32 _count, u32 _stride);
	GfxBufferDesc(GfxBufferFlags _flags, u32 _count, u32 _stride);

	GfxBufferFlags flags       = GfxBufferFlags::None;
	GfxFormat      format      = GfxFormat_Unknown;
	u32            stride      = 0;
	u32            count       = 0;
	bool           hostVisible = false;
};

struct GfxBlendStateDesc
{
	GfxBlendParam src = GfxBlendParam::One;
	GfxBlendParam dst = GfxBlendParam::Zero;
	GfxBlendOp    op  = GfxBlendOp::Add;

	GfxBlendParam alphaSrc = GfxBlendParam::One;
	GfxBlendParam alphaDst = GfxBlendParam::Zero;
	GfxBlendOp    alphaOp  = GfxBlendOp::Add;

	bool alphaSeparate = false;
	bool enable        = false;

	// Helper functions to create common blend states

	static GfxBlendStateDesc makeOpaque();
	static GfxBlendStateDesc makeLerp();
	static GfxBlendStateDesc makeAdditive();
	static GfxBlendStateDesc makePremultiplied();
};

struct GfxSamplerDesc
{
	GfxTextureFilter filterMin = GfxTextureFilter::Linear;
	GfxTextureFilter filterMag = GfxTextureFilter::Linear;
	GfxTextureFilter filterMip = GfxTextureFilter::Linear;

	GfxTextureWrap wrapU = GfxTextureWrap::Wrap;
	GfxTextureWrap wrapV = GfxTextureWrap::Wrap;
	GfxTextureWrap wrapW = GfxTextureWrap::Wrap;

	GfxCompareFunc compareFunc   = GfxCompareFunc::Never;
	bool           compareEnable = false;

	float anisotropy = 1.0f;
	float mipLodBias = 0.0f;

	// Helper functions

	static GfxSamplerDesc makeLinear();
	static GfxSamplerDesc makePoint();
};

struct GfxDepthStencilDesc
{
	GfxCompareFunc compareFunc = GfxCompareFunc::LessEqual;
	bool           enable      = true;
	bool           writeEnable = true;

	// TODO: Stencil and others
};

struct GfxRasterizerDesc
{
	GfxFillMode fillMode            = GfxFillMode::Solid;
	GfxCullMode cullMode            = GfxCullMode::None;
	float       depthBias           = 0.0f;
	float       depthBiasSlopeScale = 0.0f;
};

class GfxVertexFormatDesc
{

	static const u32 MaxStreams  = 8;
	static const u32 MaxElements = 16;

public:
	enum class DataType : u8
	{
		Unused  = 0,
		Float1  = 1,  // float
		Float2  = 2,  // float x2
		Float3  = 3,  // float x3
		Float4  = 4,  // float x4
		Half2   = 5,  // half-float x2
		Half4   = 6,  // half-float x4
		Short2  = 7,  // 16-bit signed integer x2
		Short2N = 8,  // normalized 16 bit signed integer
		UByte4  = 9,  // 8-bit unsigned integer x4
		Dec3N   = 10, // 10-10-10-2 normalized integer
		Color   = 11, // 8-8-8-8 normalized integer
		UInt    = 12, // unsigned 32-bit integer
		UByte4N = 13, // normalized 8-bit unsigned integer x4
	};

	enum class Semantic : u8
	{
		Unused       = RUSH_GFX_VERTEX_SEMANTIC_UNUSED,
		Position     = RUSH_GFX_VERTEX_SEMANTIC_POSITION,
		Texcoord     = RUSH_GFX_VERTEX_SEMANTIC_TEXCOORD,
		Color        = RUSH_GFX_VERTEX_SEMANTIC_COLOR,
		Normal       = RUSH_GFX_VERTEX_SEMANTIC_NORMAL,
		TangentU     = RUSH_GFX_VERTEX_SEMANTIC_TANGENTU,
		TangentV     = RUSH_GFX_VERTEX_SEMANTIC_TANGENTV,
		InstanceData = RUSH_GFX_VERTEX_SEMANTIC_INSTANCEDATA,
		BoneIndex    = RUSH_GFX_VERTEX_SEMANTIC_BONEINDEX,
		BoneWeight   = RUSH_GFX_VERTEX_SEMANTIC_BONEWEIGHT,

		Tangent   = TangentU, // backwards-compatible name
		Bitangent = TangentV, // backwards-compatible name
	};

	struct Element
	{
		Element() = default;
		Element(u16 _stream, DataType _type, Semantic _semantic, u8 _index);

		u16      stream;
		u16      size;
		u16      offset;
		u8       index;
		DataType type;
		Semantic semantic;

		bool operator==(const Element& rhs) const
		{
			return stream == rhs.stream && type == rhs.type && semantic == rhs.semantic && index == rhs.index;
		}
	};

public:
	GfxVertexFormatDesc();

	inline const Element& element(u32 n) const { return m_elements[n]; }
	inline size_t         elementCount() const { return m_elements.size(); }
	inline u16            streamStride(u32 n) const { return m_streamOffset[n]; }

	inline bool hasPosition() const { return m_hasPosition; }
	inline bool hasNormal() const { return m_hasNormal; }
	inline bool hasColor() const { return m_hasColor; }

	bool operator==(const GfxVertexFormatDesc& rhs) const;

	inline void add(u16 _stream, DataType _type, Semantic _usage, u8 _index)
	{
		Element elem(_stream, _type, _usage, _index);
		add(elem);
	}

	void add(Element element);

	const Element* begin() const { return m_elements.begin(); }
	const Element* end() const { return m_elements.end(); }

private:
	StaticArray<Element, MaxElements> m_elements;

	u16 m_streamOffset[MaxStreams] = {};

	bool m_hasPosition;
	bool m_hasNormal;
	bool m_hasColor;
};

const char* toString(GfxVertexFormatDesc::Semantic type);

u16 dataTypeSize(GfxVertexFormatDesc::DataType type);

struct GfxShaderSource : public DynamicArray<char>
{
	GfxShaderSource() : type(GfxShaderSourceType_Unknown) {}
	GfxShaderSource(GfxShaderSourceType _type, const char* _code, size_t _size = 0, const char* _entry = "main");
	GfxShaderSourceType type;
	const char*         entry = "main";
};

enum class GfxDescriptorSetFlags : u8
{
	None  = 0,
	TextureArray = 1 << 0,
	VariableDescriptorCount = 1 << 1,
};
RUSH_IMPLEMENT_FLAG_OPERATORS(GfxDescriptorSetFlags, u8)

struct GfxDescriptorSetDesc
{
	u16                   constantBuffers        = 0;
	u16                   samplers               = 0;
	u16                   textures               = 0;
	u16                   rwImages               = 0;
	u16                   rwBuffers              = 0;
	u16                   rwTypedBuffers         = 0;
	u16                   accelerationStructures = 0;
	GfxStageFlags         stageFlags             = GfxStageFlags::All;
	GfxDescriptorSetFlags flags                  = GfxDescriptorSetFlags::None;

	u32 getResourceCount() const
	{
		return constantBuffers + samplers 
			+ (bool(flags&GfxDescriptorSetFlags::TextureArray) ? 1 : textures)
			+ rwImages + rwBuffers + rwTypedBuffers + accelerationStructures;
	}

	bool isEmpty() const { return getResourceCount() == 0; }

	bool operator==(const GfxDescriptorSetDesc& other) const { return !memcmp(this, &other, sizeof(*this)); }

	bool operator!=(const GfxDescriptorSetDesc& other) const { return !(*this == other); }
};

struct GfxShaderBindingDesc : GfxDescriptorSetDesc
{
	// Shader resources must be specified in the same order as members of this struct
	GfxStageFlags pushConstantStageFlags = GfxStageFlags::None;
	u8            pushConstants          = 0;

	static constexpr u32 MaxDescriptorSets                 = 4;
	bool                 useDefaultDescriptorSet           = true; // if 'true', then descriptor set 0 is reserved
	GfxDescriptorSetDesc descriptorSets[MaxDescriptorSets] = {};
};

struct GfxSpecializationConstant
{
	u32    id;
	u32    offset;
	u32    size;
};

struct GfxTechniqueDesc
{
	GfxTechniqueDesc() {}

	GfxTechniqueDesc(
	    GfxPixelShaderArg _ps, GfxVertexShaderArg _vs, GfxVertexFormatArg _vf, const GfxShaderBindingDesc& _bindings)
	: ps(_ps), vs(_vs), vf(_vf), bindings(_bindings)
	{
	}

	GfxTechniqueDesc(GfxPixelShaderArg _ps, GfxMeshShaderArg _ms, const GfxShaderBindingDesc& _bindings)
	: ps(_ps), ms(_ms), bindings(_bindings)
	{
	}

	GfxTechniqueDesc(GfxComputeShaderArg _cs, const GfxShaderBindingDesc& _bindings, const Tuple3<u16>& _workGroupSize)
	: cs(_cs), bindings(_bindings), workGroupSize(_workGroupSize)
	{
	}

	GfxComputeShader     cs;
	GfxPixelShader       ps;
	GfxGeometryShader    gs;
	GfxVertexShader      vs;
	GfxMeshShader        ms;
	GfxVertexFormat      vf;
	GfxShaderBindingDesc bindings      = {};
	Tuple3<u16>          workGroupSize = {};

	u32                              specializationConstantCount = 0;
	const GfxSpecializationConstant* specializationConstants     = nullptr;
	const void*                      specializationData          = nullptr;
	u32                              specializationDataSize      = 0;

	float psWaveLimit = 1.0f;
	float vsWaveLimit = 1.0f;
	float csWaveLimit = 1.0f;
};

struct GfxTextureDesc
{
	static GfxTextureDesc make2D(u32 _width, u32 _height, GfxFormat _format = GfxFormat_RGBA8_Unorm,
	    GfxUsageFlags _usage = GfxUsageFlags::ShaderResource);

	static GfxTextureDesc make2D(const Tuple2i size, GfxFormat _format = GfxFormat_RGBA8_Unorm,
		GfxUsageFlags _usage = GfxUsageFlags::ShaderResource)
	{
		return make2D(size.x, size.y, _format, _usage);
	}

	static GfxTextureDesc make3D(u32 _width, u32 _height, u32 _depth, GfxFormat _format = GfxFormat_RGBA8_Unorm,
	    GfxUsageFlags _usage = GfxUsageFlags::ShaderResource);

	static GfxTextureDesc makeCube(
	    u32 _size, GfxFormat _format = GfxFormat_RGBA8_Unorm, GfxUsageFlags _usage = GfxUsageFlags::ShaderResource);

	bool operator==(const GfxTextureDesc& rhs);
	bool operator!=(const GfxTextureDesc& rhs);

	bool isArray() const
	{
		return type == TextureType::Tex2DArray || type == TextureType::Tex1DArray || type == TextureType::TexCubeArray;
	}

	Tuple2i getSize2D() const { return { int(width), int(height) }; }
	Tuple3i getSize3D() const { return { int(width), int(height), int(depth) }; }

	u32           width     = 0;
	u32           height    = 0;
	u32           depth     = 0;
	u32           mips      = 0;
	u32           samples   = 1;
	GfxFormat     format    = GfxFormat_Unknown;
	TextureType   type      = TextureType::Tex2D;
	GfxUsageFlags usage     = GfxUsageFlags::ShaderResource;
	const char*   debugName = nullptr;
};

enum class GfxRayTracingShaderType
{
	RayGen,
	Miss,
	HitGroup,
	Callable
};

enum class GfxRayTracingGeometryType
{
	Triangles,
	// Procedural,
};

struct GfxRayTracingInstanceDesc
{
	float transform[12];
	u32   instanceID : 24;
	u32   instanceMask : 8;
	u32   instanceContributionToHitGroupIndex : 24;
	u32   flags : 8;
	u64   accelerationStructureHandle;

	void init()
	{
		memset(this, 0, sizeof(*this));

		transform[0] = 1;
		transform[5] = 1;
		transform[10] = 1;

		instanceMask = 0xFF;
	}
};

enum class GfxAccelerationStructureType
{
	BottomLevel,
	TopLevel,
};

enum class GfxAccelerationStructureBuildFlags
{
	AllowUpdate     = 0x01,
	AllowCompaction = 0x02,
	FastTrace       = 0x04,
	FastBuild       = 0x08,
	LowMemory       = 0x10,
};

struct GfxRayTracingGeometryDesc
{
	GfxRayTracingGeometryType type = GfxRayTracingGeometryType::Triangles;

	GfxBuffer vertexBuffer;
	u32       vertexBufferOffset = 0;
	u32       vertexCount        = 0;
	u32       vertexStride       = 0;
	GfxFormat vertexFormat       = GfxFormat_Unknown;

	GfxBuffer indexBuffer;
	u32       indexBufferOffset = 0;
	u32       indexCount        = 0;
	GfxFormat indexFormat       = GfxFormat_Unknown;

	GfxBuffer transformBuffer;
	u32       transformBufferOffset = 0;

	bool isOpaque = false;
};

struct GfxAccelerationStructureDesc
{
	GfxAccelerationStructureType type = GfxAccelerationStructureType::BottomLevel;

	GfxRayTracingGeometryDesc* geometries   = nullptr;
	u32                        geometyCount = 0;

	u32                        instanceCount = 0;
};

struct GfxRayTracingPipelineDesc
{
	GfxShaderSource rayGen;
	GfxShaderSource miss;
	GfxShaderSource closestHit;
	GfxShaderSource anyHit;

	GfxShaderBindingDesc bindings;

	u32 maxRecursionDepth = 1;
};

inline u32 computeSubresourceCount(TextureType type, u32 mipCount, u32 layerCount)
{
	switch (type)
	{
	default: return mipCount * layerCount;
	case TextureType::TexCube:
	case TextureType::TexCubeArray: return mipCount * layerCount * 6;
	}
}

inline u32 computeSubresourceCount(const GfxTextureDesc& desc)
{
	return computeSubresourceCount(desc.type, desc.mips, desc.type == TextureType::Tex3D ? 1 : desc.depth);
}

inline u32 computeSubresourceIndex(u32 mip, u32 layer, u32 mipCount) { return mip + layer * mipCount; }

inline u32 computeSubresourceMip(u32 subresourceIndex, u32 mipCount) { return subresourceIndex % mipCount; }

inline u32 computeSubresourceSlice(u32 subresourceIndex, u32 mipCount, u32 sliceCount)
{
	return (subresourceIndex / mipCount) % sliceCount;
}

struct GfxDrawIndexedArg
{
	u32 indexCount;
	u32 instanceCount;
	u32 firstIndex;
	s32 vertexOffset;
	u32 firstInstance;
};

struct GfxDispatchArg
{
	u32 x, y, z;
};

enum GfxResourceState : u8
{
	GfxResourceState_Undefined = RUSH_GFX_RESOURCE_STATE_UNDEFINED,
	GfxResourceState_General = RUSH_GFX_RESOURCE_STATE_GENERAL,
	GfxResourceState_RenderTarget = RUSH_GFX_RESOURCE_STATE_RENDER_TARGET,
	GfxResourceState_DepthStencilTarget = RUSH_GFX_RESOURCE_STATE_DEPTH_STENCIL_TARGET,
	GfxResourceState_DepthStencilTargetReadOnly = RUSH_GFX_RESOURCE_STATE_DEPTH_STENCIL_TARGET_READ_ONLY,
	GfxResourceState_ShaderRead = RUSH_GFX_RESOURCE_STATE_SHADER_READ,
	GfxResourceState_TransferSrc = RUSH_GFX_RESOURCE_STATE_TRANSFER_SRC,
	GfxResourceState_TransferDst = RUSH_GFX_RESOURCE_STATE_TRANSFER_DST,
	GfxResourceState_Preinitialized = RUSH_GFX_RESOURCE_STATE_PREINITIALIZED,
	GfxResourceState_Present = RUSH_GFX_RESOURCE_STATE_PRESENT,
	GfxResourceState_SharedPresent = RUSH_GFX_RESOURCE_STATE_SHARED_PRESENT,
};

enum class GfxImageAspectFlags : u8
{
	Color    = RUSH_GFX_IMAGE_ASPECT_COLOR,
	Depth    = RUSH_GFX_IMAGE_ASPECT_DEPTH,
	Stencil  = RUSH_GFX_IMAGE_ASPECT_STENCIL,
	Metadata = RUSH_GFX_IMAGE_ASPECT_METADATA,

	DepthStencil = Depth | Stencil,
};
RUSH_IMPLEMENT_FLAG_OPERATORS(GfxImageAspectFlags, u8);

struct GfxSubresourceRange
{
	GfxImageAspectFlags aspectMask     = GfxImageAspectFlags::Color;
	u32                 baseMipLevel   = 0;
	u32                 levelCount     = 0;
	u32                 baseArrayLayer = 0;
	u32                 layerCount     = 0;
};

template <typename T> inline void GfxRef<T>::retain(GfxArg<T> h)
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

} // namespace Rush
