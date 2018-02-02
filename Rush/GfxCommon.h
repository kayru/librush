#pragma once

#include "Rush.h"

#include "MathTypes.h"
#include "UtilResourcePool.h"
#include "UtilTuple.h"

#include <initializer_list>
#include <string>

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
#	ifdef RUSH_PLATFORM_MACOS
#		define RUSH_RENDER_API RUSH_RENDER_API_GL
#	endif // RUSH_PLATFORM_MACOS
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
#elif RUSH_RENDER_API == RUSH_RENDER_API_VK
#define RUSH_RENDER_API_NAME "Vulkan"
#define RUSH_RENDER_SUPPORT_IMAGE_BARRIERS
#define RUSH_RENDER_SUPPORT_ASYNC_COMPUTE
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
struct GfxComputeShaderDesc;
struct GfxTextureDesc;
struct GfxBufferDesc;
struct GfxSamplerDesc;
struct GfxBlendStateDesc;
struct GfxTechniqueDesc;
struct GfxDepthStencilDesc;
struct GfxRasterizerDesc;

typedef ResourceHandle<GfxVertexFormatDesc>  GfxVertexFormat;
typedef ResourceHandle<GfxVertexShaderDesc>  GfxVertexShader;
typedef ResourceHandle<GfxPixelShaderDesc>   GfxPixelShader;
typedef ResourceHandle<GfxComputeShaderDesc> GfxComputeShader;
typedef ResourceHandle<GfxTextureDesc>       GfxTexture;
typedef ResourceHandle<GfxBufferDesc>        GfxBuffer;
typedef ResourceHandle<GfxSamplerDesc>       GfxSampler;
typedef ResourceHandle<GfxBlendStateDesc>    GfxBlendState;
typedef ResourceHandle<GfxDepthStencilDesc>  GfxDepthStencilState;
typedef ResourceHandle<GfxRasterizerDesc>    GfxRasterizerState;
typedef ResourceHandle<GfxTechniqueDesc>     GfxTechnique;

enum class GfxContextType : u8
{
	Graphics, // Universal
	Compute,  // Async compute
	Transfer, // Async copy / DMA
	count
};

enum class TextureType : u8
{
	Tex1D,
	Tex1DArray,
	Tex2D,
	Tex2DArray,
	Tex3D,
	TexCube,
	TexCubeArray,
};

enum GfxShaderSourceType : u8
{
	GfxShaderSourceType_Unknown,
	GfxShaderSourceType_SPV,
	GfxShaderSourceType_GLSL,
	GfxShaderSourceType_HLSL,
	GfxShaderSourceType_DXBC,
	GfxShaderSourceType_DXIL,
	GfxShaderSourceType_Metal,
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
	GfxFormat_RGB8_Unorm   = GfxFormatStorage_RGB8 | GfxFormatType_Unorm | GfxFormatComponent_RGB,
	GfxFormat_RGBA16_Float = GfxFormatStorage_RGBA16 | GfxFormatType_Float | GfxFormatComponent_RGBA,
	GfxFormat_RGBA16_Unorm = GfxFormatStorage_RGB16 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_RGBA32_Float = GfxFormatStorage_RGBA32 | GfxFormatType_Float | GfxFormatComponent_RGBA,
	GfxFormat_RGBA8_Unorm  = GfxFormatStorage_RGBA8 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_BGRA8_Unorm  = GfxFormatStorage_BGRA8 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,

	// Block-compressed formats
	GfxFormat_BC3_Unorm      = GfxFormatStorage_BC3 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_BC3_Unorm_sRGB = GfxFormatStorage_BC3 | GfxFormatType_sRGB | GfxFormatComponent_RGBA,
	GfxFormat_BC4_Unorm      = GfxFormatStorage_BC4 | GfxFormatType_Unorm | GfxFormatComponent_R,
	GfxFormat_BC6H_UFloat    = GfxFormatStorage_BC6H | GfxFormatType_UFloat | GfxFormatComponent_RGB,
	GfxFormat_BC6H_SFloat    = GfxFormatStorage_BC6H | GfxFormatType_Float | GfxFormatComponent_RGB,
	GfxFormat_BC7_Unorm      = GfxFormatStorage_BC7 | GfxFormatType_Unorm | GfxFormatComponent_RGBA,
	GfxFormat_BC7_Unorm_sRGB = GfxFormatStorage_BC7 | GfxFormatType_sRGB | GfxFormatComponent_RGBA,
};

