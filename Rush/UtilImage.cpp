#include <cstring>

#include "UtilImage.h"
#include "MathHalf.h"
#include "MathCommon.h"

namespace Rush
{

void convertToRGBA8(ImageView image, ArrayView<ColorRGBA8> output)
{
	const size_t pixelCount = static_cast<size_t>(image.width) * image.height;
	RUSH_ASSERT(output.size() >= pixelCount);

	const GfxFormatStorage storage = getGfxFormatStorage(image.format);

	if (storage == GfxFormatStorage_RGBA8)
	{
		forRows(image, [&](const u8* row, u32 y) {
			const size_t dstIndex = static_cast<size_t>(y) * image.width;
			std::memcpy(output.data() + dstIndex, row, image.width * sizeof(ColorRGBA8));
		});
		return;
	}

	if (storage == GfxFormatStorage_RGBA16 && getGfxFormatType(image.format) == GfxFormatType_Float)
	{
		forRows(image, [&](const u8* row, u32 y) {
			const u16* src = reinterpret_cast<const u16*>(row);
			for (u32 x = 0; x < image.width; ++x)
			{
				u8 channels[4];
				for (u32 c = 0; c < 4; ++c)
				{
					const float v = halfToFloat(src[x * 4 + c]);
					channels[c] = u8(clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
				}
				output[static_cast<size_t>(y) * image.width + x] =
				    ColorRGBA8(channels[0], channels[1], channels[2], channels[3]);
			}
		});
		return;
	}

	if (storage == GfxFormatStorage_RGB10A2)
	{
		forRows(image, [&](const u8* row, u32 y) {
			const u32* src = reinterpret_cast<const u32*>(row);
			for (u32 x = 0; x < image.width; ++x)
			{
				const u32 v = src[x];
				const u8 r = u8((((v >> 0) & 0x3FF) * 255 + 511) / 1023);
				const u8 g = u8((((v >> 10) & 0x3FF) * 255 + 511) / 1023);
				const u8 b = u8((((v >> 20) & 0x3FF) * 255 + 511) / 1023);
				const u8 a = u8(((v >> 30) & 0x3) * 85);
				output[static_cast<size_t>(y) * image.width + x] = ColorRGBA8(r, g, b, a);
			}
		});
		return;
	}

	if (storage != GfxFormatStorage_BGRA8)
	{
		RUSH_LOG_ERROR("convertToRGBA8 does not support conversion from %u",
		    static_cast<u32>(image.format));
		return;
	}

	forRows(image, [&](const u8* row, u32 y) {
		for (u32 x = 0; x < image.width; ++x)
		{
			const size_t srcIndex = static_cast<size_t>(x) * 4;
			const size_t dstIndex = static_cast<size_t>(y) * image.width + x;
			const u8 b = row[srcIndex + 0];
			const u8 g = row[srcIndex + 1];
			const u8 r = row[srcIndex + 2];
			const u8 a = row[srcIndex + 3];
			output[dstIndex] = ColorRGBA8(r, g, b, a);
		}
	});
}

} // namespace Rush
