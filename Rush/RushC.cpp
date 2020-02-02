#include "RushC.h"
#include "Platform.h"
#include "GfxDevice.h"

#include <stdio.h>
#include <string.h>

using namespace Rush;

namespace {

void convert(const AppConfig* cfg, rush_app_config* out_cfg)
{
    out_cfg->name = cfg->name;
    out_cfg->vsync = cfg->vsync;
    out_cfg->width = cfg->width;
    out_cfg->height = cfg->height;
    out_cfg->max_width = cfg->maxWidth;
    out_cfg->max_height = cfg->maxHeight;
    out_cfg->fullscreen = cfg->fullScreen;
    out_cfg->resizable = cfg->resizable;
    out_cfg->debug = cfg->debug;
    out_cfg->warp = cfg->warp;
    out_cfg->minimize_latency = cfg->minimizeLatency;
    out_cfg->argc = cfg->argc;
    out_cfg->argv = cfg->argv;
    out_cfg->user_data = cfg->userData;
    out_cfg->on_startup = cfg->onStartup;
    out_cfg->on_update = cfg->onUpdate;
    out_cfg->on_shutdown = cfg->onShutdown;
}

void convert(const rush_app_config* cfg, AppConfig* out_cfg)
{
    out_cfg->name = cfg->name;
    out_cfg->vsync = cfg->vsync;
    out_cfg->width = cfg->width;
    out_cfg->height = cfg->height;
    out_cfg->maxWidth = cfg->max_width;
    out_cfg->maxHeight = cfg->max_height;
    out_cfg->fullScreen = cfg->fullscreen;
    out_cfg->resizable = cfg->resizable;
    out_cfg->debug = cfg->debug;
    out_cfg->warp = cfg->warp;
    out_cfg->minimizeLatency = cfg->minimize_latency;
    out_cfg->argc = cfg->argc;
    out_cfg->argv = cfg->argv;
    out_cfg->userData = cfg->user_data;
    out_cfg->onStartup = cfg->on_startup;
    out_cfg->onUpdate = cfg->on_update;
    out_cfg->onShutdown = cfg->on_shutdown;
}

ColorRGBA convert(const rush_color_rgba& c)
{
    return ColorRGBA(c.r, c.g, c.b, c.a);
}

} // namespace

void rush_app_config_init(rush_app_config* out_cfg)
{
    AppConfig cfg;
    convert(&cfg, out_cfg);
}

int rush_platform_main(const rush_app_config* in_cfg)
{
    AppConfig cfg;
    convert(in_cfg, &cfg);
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
	enum rush_gfx_pass_flags flags
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
