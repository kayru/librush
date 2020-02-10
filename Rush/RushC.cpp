#include "RushC.h"
#include "Platform.h"
#include "GfxDevice.h"

#include <stdio.h>
#include <string.h>

using namespace Rush;

namespace {

rush_app_config convert(const AppConfig* cfg)
{
    rush_app_config result;
    result.name = cfg->name;
    result.vsync = cfg->vsync;
    result.width = cfg->width;
    result.height = cfg->height;
    result.max_width = cfg->maxWidth;
    result.max_height = cfg->maxHeight;
    result.fullscreen = cfg->fullScreen;
    result.resizable = cfg->resizable;
    result.debug = cfg->debug;
    result.warp = cfg->warp;
    result.minimize_latency = cfg->minimizeLatency;
    result.argc = cfg->argc;
    result.argv = cfg->argv;
    result.user_data = cfg->userData;
    result.on_startup = cfg->onStartup;
    result.on_update = cfg->onUpdate;
    result.on_shutdown = cfg->onShutdown;
    return result;
}

AppConfig convert(const rush_app_config* cfg)
{
    AppConfig result;
    result.name = cfg->name;
    result.vsync = cfg->vsync;
    result.width = cfg->width;
    result.height = cfg->height;
    result.maxWidth = cfg->max_width;
    result.maxHeight = cfg->max_height;
    result.fullScreen = cfg->fullscreen;
    result.resizable = cfg->resizable;
    result.debug = cfg->debug;
    result.warp = cfg->warp;
    result.minimizeLatency = cfg->minimize_latency;
    result.argc = cfg->argc;
    result.argv = cfg->argv;
    result.userData = cfg->user_data;
    result.onStartup = cfg->on_startup;
    result.onUpdate = cfg->on_update;
    result.onShutdown = cfg->on_shutdown;
    return result;
}

GfxFormat convert(rush_gfx_format fmt)
{
    switch(fmt)
    {
    case RUSH_GFX_FORMAT_UNKNOWN: return GfxFormat_Unknown;
    case RUSH_GFX_FORMAT_D24_UNORM_S8_UINT: return GfxFormat_D24_Unorm_S8_Uint;
    case RUSH_GFX_FORMAT_D24_UNORM_X8: return GfxFormat_D24_Unorm_X8;
    case RUSH_GFX_FORMAT_D32_FLOAT: return GfxFormat_D32_Float;
    case RUSH_GFX_FORMAT_D32_FLOAT_S8_UINT: return GfxFormat_D32_Float_S8_Uint;
    case RUSH_GFX_FORMAT_R8_UNORM: return GfxFormat_R8_Unorm;
    case RUSH_GFX_FORMAT_R16_FLOAT: return GfxFormat_R16_Float;
    case RUSH_GFX_FORMAT_R16_UINT: return GfxFormat_R16_Uint;
    case RUSH_GFX_FORMAT_R32_FLOAT: return GfxFormat_R32_Float;
    case RUSH_GFX_FORMAT_R32_UINT: return GfxFormat_R32_Uint;
    case RUSH_GFX_FORMAT_RG8_UNORM: return GfxFormat_RGB8_Unorm;
    case RUSH_GFX_FORMAT_RG16_FLOAT: return GfxFormat_RG16_Float;
    case RUSH_GFX_FORMAT_RG32_FLOAT: return GfxFormat_RG32_Float;
    case RUSH_GFX_FORMAT_RGB32_FLOAT: return GfxFormat_RGB32_Float;
    case RUSH_GFX_FORMAT_RGB8_UNORM: return GfxFormat_RGB8_Unorm;
    case RUSH_GFX_FORMAT_RGBA16_FLOAT: return GfxFormat_RGBA16_Float;
    case RUSH_GFX_FORMAT_RGBA16_UNORM: return GfxFormat_RGBA16_Unorm;
    case RUSH_GFX_FORMAT_RGBA32_FLOAT: return GfxFormat_RGBA32_Float;
    case RUSH_GFX_FORMAT_RGBA8_UNORM: return GfxFormat_RGBA8_Unorm;
    case RUSH_GFX_FORMAT_RGBA8_SRGB: return GfxFormat_RGBA8_sRGB;
    case RUSH_GFX_FORMAT_BGRA8_UNORM: return GfxFormat_BGRA8_Unorm;
    case RUSH_GFX_FORMAT_BGRA8_SRGB: return GfxFormat_BGRA8_sRGB;
    case RUSH_GFX_FORMAT_BC1_UNORM: return GfxFormat_BC1_Unorm;
    case RUSH_GFX_FORMAT_BC1_UNORM_SRGB: return GfxFormat_BC1_Unorm_sRGB;
    case RUSH_GFX_FORMAT_BC3_UNORM: return GfxFormat_BC3_Unorm;
    case RUSH_GFX_FORMAT_BC3_UNORM_SRGB: return GfxFormat_BC3_Unorm_sRGB;
    case RUSH_GFX_FORMAT_BC4_UNORM: return GfxFormat_BC4_Unorm;
    case RUSH_GFX_FORMAT_BC5_UNORM: return GfxFormat_BC5_Unorm;
    case RUSH_GFX_FORMAT_BC6H_UFLOAT: return GfxFormat_BC6H_UFloat;
    case RUSH_GFX_FORMAT_BC6H_SFLOAT: return GfxFormat_BC6H_SFloat;
    case RUSH_GFX_FORMAT_BC7_UNORM: return GfxFormat_BC7_Unorm;
    case RUSH_GFX_FORMAT_BC7_UNORM_SRGB: return GfxFormat_BC7_Unorm_sRGB;
    }
}

ColorRGBA convert(const rush_color_rgba& c)
{
    return ColorRGBA(c.r, c.g, c.b, c.a);
}

} // namespace

void rush_app_config_init(rush_app_config* out_cfg)
{
    AppConfig cfg;
    *out_cfg = convert(&cfg);
}

int rush_platform_main(const rush_app_config* in_cfg)
{
    AppConfig cfg = convert(in_cfg);
    return Platform_Main(cfg);
}

rush_gfx_device* rush_platform_get_device()
{
    return (rush_gfx_device*)Platform_GetGfxDevice();
}

rush_gfx_context* rush_platform_get_context()
{
    return (rush_gfx_context*)Platform_GetGfxContext();
}

void rush_gfx_begin_pass(
	struct rush_gfx_context* ctx, 
	uint32_t color_count,
	const rush_gfx_color_target* color,
	const rush_gfx_depth_target* depth,
	rush_gfx_pass_flags flags
)
{
    GfxPassDesc desc;
    color_count = min<u32>(color_count, GfxPassDesc::MaxTargets);
    for (u32 i=0; i<color_count; ++i)
    {
        desc.color[i] = GfxTexture(UntypedResourceHandle(color[i].target.handle));
        desc.clearColors[i] = convert(color[i].clear_color);
    }
    if (depth)
    {
        desc.depth = GfxTexture(UntypedResourceHandle(depth->target.handle));
        desc.clearDepth = depth->clear_depth;
        desc.clearStencil = depth->clear_stencil;
    }
    desc.flags = (GfxPassFlags)flags;
    Gfx_BeginPass((GfxContext*)ctx, desc);
}

void rush_gfx_end_pass(struct rush_gfx_context* ctx)
{
    Gfx_EndPass((GfxContext*)ctx);
}

void rush_gfx_draw(struct rush_gfx_context* ctx, uint32_t first_vertex, uint32_t vertex_count)
{
    Gfx_Draw((GfxContext*)ctx, first_vertex, vertex_count);
}