enum class GfxUsageFlags : u8
{
	None = 0,

	ShaderResource = 1 << 0,
	RenderTarget   = 1 << 1,
	DepthStencil   = 1 << 2,
	StorageImage   = 1 << 3,

	RenderTarget_ShaderResource = ShaderResource | RenderTarget,
	DepthStencil_ShaderResource = ShaderResource | DepthStencil,
};

RUSH_IMPLEMENT_FLAG_OPERATORS(GfxUsageFlags, u8);

enum class GfxStage : u8
{
	Vertex,
	Pixel,
	Hull,
	Domain,
	Compute,
	count
};

enum class GfxStageFlags : u8
{
	Vertex  = 1 << (u32)GfxStage::Vertex,
	Pixel   = 1 << (u32)GfxStage::Pixel,
	Hull    = 1 << (u32)GfxStage::Hull,
	Domain  = 1 << (u32)GfxStage::Domain,
	Compute = 1 << (u32)GfxStage::Compute,
};

RUSH_IMPLEMENT_FLAG_OPERATORS(GfxStageFlags, u8);

enum class GfxPrimitive : u8
{
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip,
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

enum class GfxBufferType : u8
{
	Vertex,
	Index,
	Constant,
	Storage,
	Texel,
	IndirectArgs,

	count
};

enum class GfxBufferMode : u8
{
	Static,    // persistent GPU memory
	Temporary, // frame-scoped GPU memory

	count
};

enum class GfxShaderType : u8
{
	Vertex,
	Pixel,
	Geometry,
	Hull,
	Domain,
	Compute
};

enum class GfxBlendParam : u8
{
	Zero,
	One,
	SrcColor,
	InvSrcColor,
	SrcAlpha,
	InvSrcAlpha,
	DestAlpha,
	InvDestAlpha,
	DestColor,
	InvDestColor,

	count
};

enum class GfxBlendOp : u8
{
	Add,
	Subtract,
	RevSubtract,
	Min,
	Max,

	count
};

enum class GfxTextureFilter : u8
{
	Point,
	Linear,
	Anisotropic,

	count
};

enum class GfxTextureWrap : u8
{
	Wrap,
	Mirror,
	Clamp,

	count
};

enum class GfxCompareFunc : u8
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always,

	count
};

enum class GfxFillMode : u8
{
	Solid,
	Wireframe
};

enum class GfxCullMode : u8
{
	None,
	CW,
	CCW,
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
	GfxBufferDesc(GfxBufferType _type, GfxBufferMode _mode, GfxFormat _format, u32 _count, u32 _stride);
	GfxBufferDesc(GfxBufferType _type, GfxBufferMode _mode, u32 _count, u32 _stride);

	GfxBufferType type        = GfxBufferType::Vertex;
	GfxBufferMode mode        = GfxBufferMode::Static;
	GfxFormat     format      = GfxFormat_Unknown;
	u32           stride      = 0;
	u32           count       = 0;
	bool          hostVisible = false;
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

	float anisotropy = 1.0;

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
	GfxCullMode cullMode            = GfxCullMode::CCW;
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
		Unused       = 0,
		Position     = 1,
		Texcoord     = 2,
		Color        = 3,
		Normal       = 4,
		TangentU     = 5,
		TangentV     = 6,
		InstanceData = 7,
		BoneIndex    = 8,
		BoneWeight   = 9,

		Tangent   = TangentU, // backwards-compatible name
		Bitangent = TangentV, // backwards-compatible name
	};

	struct Element
	{
		Element() {}
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

	u16 m_streamOffset[MaxStreams];

	bool m_hasPosition;
	bool m_hasNormal;
	bool m_hasColor;
};

const char* toString(GfxVertexFormatDesc::Semantic type);

