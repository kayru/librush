#include "RushC.h"
#include "Platform.h"

#include <stdio.h>
#include <string.h>

namespace {

void convert(const Rush::AppConfig* cfg, rush_app_config* out_cfg)
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

void convert(const rush_app_config* cfg, Rush::AppConfig* out_cfg)
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

} // namespace

void rush_app_config_init(rush_app_config* out_cfg)
{
    Rush::AppConfig cfg;
    convert(&cfg, out_cfg);
}

int rush_platform_main(const rush_app_config* in_cfg)
{
    Rush::AppConfig cfg;
    convert(in_cfg, &cfg);
    return Platform_Main(cfg);
}
