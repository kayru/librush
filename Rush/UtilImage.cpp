#include <cstring>

#include "UtilImage.h"

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
