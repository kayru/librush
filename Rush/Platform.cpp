#include "Platform.h"
#include "GfxDevice.h"
#include "UtilLog.h"
#include "Window.h"

namespace Rush
{

Window*     g_mainWindow     = nullptr;
GfxDevice*  g_mainGfxDevice  = nullptr;
GfxContext* g_mainGfxContext = nullptr;

void Platform_Startup(const AppConfig& cfg)
{
	RUSH_ASSERT(g_mainWindow == nullptr);
	RUSH_ASSERT(g_mainGfxDevice == nullptr);
	RUSH_ASSERT(g_mainGfxContext == nullptr);

	WindowDesc windowDesc;
	windowDesc.width      = cfg.width;
	windowDesc.height     = cfg.height;
	windowDesc.resizable  = cfg.resizable;
	windowDesc.caption    = cfg.name;
	windowDesc.fullScreen = cfg.fullScreen;
	windowDesc.maximized  = cfg.maximized;

	Window* window = Platform_CreateWindow(windowDesc);

	g_mainWindow = window;

	GfxConfig gfxConfig;
	if (cfg.gfxConfig)
	{
		gfxConfig = *cfg.gfxConfig;
	}
	else
	{
		gfxConfig = GfxConfig(cfg);
	}
	g_mainGfxDevice  = Gfx_CreateDevice(window, gfxConfig);
	g_mainGfxContext = Gfx_AcquireContext();
}

void Platform_Shutdown()
{
	RUSH_ASSERT(g_mainWindow != nullptr);
	RUSH_ASSERT(g_mainGfxDevice != nullptr);
	RUSH_ASSERT(g_mainGfxContext != nullptr);
	
	Gfx_Release(g_mainGfxContext);
	Gfx_Release(g_mainGfxDevice);

	g_mainWindow->release();
}

int Platform_Main(const AppConfig& cfg)
{
	Platform_Startup(cfg);
	
	if (cfg.onStartup)
	{
		cfg.onStartup(cfg.userData);
	}

	Platform_Run(cfg.onUpdate, cfg.userData);
	
	if (cfg.onShutdown)
	{
		cfg.onShutdown(cfg.userData);
	}
	
	Platform_Shutdown();
	
	return 0;
}

}
