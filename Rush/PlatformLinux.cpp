#include "Platform.h"

#ifdef RUSH_PLATFORM_LINUX

#include "GfxCommon.h"
#include "GfxDevice.h"
#include "WindowXCB.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

namespace Rush
{

extern Window*     g_mainWindow;
extern GfxDevice*  g_mainGfxDevice;
extern GfxContext* g_mainGfxContext;

Window* Platform_CreateWindow(const WindowDesc& desc) { return new WindowXCB(desc); }

void        Platform_TerminateProcess(int status) { exit(status); }
GfxDevice*  Platform_GetGfxDevice() { return g_mainGfxDevice; }
GfxContext* Platform_GetGfxContext() { return g_mainGfxContext; }
Window*     Platform_GetWindow() { return g_mainWindow; }

const char* Platform_GetExecutableDirectory()
{
	static bool isInitialized = false;
	static char path[4096] = {};

	if (!isInitialized)
	{
		ssize_t writtenBytes = readlink("/proc/self/exe", path, sizeof(path));

		if (writtenBytes < 0)
		{
			RUSH_LOG_ERROR("readlink(\"/proc/self/exe\") failed: %s (%d)", strerror(errno), errno);
			strncpy(path, ".", 2);
		}
		else if (writtenBytes == sizeof(path))
		{
			RUSH_LOG_ERROR("readlink(\"/proc/self/exe\") failed because output buffer is too small");
			strncpy(path, ".", 2);
		}
		else
		{
			size_t lastSlash = 0;
			for (size_t i = 0; i < sizeof(path) && path[i]; ++i)
			{
				if (path[i] == '/')
					lastSlash = i;
			}

			if (lastSlash != 0)
			{
				path[lastSlash] = 0;
			}
		}

		isInitialized = true;
	}

	return path;
}

void Platform_Run(PlatformCallback_Update onUpdate, void* userData) 
{
	while (window->isClosed() == false)
	{
		window->pollEvents();

		Gfx_BeginFrame();

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
