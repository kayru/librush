#include "GfxBitmapFont.h"

#include "GfxDevice.h"
#include "UtilFile.h"
#include "UtilLog.h"

#include <string.h>

namespace Rush
{

namespace
{
enum class BlockType
{
	Info    = 1,
	Common  = 2,
	Pages   = 3,
	Chars   = 4,
	Kerning = 5
};

#pragma pack(push)
#pragma pack(1)

struct InfoBlock
{
	s16  fontSize;
	u8   flags;
	u8   char_set;
	u16  stretch_h;
	u8   antialias;
	u8   paddingUp;
	u8   paddingRight;
	u8   paddingDown;
	u8   paddingLeft;
	u8   paddingHoriz;
	u8   paddingVert;
	u8   outline;
	char name[1];
};

struct CommonBlock
{
	u16 lineHeight;
	u16 base;
	u16 scaleWidth;
	u16 scaleHeight;
	u16 pages;
	u8  bitfield;
	u8  flags;
	u8  chanA;
	u8  chanR;
	u8  chanG;
	u8  chanB;
};

struct PagesBlock
{
	char names[1];
};

struct CharInfo
{
	u32 id;
	u16 x;
	u16 y;
	u16 width;
	u16 height;
	s16 offsetX;
	s16 offsetY;
	s16 advanceX;
	u8  page;
	u8  chan;
};

struct CharsBlock
{
	CharInfo chars[1];
};

struct KerningPair
{
	u32 first;
	u32 second;
	u16 amount;
};

struct KerningBlock
{
	KerningPair pairs[1];
};

#pragma pack(pop)
}

bool BitmapFontDesc::read(DataStream& stream)
{
	u32 magic;
	stream.read(&magic, 4);

	bool magicFound = false;

	auto makeFourCC = [](char a, char b, char c, char d) { return (a | (b << 8) | (c << 16) | (d << 24)); };

	u32 magicNativeEndian = makeFourCC('B', 'M', 'F', 3);

	if (magic == magicNativeEndian)
	{
		magicFound = true;
	}

	if (magicFound == false)
	{
		RUSH_LOG_ERROR("BitmapFontDesc::read() BMF file identifier not found.");
		return false;
	}

	InfoBlock*    info    = nullptr;
	CommonBlock*  common  = nullptr;
	PagesBlock*   pages   = nullptr;
	CharsBlock*   chars   = nullptr;
	KerningBlock* kerning = nullptr;

	u32 charCount = 0;
	// u32 kpairsCount = 0;
	u32 pageCount  = 0;
	u32 pageLength = 0;

	u8 type;
	while (stream.read(&type, 1))
	{
		u32 blockSize;
		stream.read(&blockSize, 4);

		BlockType blockType = (BlockType)type;

		switch (blockType)
		{
		case BlockType::Info:
			info = (InfoBlock*)allocateBytes(blockSize);
			if (info)
			{
				stream.read(info, blockSize);
			}
			break;
		case BlockType::Common:
			common = (CommonBlock*)allocateBytes(blockSize);
			if (common)
			{
				stream.read(common, blockSize);
			}
			break;
		case BlockType::Pages:
			pages = (PagesBlock*)allocateBytes(blockSize);
			if (pages)
			{
				stream.read(pages, blockSize);
				pageLength = (u32)strlen(pages->names);
				pageCount  = blockSize / (pageLength + 1);
			}
			break;
		case BlockType::Chars:
			chars = (CharsBlock*)allocateBytes(blockSize);
			if (chars)
			{
				stream.read(chars, blockSize);
				charCount = blockSize / sizeof(CharInfo);
			}
			break;
		case BlockType::Kerning:
			kerning = (KerningBlock*)allocateBytes(blockSize);
			if (kerning)
			{
				stream.read(kerning, blockSize);
				// kpairsCount = blockSize / sizeof(KerningPair);
			}
			break;
		}
	}

	if (info && pages && chars)
	{
		m_size = (u32)fabs((float)info->fontSize);

		// handle page info
		m_pageCount          = pageCount;
		u32 clamped_page_len = min<u32>(pageLength, sizeof(PageData) - 1);
		for (u32 i = 0; i < m_pageCount; ++i)
		{
			memcpy(m_pages[i].filename, (char*)pages + (pageLength + 1) * i, clamped_page_len);
			m_pages[i].filename[clamped_page_len] = 0;
		}

		// handle char into
		for (u32 i = 0; i < 256; ++i)
		{
			memset(&m_chars[i], 0, sizeof(CharData));
			m_chars[i].page = InvalidPage;
		}

		for (u32 i = 0; i < charCount; ++i)
		{
			u32 c = chars->chars[i].id;

			m_chars[c].x        = chars->chars[i].x;
			m_chars[c].y        = chars->chars[i].y;
			m_chars[c].width    = chars->chars[i].width;
			m_chars[c].height   = chars->chars[i].height;
			m_chars[c].offsetX  = chars->chars[i].offsetX;
			m_chars[c].offsetY  = chars->chars[i].offsetY;
			m_chars[c].advanceX = chars->chars[i].advanceX;
			m_chars[c].page     = chars->chars[i].page;
			m_chars[c].chan     = chars->chars[i].chan;
		}
	}
	else
	{
		RUSH_LOG_ERROR("BitmapFontDesc::read() Failed to load font data (info, pages or chars).");
	}

	deallocateBytes(info);
	deallocateBytes(common);
	deallocateBytes(pages);
	deallocateBytes(chars);
	deallocateBytes(kerning);

	return true;
}

BitmapFontDesc::BitmapFontDesc() : m_pageCount(0), m_size(0)
{
	memset(&m_pages, 0, sizeof(m_pages));
	memset(&m_chars, 0, sizeof(m_chars));
}

BitmapFontRenderer::BitmapFontRenderer(const BitmapFontData& data) : m_scale(1.0f)
{
	m_fontDesc = data.font;

	GfxTextureDesc td = GfxTextureDesc::make2D(data.width, data.height, GfxFormat_RGBA8_Unorm);
	m_textureDesc.push_back(td);

	auto th = Gfx_CreateTexture(td, data.pixels.data());

	m_textures.push_back(std::move(th));

	createSprites();
}

BitmapFontRenderer::BitmapFontRenderer(const void* headerData, size_t headerSize, const void* pixelsData,
    size_t pixelsSize, u32 width, u32 height, GfxFormat format)
: m_scale(1.0f)
{
	MemDataStream fi(headerData, (u32)headerSize);
	m_fontDesc.read(fi);

	GfxTextureDesc td = GfxTextureDesc::make2D(width, height, format);
	m_textureDesc.push_back(td);

	auto th = Gfx_CreateTexture(td, pixelsData);

	m_textures.push_back(std::move(th));

	createSprites();
}

BitmapFontRenderer::~BitmapFontRenderer()
{
}

Vec2 BitmapFontRenderer::draw(PrimitiveBatch* batch, const Vec2& pos, const char* str, ColorRGBA8 col, bool flush)
{
	if (flush)
	{
		batch->flush();
	}

	if (m_textures.size() == 0)
		return pos;

	float x      = pos.x;
	float y      = pos.y;
	float orig_x = x;

	while (*str)
	{
		u8 c = *str;
		if (c == '\n')
		{
			y += m_scale.y * (float)m_fontDesc.m_size;
			x = orig_x;
		}
		else
		{
			const BitmapFontDesc::CharData& data = m_fontDesc.m_chars[c];
			if (data.page != BitmapFontDesc::InvalidPage)
			{
				TexturedQuad2D q = m_chars[c];

				Vec2 offset(x, y);
				q.pos[0] = q.pos[0] * m_scale + offset;
				q.pos[1] = q.pos[1] * m_scale + offset;
				q.pos[2] = q.pos[2] * m_scale + offset;
				q.pos[3] = q.pos[3] * m_scale + offset;

				batch->setSampler(PrimitiveBatch::SamplerState_Point);
				batch->setTexture(m_textures[data.page]);
				batch->drawTexturedQuad(&q, col);
				x += m_scale.x * float(data.advanceX);
			}
		}
		++str;
	}

	if (flush)
	{
		batch->flush();
	}

	return Vec2(x, y);
}

Vec2 BitmapFontRenderer::measure(const char* str)
{
	if (m_textures.size() == 0)
		return Vec2(0, 0);

	float x      = 0;
	float y      = 0;
	float orig_x = x;
	float max_x  = 0;

	float char_w = 0;
	float char_h = float(m_fontDesc.m_size);

	while (*str)
	{
		u8 c = *str;
		if (c == '\n')
		{
			max_x = max(max_x, x);
			y += char_h * m_scale.y;
			x = orig_x;
		}
		else
		{
			const BitmapFontDesc::CharData& data = m_fontDesc.m_chars[c];
			if (data.page != BitmapFontDesc::InvalidPage)
			{
				char_w = (float)data.advanceX;
				x += char_w * m_scale.x;
			}
		}
		++str;
	}

	max_x = max(max_x, x);

	return Vec2(max_x, y + char_h * m_scale.y);
}

void BitmapFontRenderer::createSprites()
{
	for (u8 c = 0; c < 255; ++c)
	{
		const BitmapFontDesc::CharData& data = m_fontDesc.m_chars[c];
		if (data.page != BitmapFontDesc::InvalidPage)
		{
			GfxTextureDesc& textureDesc = m_textureDesc[data.page];
			Vec2            page_size(float(textureDesc.width), float(textureDesc.height));

			float w = float(data.width);
			float h = float(data.height);

			float px = float(data.offsetX);
			float py = float(data.offsetY);

			float tx = float(data.x) / page_size.x;
			float ty = float(data.y) / page_size.y;
			float tw = float(data.width) / page_size.x;
			float th = float(data.height) / page_size.y;

			TexturedQuad2D& q = m_chars[c];

			q.pos[0] = Vec2(px, py);
			q.pos[1] = Vec2(px + w, py);
			q.pos[2] = Vec2(px + w, py + h);
			q.pos[3] = Vec2(px, py + h);

			q.tex[0] = Vec2(tx, ty);
			q.tex[1] = Vec2(tx + tw, ty);
			q.tex[2] = Vec2(tx + tw, ty + th);
			q.tex[3] = Vec2(tx, ty + th);
		}
	}
}

BitmapFontData BitmapFontRenderer::createEmbeddedFont(bool shadow, u32 pad_x, u32 pad_y)
{
	const u16 charWidth  = u16(6 + pad_x);
	const u16 charHeight = u16(8 + pad_y);
	const u16 charCount  = '~' - ' ' + 1;

	const u16 glyphBorder = shadow ? 1 : 0;
	const u16 glyphWidth  = u16(charWidth + glyphBorder);
	const u16 glyphHeight = u16(charHeight + glyphBorder);

	BitmapFontData res;

	res.width  = nextPow2(glyphWidth * charCount);
	res.height = nextPow2(glyphHeight);

	res.pixels.resize(res.width * res.height, 0);

	for (u8 n = 0; n < charCount; ++n)
	{
		char c      = char(n + ' ');
		char str[2] = {c, 0};

		BitmapFontDesc::CharData charData;

		charData.x      = u16(glyphWidth * n);
		charData.y      = 0;
		charData.width  = glyphWidth;
		charData.height = glyphHeight;

		charData.offsetX  = 0;
		charData.offsetY  = 0;
		charData.advanceX = charWidth;
		charData.page     = 0;
		charData.chan     = 15;

		res.font.m_chars[(size_t)c] = charData;

		if (shadow)
		{
			EmbeddedFont_Blit6x8(&res.pixels[glyphWidth * n + 1], res.width, 0xFF000000, str);
			EmbeddedFont_Blit6x8(&res.pixels[glyphWidth * n + res.width + 1], res.width, 0xFF000000, str);
			EmbeddedFont_Blit6x8(&res.pixels[glyphWidth * n + res.width], res.width, 0xFF000000, str);
		}

		EmbeddedFont_Blit6x8(&res.pixels[glyphWidth * n], res.width, 0xFFFFFFFF, str);
	}

	res.font.m_pageCount = 1;
	res.font.m_size      = charHeight;

	return res;
}

// clang-format off
static const u32 g_embeddedFontBitmap6x8[] =
{
	0xffffffff, 0xffffffff, 0xfbf1f1fb, 0xfffbfffb, 0xfff6e4e4, 0xffffffff, 0xf5e0f5ff, 0xfff5e0f5,
	0xf9fef1fd, 0xfffbf8f7, 0xfbf7ecec, 0xffe6e6fd, 0xfdfafafd, 0xffe9f6ea, 0xfffdf9f9, 0xffffffff,
	0xfdfdfdfb, 0xfffbfdfd, 0xfbfbfbfd, 0xfffdfbfb, 0xe0f1f5ff, 0xfffff5f1, 0xe0fbfbff, 0xfffffbfb,
	0xffffffff, 0xfdf9f9ff, 0xe0ffffff, 0xffffffff, 0xffffffff, 0xfff9f9ff, 0xfbf7efff, 0xfffffefd,
	0xeae6eef1, 0xfff1eeec, 0xfbfbf9fb, 0xfff1fbfb, 0xf3efeef1, 0xffe0fefd, 0xf1efeef1, 0xfff1eeef,
	0xf6f5f3f7, 0xfff7f7e0, 0xf0fefee0, 0xfff1eeef, 0xf0fefdf3, 0xfff1eeee, 0xfbf7efe0, 0xfffdfdfd,
	0xf1eeeef1, 0xfff1eeee, 0xe1eeeef1, 0xfff9f7ef, 0xf9f9ffff, 0xfff9f9ff, 0xf9f9ffff, 0xfdf9f9ff,
	0xfefdfbf7, 0xfff7fbfd, 0xffe0ffff, 0xffffe0ff, 0xeff7fbfd, 0xfffdfbf7, 0xf3efeef1, 0xfffbfffb,
	0xeae2eef1, 0xfff1fee2, 0xeeeeeef1, 0xffeeeee0, 0xf0eeeef0, 0xfff0eeee, 0xfefeeef1, 0xfff1eefe,
	0xeeeeeef0, 0xfff0eeee, 0xf0fefee0, 0xffe0fefe, 0xf0fefee0, 0xfffefefe, 0xe2feeef1, 0xffe1eeee,
	0xe0eeeeee, 0xffeeeeee, 0xfbfbfbf1, 0xfff1fbfb, 0xefefefef, 0xfff1eeee, 0xfcfaf6ee, 0xffeef6fa,
	0xfefefefe, 0xffe0fefe, 0xeeeae4ee, 0xffeeeeee, 0xe6eaecee, 0xffeeeeee, 0xeeeeeef1, 0xfff1eeee,
	0xf0eeeef0, 0xfffefefe, 0xeeeeeef1, 0xffe9f6ea, 0xf0eeeef0, 0xffeeeef6, 0xf1feeef1, 0xfff1eeef,
	0xfbfbfbe0, 0xfffbfbfb, 0xeeeeeeee, 0xfff1eeee, 0xeeeeeeee, 0xfffbf5ee, 0xeaeaeeee, 0xfff5eaea,
	0xfbf5eeee, 0xffeeeef5, 0xf5eeeeee, 0xfffbfbfb, 0xfdfbf7f0, 0xfff0fefe, 0xfdfdfdf1, 0xfff1fdfd,
	0xfbfdfeff, 0xffffeff7, 0xf7f7f7f1, 0xfff1f7f7, 0xffeef5fb, 0xdfffffff, 0xffffffff, 0xe0ffffff,
	0xfffbf9f9, 0xffffffff, 0xeff1ffff, 0xffe1eee1, 0xeef0fefe, 0xfff0eeee, 0xeef1ffff, 0xfff1eefe,
	0xeee1efef, 0xffe1eeee, 0xeef1ffff, 0xfff1fef0, 0xf0fdfdf3, 0xfffdfdfd, 0xeee1ffff, 0xf1efe1ee,
	0xf6f8fefe, 0xfff6f6f6, 0xfbfbfffb, 0xfff3fbfb, 0xf7f3fff7, 0xf9f6f7f7, 0xfaf6fefe, 0xfff6fafc,
	0xfbfbfbfb, 0xfff3fbfb, 0xeaf4ffff, 0xffeeeeea, 0xf6f8ffff, 0xfff6f6f6, 0xeef1ffff, 0xfff1eeee,
	0xeef0ffff, 0xfef0eeee, 0xeee1ffff, 0xefe1eeee, 0xedf2ffff, 0xfff8fdfd, 0xfef1ffff, 0xfff1eff1,
	0xfdf0fdff, 0xfffbf5fd, 0xf6f6ffff, 0xfff5f2f6, 0xeeeeffff, 0xfffbf5ee, 0xeeeeffff, 0xfff5e0ea,
	0xf6f6ffff, 0xfff6f6f9, 0xf6f6ffff, 0xfcfbf1f6, 0xf7f0ffff, 0xfff0fef9, 0xfcfdfdf3, 0xfff3fdfd,
	0xfffbfbfb, 0xfffbfbfb, 0xe7f7f7f9, 0xfff9f7f7, 0xfffffaf5, 0xffffffff,
};
// clang-format on

namespace {
RUSH_FORCEINLINE u32 selectBit(u32 a, u32 b, u32 m) { return ((b ^ a) & m) ^ b; }

RUSH_FORCEINLINE void blitColumn6x8(u32* output, u32 color, u32 m, u32 b)
{
	output[0] = selectBit(output[0], color, u32(int(m << (31 - b)) >> 31));
	output[1] = selectBit(output[1], color, u32(int(m << (30 - b)) >> 31));
	output[2] = selectBit(output[2], color, u32(int(m << (29 - b)) >> 31));
	output[3] = selectBit(output[3], color, u32(int(m << (28 - b)) >> 31));
	output[4] = selectBit(output[4], color, u32(int(m << (27 - b)) >> 31));
	output[5] = selectBit(output[5], color, u32(int(m << (26 - b)) >> 31));
}
}

void EmbeddedFont_Blit6x8(u32* output, u32 width, u32 color, const char* str)
{
	while (*str)
	{
		int id = *str - ' ';
		if (id <= '~' - ' ')
		{
			u32 m0 = g_embeddedFontBitmap6x8[id * 2 + 0];
			u32 m1 = g_embeddedFontBitmap6x8[id * 2 + 1];
			blitColumn6x8(output + 0 * width, color, m0, 0);
			blitColumn6x8(output + 1 * width, color, m0, 8);
			blitColumn6x8(output + 2 * width, color, m0, 16);
			blitColumn6x8(output + 3 * width, color, m0, 24);
			blitColumn6x8(output + 4 * width, color, m1, 0);
			blitColumn6x8(output + 5 * width, color, m1, 8);
			blitColumn6x8(output + 6 * width, color, m1, 16);
			blitColumn6x8(output + 7 * width, color, m1, 24);
		}
		str += 1;
		output += 6;
	}
}
}
