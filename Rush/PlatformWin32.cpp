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

		size_t pathLen  = std::strlen(path);
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

#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
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
#endif

	if (cfg.onStartup)
	{
		cfg.onStartup(cfg.userData);
	}

	while (window->isClosed() == false)
	{
#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
		Gfx_BeginFrame();

		if (cfg.onUpdate)
		{
			cfg.onUpdate(cfg.userData);
		}

		Gfx_EndFrame();
		Gfx_Present();
#endif // RUSH_RENDER_API!=RUSH_RENDER_API_NULL

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

#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
	Gfx_Release(g_mainGfxContext);
	Gfx_Release(g_mainGfxDevice);
#endif

	window->release();

	return 0;
}
}

#endif
