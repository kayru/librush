#include "Platform.h"

#ifdef RUSH_PLATFORM_WINDOWS

#include "GfxCommon.h"
#include "GfxDevice.h"
#include "WindowWin32.h"

#include <string.h>

namespace Rush
{

extern Window*     g_mainWindow;
extern GfxDevice*  g_mainGfxDevice;
extern GfxContext* g_mainGfxContext;

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

void Platform_Run(PlatformCallback_Update onUpdate, void* userData) 
{
	while (g_mainWindow->isClosed() == false)
	{
		Gfx_BeginFrame();

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (onUpdate)
		{
			onUpdate(userData);
		}

		Gfx_EndFrame();
		Gfx_Present();
	}
}

}

#endif