u16 dataTypeSize(GfxVertexFormatDesc::DataType type);

struct ShaderConstantIndex
{
	inline ShaderConstantIndex() : index(u32(-1)), count(u32(-1)) {}

	inline ShaderConstantIndex(u32 _index, u32 _count = 1) : index(_index), count(_count) {}

	bool isValid() const { return index != u32(-1); }

	u32 index;
	u32 count;
};

struct ShaderConstantDesc
{
	std::string name;
	u32         registerIndex;
	u32         registerCount;

	ShaderConstantDesc() : name(), registerIndex(0), registerCount(0) {}

	inline bool operator==(const ShaderConstantDesc& rhs) const
	{
		return registerIndex == rhs.registerIndex && registerCount == rhs.registerCount && name == rhs.name;
	}
};

struct GfxShaderSource : public std::vector<char>
{
	GfxShaderSource() : type(GfxShaderSourceType_Unknown) {}
	GfxShaderSource(GfxShaderSourceType _type, const char* _code, size_t _size = 0, const char* _entry = "main");
	GfxShaderSourceType type;
	const char*         entry = "main";
};

enum GfxBindingType
{
	GfxBindingType_Unknown,

	// Bindings must be specified in the exact order:
	GfxBindingType_PushConstants,
	GfxBindingType_ConstantBuffer,
	GfxBindingType_CombinedSampler, // sampler & texture in the same slot (Vulkan only)
	GfxBindingType_Sampler,
	GfxBindingType_Texture,
	GfxBindingType_RWImage,
	GfxBindingType_RWBuffer,
	GfxBindingType_RWTypedBuffer,

	// Loose constant bindings (OpenGL only)
	GfxBindingType_Int,
	GfxBindingType_Scalar,
	GfxBindingType_Vec2,
	GfxBindingType_Vec3,
	GfxBindingType_Vec4,
	GfxBindingType_Matrix,
};

struct GfxShaderBindings
{
	enum BindingType : u8 // TODO: deprecate this in favor of using GfxBindingType directly
	{
		BindingType_Unknown = GfxBindingType_Unknown,

		// Bindings must be specified in the exact order:
		BindingType_PushConstants  = GfxBindingType_PushConstants,
		BindingType_ConstantBuffer = GfxBindingType_ConstantBuffer,
		BindingType_CombinedSampler =
		    GfxBindingType_CombinedSampler, // sampler & texture in the same slot (Vulkan only)
		BindingType_Sampler       = GfxBindingType_Sampler,
		BindingType_Texture       = GfxBindingType_Texture,
		BindingType_RWImage       = GfxBindingType_RWImage,
		BindingType_RWBuffer      = GfxBindingType_RWBuffer,
		BindingType_RWTypedBuffer = GfxBindingType_RWTypedBuffer,

		// Loose constant bindings (OpenGL only)
		BindingType_Int    = GfxBindingType_Int,
		BindingType_Scalar = GfxBindingType_Scalar,
		BindingType_Vec2   = GfxBindingType_Vec2,
		BindingType_Vec3   = GfxBindingType_Vec3,
		BindingType_Vec4   = GfxBindingType_Vec4,
		BindingType_Matrix = GfxBindingType_Matrix,

		// Backwards-compatibility aliases
		BindingType_StorageImage  = GfxBindingType_RWImage,
		BindingType_StorageBuffer = GfxBindingType_RWBuffer
	};

	struct Item
	{
		const char* name;
		const void* data;
		union {
			struct
			{
				u32           size;
				GfxStageFlags stageFlags;
			} pushConstants;
			struct
			{
				u32 count;
				u32 idx;
			};
		};

		BindingType type;
	};

