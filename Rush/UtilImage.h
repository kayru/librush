#pragma once

#include "GfxCommon.h"
#include "Rush.h"
#include "UtilArray.h"
#include "UtilColor.h"

namespace Rush
{

struct ImageView
{
	const u8* data = nullptr;
	u32 width = 0;
	u32 height = 0;
	u32 bytesPerRow = 0;
	GfxFormat format = GfxFormat_Unknown;
};

template <typename Func>
void forRows(const ImageView& image, Func&& func)
{
	for (u32 y = 0; y < image.height; ++y)
	{
		const u8* row = image.data + static_cast<size_t>(image.bytesPerRow) * y;
		func(row, y);
	}
}

void convertToRGBA8(ImageView image, ArrayView<ColorRGBA8> output);

} // namespace Rush
