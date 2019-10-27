#include "Platform.h"

#ifdef RUSH_PLATFORM_WINDOWS

#include "GfxCommon.h"
#include "GfxDevice.h"
#include "WindowWin32.h"

#include <string.h>

namespace Rush
{

static Window*     g_mainWindow     = nullptr;
static GfxDevice*  g_mainGfxDevice  = nullptr;
static GfxContext* g_mainGfxContext = nullptr;

Window* Platform_CreateWindow(const WindowDesc& desc) { return new WindowWin32(desc); }

void        Platform_TerminateProcess(int status) { exit(status); }
GfxDevice*  Platform_GetGfxDevice() { return g_mainGfxDevice; }
GfxContext* Platform_GetGfxContext() { return g_mainGfxContext; }
Window*     Platform_GetWindow() { return g_mainWindow; }

const char* Platform_GetExecutableDirectory()
{
	static char path[1024] = {};

	if (!path[0])
	{
		GetModuleFileNameA(nullptr, path, sizeof(path));

		size_t pathLen  = strlen(path);
		size_t slashIdx = (size_t)-1;
		for (size_t i = pathLen - 1; i > 0; --i)
		{
			if (path[i] == '/' || path[i] == '\\')
			{
				slashIdx = i;
				break;
			}
		}

		if (slashIdx != size_t(-1))
		{
			path[slashIdx] = 0;
		}
	}

	return path;
}

int Platform_Main(const AppConfig& cfg)
{
	WindowDesc windowDesc;
	windowDesc.width      = cfg.width;
	windowDesc.height     = cfg.height;
	windowDesc.resizable  = cfg.resizable;
	windowDesc.caption    = cfg.name;
	windowDesc.fullScreen = cfg.fullScreen;

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

	if (cfg.onStartup)
	{
		cfg.onStartup(cfg.userData);
	}

	while (window->isClosed() == false)
	{
		Gfx_BeginFrame();

		if (cfg.onUpdate)
		{
			cfg.onUpdate(cfg.userData);
		}

		Gfx_EndFrame();
		Gfx_Present();

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (cfg.onShutdown)
	{
		cfg.onShutdown(cfg.userData);
	}

	Gfx_Release(g_mainGfxContext);
	Gfx_Release(g_mainGfxDevice);

	window->release();

	return 0;
}
}

#endif