	bool addConstant(const char* name, const int* data, u32 count = 1);
	bool addConstant(const char* name, const float* data, u32 count = 1);
	bool addConstant(const char* name, const Vec2* data, u32 count = 1);
	bool addConstant(const char* name, const Vec3* data, u32 count = 1);
	bool addConstant(const char* name, const Vec4* data, u32 count = 1);
	bool addConstant(const char* name, const Mat4* data, u32 count = 1);
	bool addConstantBuffer(const char* name, u32 idx);
	// TODO: replace with explicit separate / combined sampler declaration API (leaving this for back-compat)
	bool        addSampler(const char* name, u32 idx, bool combined = true);
	inline bool addCombinedSampler(const char* name, u32 idx) { return addSampler(name, idx, true); }
	inline bool addSeparateSampler(const char* name, u32 idx) { return addSampler(name, idx, false); }
	bool        addTexture(const char* name, u32 idx);
	bool        addStorageImage(const char* name, u32 idx);
	bool        addStorageBuffer(const char* name, u32 idx) { return addRWBuffer(name, idx); }
	bool        addPushConstants(const char* name, GfxStageFlags stageFlags, u32 size);

	bool addRWBuffer(const char* name, u32 idx);
	bool addTypedRWBuffer(const char* name, u32 idx);

	StaticArray<Item, 128> items;
};

struct GfxSpecializationConstant
{
	u32    id;
	u32    offset;
	size_t size;
};

struct GfxTechniqueDesc
{
	GfxTechniqueDesc(
	    GfxPixelShader _ps, GfxVertexShader _vs, GfxVertexFormat _vf, const GfxShaderBindings* _bindings = nullptr)
	: ps(_ps), vs(_vs), vf(_vf), bindings(_bindings)
	{
	}

	GfxTechniqueDesc(GfxComputeShader _cs, const GfxShaderBindings* _bindings = nullptr) : cs(_cs), bindings(_bindings)
	{
	}

	GfxComputeShader         cs;
	GfxPixelShader           ps;
	GfxVertexShader          vs;
	GfxVertexFormat          vf;
	const GfxShaderBindings* bindings = nullptr;

	u32                              specializationConstantCount = 0;
	const GfxSpecializationConstant* specializationConstants     = nullptr;
	const void*                      specializationData          = nullptr;
	u32                              specializationDataSize      = 0;

	float waveLimits[u32(GfxStage::count)] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};

struct GfxTextureDesc
{
	static GfxTextureDesc make2D(u32 _width, u32 _height, GfxFormat _format = GfxFormat_RGBA8_Unorm,
	    GfxUsageFlags _usage = GfxUsageFlags::ShaderResource);

	static GfxTextureDesc make3D(u32 _width, u32 _height, u32 _depth, GfxFormat _format = GfxFormat_RGBA8_Unorm,
	    GfxUsageFlags _usage = GfxUsageFlags::ShaderResource);

	static GfxTextureDesc makeCube(
	    u32 _size, GfxFormat _format = GfxFormat_RGBA8_Unorm, GfxUsageFlags _usage = GfxUsageFlags::ShaderResource);

	bool operator==(const GfxTextureDesc& rhs);
	bool operator!=(const GfxTextureDesc& rhs);

	u32 getPixelCount() const { return width * height * depth; }

	u32 getSizeBytes() const { return getPixelCount() * getBitsPerPixel(format) / 8; }

	bool isArray() const
	{
		return type == TextureType::Tex2DArray || type == TextureType::Tex1DArray || type == TextureType::TexCubeArray;
	}

	u32           width     = 0;
	u32           height    = 0;
	u32           depth     = 0;
	u32           mips      = 0;
	GfxFormat     format    = GfxFormat_Unknown;
	TextureType   type      = TextureType::Tex2D;
	GfxUsageFlags usage     = GfxUsageFlags::ShaderResource;
	const char*   debugName = nullptr;
};

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
	GfxResourceState_Undefined,
	GfxResourceState_General,
	GfxResourceState_RenderTarget,
	GfxResourceState_DepthStencilTarget,
	GfxResourceState_DepthStencilTargetReadOnly,
	GfxResourceState_ShaderRead,
	GfxResourceState_TransferSrc,
	GfxResourceState_TransferDst,
	GfxResourceState_Preinitialized,
	GfxResourceState_Present,
	GfxResourceState_SharedPresent,
};

enum class GfxImageAspectFlags : u8
{
	Color    = 1 << 0,
	Depth    = 1 << 1,
	Stencil  = 1 << 2,
	Metadata = 1 << 3,

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
}
