#include "GfxCommon.h"
#include "UtilLog.h"
#include <string.h>

namespace Rush
{
u32 getBitsPerPixel(GfxFormat fmt) { return getBitsPerPixel(getGfxFormatStorage(fmt)); }

u32 getBitsPerPixel(GfxFormatStorage fmt)
{
	switch (fmt)
	{
	case GfxFormatStorage_BC1: return 4;
	case GfxFormatStorage_BC2: return 8;
	case GfxFormatStorage_BC3: return 8;
	case GfxFormatStorage_BC4: return 4;
	case GfxFormatStorage_BC5: return 8;
	case GfxFormatStorage_BC6H: return 8;
	case GfxFormatStorage_BC7: return 8;
	case GfxFormatStorage_R16: return 16;
	case GfxFormatStorage_R24G8: return 32;
	case GfxFormatStorage_R32: return 32;
	case GfxFormatStorage_R32G8X24: return 64;
	case GfxFormatStorage_R8: return 8;
	case GfxFormatStorage_RG16: return 32;
	case GfxFormatStorage_RG32: return 64;
	case GfxFormatStorage_RG8: return 16;
	case GfxFormatStorage_RGB16: return 48;
	case GfxFormatStorage_RGB32: return 96;
	case GfxFormatStorage_RGB8: return 24;
	case GfxFormatStorage_RGBA16: return 64;
	case GfxFormatStorage_RGBA32: return 128;
	case GfxFormatStorage_RGBA8: return 32;
	case GfxFormatStorage_BGRA8: return 32;

	default: RUSH_BREAK; return 0;
	}
}

const char* toString(GfxFormat fmt)
{
	switch (fmt)
	{
	case GfxFormat_Unknown: return "GfxFormat_Unknown";
	case GfxFormat_D24_Unorm_S8_Uint: return "GfxFormat_D24_Unorm_S8_Uint";
	case GfxFormat_D24_Unorm_X8: return "GfxFormat_D24_Unorm_X8";
	case GfxFormat_D32_Float: return "GfxFormat_D32_Float";
	case GfxFormat_D32_Float_S8_Uint: return "GfxFormat_D32_Float_S8_Uint";
	case GfxFormat_R8_Unorm: return "GfxFormat_R8_Unorm";
	case GfxFormat_R16_Float: return "GfxFormat_R16_Float";
	case GfxFormat_R16_Uint: return "GfxFormat_R16_Uint";
	case GfxFormat_R32_Float: return "GfxFormat_R32_Float";
	case GfxFormat_R32_Uint: return "GfxFormat_R32_Uint";
	case GfxFormat_RG8_Unorm: return "GfxFormat_RG8_Unorm";
	case GfxFormat_RG16_Float: return "GfxFormat_RG16_Float";
	case GfxFormat_RG32_Float: return "GfxFormat_RG32_Float";
	case GfxFormat_RGB8_Unorm: return "GfxFormat_RGB8_Unorm";
	case GfxFormat_RGBA16_Float: return "GfxFormat_RGBA16_Float";
	case GfxFormat_RGBA16_Unorm: return "GfxFormat_RGBA16_Unorm";
	case GfxFormat_RGBA32_Float: return "GfxFormat_RGBA32_Float";
	case GfxFormat_RGBA8_Unorm: return "GfxFormat_RGBA8_Unorm";
	case GfxFormat_RGBA8_sRGB: return "GfxFormat_RGBA8_sRGB";
	case GfxFormat_BGRA8_Unorm: return "GfxFormat_BGRA8_Unorm";
	case GfxFormat_BGRA8_sRGB: return "GfxFormat_BGRA8_sRGB";
	case GfxFormat_BC1_Unorm: return "GfxFormat_BC1_Unorm";
	case GfxFormat_BC1_Unorm_sRGB: return "GfxFormat_BC1_Unorm_sRGB";
	case GfxFormat_BC3_Unorm: return "GfxFormat_BC3_Unorm";
	case GfxFormat_BC3_Unorm_sRGB: return "GfxFormat_BC3_Unorm_sRGB";
	case GfxFormat_BC4_Unorm: return "GfxFormat_BC4_Unorm";
	case GfxFormat_BC5_Unorm: return "GfxFormat_BC5_Unorm";
	case GfxFormat_BC6H_UFloat: return "GfxFormat_BC6H_UFloat";
	case GfxFormat_BC6H_SFloat: return "GfxFormat_BC6H_SFloat";
	case GfxFormat_BC7_Unorm: return "GfxFormat_BC7_Unorm";
	case GfxFormat_BC7_Unorm_sRGB: return "GfxFormat_BC7_Unorm_sRGB";
	default: RUSH_BREAK; return "GfxFormat_Unknown";
	}
}

const char* toStringShort(GfxFormat fmt)
{
	return toString(fmt) + 10;
}

const char* toString(GfxFormatStorage storage)
{
	switch (storage)
	{
	case GfxFormatStorage_BC1: return "BC1";
	case GfxFormatStorage_BC2: return "BC2";
	case GfxFormatStorage_BC3: return "BC3";
	case GfxFormatStorage_BC4: return "BC4";
	case GfxFormatStorage_BC5: return "BC5";
	case GfxFormatStorage_BC6H: return "BC6H";
	case GfxFormatStorage_BC7: return "BC7";
	case GfxFormatStorage_R16: return "R16";
	case GfxFormatStorage_R24G8: return "R24G8";
	case GfxFormatStorage_R32: return "R32";
	case GfxFormatStorage_R32G8X24: return "R32G8X24";
	case GfxFormatStorage_R8: return "R8";
	case GfxFormatStorage_RG16: return "RG16";
	case GfxFormatStorage_RG32: return "RG32";
	case GfxFormatStorage_RG8: return "RG8";
	case GfxFormatStorage_RGB16: return "RGB16";
	case GfxFormatStorage_RGB32: return "RGB32";
	case GfxFormatStorage_RGB8: return "RGB8";
	case GfxFormatStorage_RGBA16: return "RGBA16";
	case GfxFormatStorage_RGBA32: return "RGBA32";
	case GfxFormatStorage_RGBA8: return "RGBA8";
	case GfxFormatStorage_BGRA8: return "BGRA8";

	default: RUSH_BREAK; return "Unknown";
	}
}

const char* toString(GfxFormatType type)
{
	switch (type)
	{
	case GfxFormatType_Float: return "Float";
	case GfxFormatType_Sint: return "Sint";
	case GfxFormatType_Snorm: return "Snorm";
	case GfxFormatType_sRGB: return "sRGB";
	case GfxFormatType_Typeless: return "Typeless";
	case GfxFormatType_Typeless_Uint: return "Typeless_Uint";
	case GfxFormatType_Ufloat: return "Ufloat";
	case GfxFormatType_Uint: return "Uint";
	case GfxFormatType_Unorm: return "Unorm";
	case GfxFormatType_Unorm_Typeless: return "Unorm_Typeless";
	case GfxFormatType_Unorm_Uint: return "Unorm_Uint";

	default: RUSH_BREAK; return "Unknown";
	}
}

const char* toString(GfxVertexFormatDesc::Semantic type)
{
	switch (type)
	{
	case GfxVertexFormatDesc::Semantic::Position: return "Position";
	case GfxVertexFormatDesc::Semantic::Texcoord: return "Texcoord";
	case GfxVertexFormatDesc::Semantic::Color: return "Color";
	case GfxVertexFormatDesc::Semantic::Normal: return "Normal";
	case GfxVertexFormatDesc::Semantic::TangentU: return "TangentU";
	case GfxVertexFormatDesc::Semantic::TangentV: return "TangentV";
	case GfxVertexFormatDesc::Semantic::InstanceData: return "InstanceData";
	case GfxVertexFormatDesc::Semantic::BoneIndex: return "BoneIndex";
	case GfxVertexFormatDesc::Semantic::BoneWeight: return "BoneWeight";

	default: RUSH_BREAK; return "Unknown";
	}
}

u16 dataTypeSize(GfxVertexFormatDesc::DataType type)
{
	switch (type)
	{
	case GfxVertexFormatDesc::DataType::Float1: return 4;
	case GfxVertexFormatDesc::DataType::Float2: return 8;
	case GfxVertexFormatDesc::DataType::Float3: return 12;
	case GfxVertexFormatDesc::DataType::Float4: return 16;
	case GfxVertexFormatDesc::DataType::Half2: return 4;
	case GfxVertexFormatDesc::DataType::Half4: return 8;
	case GfxVertexFormatDesc::DataType::Short2: return 4;
	case GfxVertexFormatDesc::DataType::Short2N: return 4;
	case GfxVertexFormatDesc::DataType::UByte4: return 4;
	case GfxVertexFormatDesc::DataType::Dec3N: return 4;
	case GfxVertexFormatDesc::DataType::Color: return 4;
	case GfxVertexFormatDesc::DataType::UInt: return 4;
	default: return 0;
	}
}

GfxVertexFormatDesc::GfxVertexFormatDesc() : m_hasPosition(false), m_hasNormal(false), m_hasColor(false)
{
	for (u32 i = 0; i < MaxElements; ++i)
	{
		m_elements[i] = Element(0, DataType::Unused, Semantic::Unused, 0);
	}
}

void GfxVertexFormatDesc::add(Element element)
{
	if (element.semantic == Semantic::Position)
		m_hasPosition = true;
	if (element.semantic == Semantic::Normal)
		m_hasNormal = true;
	if (element.semantic == Semantic::Color)
		m_hasColor = true;

	element.offset = m_streamOffset[element.stream];

	m_streamOffset[element.stream] = u16(m_streamOffset[element.stream] + element.size);

	m_elements.pushBack(element);
}

bool GfxVertexFormatDesc::operator==(const GfxVertexFormatDesc& rhs) const
{
	if (elementCount() == rhs.elementCount())
	{
		for (u32 i = 0; i < elementCount(); ++i)
		{
			if (!(m_elements[i] == rhs.m_elements[i]))
				return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

GfxVertexFormatDesc::Element::Element(u16 _stream, DataType _type, Semantic _semantic, u8 _index)
: stream(_stream), size(dataTypeSize(_type)), offset(0), index(_index), type(_type), semantic(_semantic)
{
}

GfxBlendStateDesc GfxBlendStateDesc::makeOpaque()
{
	GfxBlendStateDesc res;

	res.enable        = false;
	res.src           = GfxBlendParam::One;
	res.dst           = GfxBlendParam::Zero;
	res.op            = GfxBlendOp::Add;
	res.alphaSeparate = false;
	res.alphaSrc      = GfxBlendParam::One;
	res.alphaDst      = GfxBlendParam::Zero;
	res.alphaOp       = GfxBlendOp::Add;

	return res;
}

GfxBlendStateDesc GfxBlendStateDesc::makeLerp()
{
	GfxBlendStateDesc res;

	res.enable        = true;
	res.src           = GfxBlendParam::SrcAlpha;
	res.dst           = GfxBlendParam::InvSrcAlpha;
	res.op            = GfxBlendOp::Add;
	res.alphaSeparate = false;
	res.alphaSrc      = GfxBlendParam::SrcAlpha;
	res.alphaDst      = GfxBlendParam::InvSrcAlpha;
	res.alphaOp       = GfxBlendOp::Add;

	return res;
}

GfxBlendStateDesc GfxBlendStateDesc::makeAdditive()
{
	GfxBlendStateDesc res;

	res.enable        = true;
	res.src           = GfxBlendParam::One;
	res.dst           = GfxBlendParam::One;
	res.op            = GfxBlendOp::Add;
	res.alphaSeparate = false;
	res.alphaSrc      = GfxBlendParam::One;
	res.alphaDst      = GfxBlendParam::One;
	res.alphaOp       = GfxBlendOp::Add;

	return res;
}

GfxBlendStateDesc GfxBlendStateDesc::makePremultiplied()
{
	GfxBlendStateDesc res;

	res.enable        = true;
	res.src           = GfxBlendParam::One;
	res.dst           = GfxBlendParam::InvSrcAlpha;
	res.op            = GfxBlendOp::Add;
	res.alphaSeparate = true;
	res.alphaSrc      = GfxBlendParam::One;
	res.alphaDst      = GfxBlendParam::One;
	res.alphaOp       = GfxBlendOp::Add;

	return res;
}

GfxSamplerDesc GfxSamplerDesc::makeLinear()
{
	GfxSamplerDesc res;

	res.filterMin = GfxTextureFilter::Linear;
	res.filterMag = GfxTextureFilter::Linear;
	res.filterMip = GfxTextureFilter::Linear;

	return res;
}

GfxSamplerDesc GfxSamplerDesc::makePoint()
{
	GfxSamplerDesc res;

	res.filterMin = GfxTextureFilter::Point;
	res.filterMag = GfxTextureFilter::Point;
	res.filterMip = GfxTextureFilter::Point;

	return res;
}

GfxViewport::GfxViewport() : x(0), y(0), w(1), h(1), depthMin(0), depthMax(1) {}

GfxViewport::GfxViewport(Tuple2i size) : x(0), y(0), w((float)size.x), h((float)size.y), depthMin(0), depthMax(1) {}

GfxViewport::GfxViewport(Tuple2u size) : x(0), y(0), w((float)size.x), h((float)size.y), depthMin(0), depthMax(1) {}

GfxViewport::GfxViewport(const Box2& bounds, float _depth_min, float _depth_max)
: x(bounds.tl().x), y(bounds.tl().y), w(bounds.width()), h(bounds.height()), depthMin(_depth_min), depthMax(_depth_max)
{
}

GfxViewport::GfxViewport(float _x, float _y, float _w, float _h, float _depth_min, float _depth_max)
: x(_x), y(_y), w(_w), h(_h), depthMin(_depth_min), depthMax(_depth_max)
{
}

bool GfxViewport::operator==(const GfxViewport& rhs) const
{
	return x == rhs.x && y == rhs.y && w == rhs.w && h == rhs.h && depthMin == rhs.depthMin && depthMax == rhs.depthMax;
}

bool GfxViewport::operator!=(const GfxViewport& rhs) const { return !(*this == rhs); }

GfxBufferDesc::GfxBufferDesc(GfxBufferFlags _flags, GfxFormat _format, u32 _count, u32 _stride)
: flags(_flags), format(_format), stride(_stride), count(_count)
{
}

GfxBufferDesc::GfxBufferDesc(GfxBufferFlags _flags, u32 _count, u32 _stride)
: flags(_flags), format(GfxFormat_Unknown), stride(_stride), count(_count)
{
}

GfxShaderSource::GfxShaderSource(GfxShaderSourceType _type, const char* _code, size_t _size, const char* _entry)
: type(_type), entry(_entry)
{
	if ((_type == GfxShaderSourceType_GLSL || _type == GfxShaderSourceType_HLSL ||
	        _type == GfxShaderSourceType_MSL) &&
	    _size == 0)
	{
		_size = (size_t)strlen(_code) + 1;
		this->resize(_size);
		memcpy(this->data(), _code, _size);
	}
	else
	{
		this->resize(_size);
		memcpy(this->data(), _code, _size);
	}
}

bool GfxTextureDesc::operator==(const GfxTextureDesc& rhs)
{
	return width == rhs.width && height == rhs.height && depth == rhs.depth && mips == rhs.mips &&
	       format == rhs.format && type == rhs.type && usage == rhs.usage;
}

bool GfxTextureDesc::operator!=(const GfxTextureDesc& rhs) { return !(*this == rhs); }

GfxTextureDesc GfxTextureDesc::make2D(u32 _width, u32 _height, GfxFormat _format, GfxUsageFlags _usage)
{
	GfxTextureDesc res;
	res.width  = _width;
	res.height = _height;
	res.depth  = 1;
	res.mips   = 1;
	res.format = _format;
	res.type   = TextureType::Tex2D;
	res.usage  = _usage;
	return res;
}

GfxTextureDesc GfxTextureDesc::make3D(u32 _width, u32 _height, u32 _depth, GfxFormat _format, GfxUsageFlags _usage)
{
	GfxTextureDesc res;
	res.width  = _width;
	res.height = _height;
	res.depth  = _depth;
	res.mips   = 1;
	res.format = _format;
	res.type   = TextureType::Tex3D;
	res.usage  = _usage;
	return res;
}

GfxTextureDesc GfxTextureDesc::makeCube(u32 _size, GfxFormat _format, GfxUsageFlags _usage)
{
	GfxTextureDesc res;
	res.width  = _size;
	res.height = _size;
	res.depth  = 1;
	res.mips   = 1;
	res.format = _format;
	res.type   = TextureType::TexCube;
	res.usage  = _usage;
	return res;
}
}
