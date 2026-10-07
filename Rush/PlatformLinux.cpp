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
void closeWindowOnTerminationSignal();
extern GfxDevice*  g_mainGfxDevice;
extern GfxContext* g_mainGfxContext;

Window* Platform_CreateWindow(const WindowDesc& desc) { return new WindowXCB(desc); }

void        Platform_TerminateProcess(int status) { exit(status); }
GfxDevice*  Platform_GetGfxDevice() { return g_mainGfxDevice; }
GfxContext* Platform_GetGfxContext() { return g_mainGfxContext; }
Window*     Platform_GetWindow() { return g_mainWindow; }

// X11 selections need a window to own them and its event loop to serve them
void Platform_SetClipboardText(const char* text)
{
	if (g_mainWindow)
	{
		static_cast<WindowXCB*>(g_mainWindow)->setClipboardText(text);
	}
}

String Platform_GetClipboardText()
{
	return g_mainWindow ? static_cast<WindowXCB*>(g_mainWindow)->getClipboardText() : String();
}

// Filled once: function-local static initialization is thread-safe
const char* Platform_GetExecutableDirectory()
{
	struct Path
	{
		char text[4096] = {};
	};
	static const Path path = []
	{
		Path result;
		const ssize_t writtenBytes = readlink("/proc/self/exe", result.text, sizeof(result.text));
		if (writtenBytes < 0)
		{
			RUSH_LOG_ERROR("readlink(\"/proc/self/exe\") failed: %s (%d)", strerror(errno), errno);
			result.text[0] = '.';
			result.text[1] = 0;
			return result;
		}
		if (size_t(writtenBytes) >= sizeof(result.text))
		{
			RUSH_LOG_ERROR("readlink(\"/proc/self/exe\") failed because output buffer is too small");
			result.text[0] = '.';
			result.text[1] = 0;
			return result;
		}
		// readlink does not terminate the string
		result.text[writtenBytes] = 0;
		for (ssize_t i = writtenBytes; i-- > 1;)
		{
			if (result.text[i] == '/')
			{
				result.text[i] = 0;
				break;
			}
		}
		return result;
	}();
	return path.text;
}

void Platform_Run(PlatformCallback_Update onUpdate, void* userData) 
{
	while (!Platform_IsExitRequested() && (!g_mainWindow || g_mainWindow->isClosed() == false))
	{
		if (g_mainWindow)
		{
			g_mainWindow->pollEvents();
		}
		closeWindowOnTerminationSignal();

		Gfx_BeginFrame();

		if (onUpdate)
		{
			onUpdate(userData);
		}

		Gfx_EndFrame();
		Gfx_Present();
	}
}

bool Platform_IsDebuggerPresent()
{
	return false;
}

}

#endif
