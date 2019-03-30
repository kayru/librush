#pragma once

#include "Rush.h"

#include "GfxCommon.h"
#include "GfxPrimitiveBatch.h"
#include "UtilArray.h"

namespace Rush
{
class DataStream;

// Class for working with AngelCode BMFont files.
// More details here: http://www.angelcode.com/products/bmfont/
// Current simple implementation is only suitable for ASCI single-byte encoding.
class BitmapFontDesc
{
public:
	enum
	{
		MaxFontPages = 16,
		InvalidPage  = 0xFF
	};

	struct CharData
	{
		u16 x, y, width, height;
		s16 offsetX, offsetY, advanceX;
		u8  page, chan;
	};

	struct PageData
	{
		char filename[128];
	};

	BitmapFontDesc();

	bool read(DataStream& stream);

	CharData m_chars[256];
	PageData m_pages[MaxFontPages];
	u32      m_pageCount;
	u32      m_size;
};

struct BitmapFontData
{
	BitmapFontDesc    font;
	DynamicArray<u32> pixels;
	u32               width;
	u32               height;
};

#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
class BitmapFontRenderer
{
public:
	BitmapFontRenderer(const BitmapFontData& data);
	BitmapFontRenderer(const void* headerData, size_t headerSize, const void* pixelsData, size_t pixelsSize, u32 width,
	    u32 height, GfxFormat format);
	~BitmapFontRenderer();

	// returns position after the last drawn character
	Vec2 draw(PrimitiveBatch* batch, const Vec2& pos, const char* str, ColorRGBA8 col = ColorRGBA8::White(),
	    bool flush = true);

	// returns the width and height of the text block in pixels
	Vec2 measure(const char* str);

	static BitmapFontData createEmbeddedFont(bool shadow = true, u32 padX = 0, u32 padY = 0);

	void setScale(float scale);

private:
	void createSprites();

private:
	BitmapFontDesc               m_fontDesc;
	DynamicArray<GfxTexture>     m_textures;
	DynamicArray<GfxTextureDesc> m_textureDesc;
	TexturedQuad2D               m_chars[256];
	float                        m_scale;
};
#endif

// Draw characters into a 32bpp RGBA bitmap.
// Only code-page 437 characters ' ' (32) to '~' (126) are supported.
// Tabs, line breaks, etc.are not handled. An empty space will be emitted for unsupported characters.
void EmbeddedFont_Blit6x8(u32* output, u32 width, u32 Color, const char* str);
}
