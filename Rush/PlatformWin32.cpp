#include "Platform.h"

#ifdef RUSH_PLATFORM_WINDOWS

#include "GfxCommon.h"
#include "GfxDevice.h"
#include "UtilLog.h"
#include "WindowWin32.h"

#include <debugapi.h>
#include <string.h>

namespace Rush
{

extern Window*     g_mainWindow;
void closeWindowOnTerminationSignal();
extern GfxDevice*  g_mainGfxDevice;
extern GfxContext* g_mainGfxContext;

Window* Platform_CreateWindow(const WindowDesc& desc) { return new WindowWin32(desc); }

void        Platform_TerminateProcess(int status) { exit(status); }
GfxDevice*  Platform_GetGfxDevice() { return g_mainGfxDevice; }
GfxContext* Platform_GetGfxContext() { return g_mainGfxContext; }
Window*     Platform_GetWindow() { return g_mainWindow; }

// The clipboard takes data only from a window that opened it: without one, nothing is copied
void Platform_SetClipboardText(const char* text)
{
	HWND hwnd = g_mainWindow ? HWND(g_mainWindow->nativeHandle()) : nullptr;
	const int wideCount = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
	if (!hwnd || wideCount <= 0 || !OpenClipboard(hwnd))
	{
		return;
	}
	EmptyClipboard();
	if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size_t(wideCount) * sizeof(wchar_t)))
	{
		if (wchar_t* wide = static_cast<wchar_t*>(GlobalLock(memory)))
		{
			MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wideCount);
			GlobalUnlock(memory);
			// The clipboard owns the memory once this succeeds
			if (!SetClipboardData(CF_UNICODETEXT, memory))
			{
				GlobalFree(memory);
			}
		}
		else
		{
			GlobalFree(memory);
		}
	}
	CloseClipboard();
}

String Platform_GetClipboardText()
{
	String result;
	HWND hwnd = g_mainWindow ? HWND(g_mainWindow->nativeHandle()) : nullptr;
	if (!OpenClipboard(hwnd))
	{
		return result;
	}
	if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
	{
		if (const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(data)))
		{
			const int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
			if (count > 1)
			{
				result.reset(size_t(count - 1));
				WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), count, nullptr, nullptr);
			}
			GlobalUnlock(data);
		}
	}
	CloseClipboard();
	return result;
}

// Filled once: function-local static initialization is thread-safe
const char* Platform_GetExecutableDirectory()
{
	struct Path
	{
		char text[1024] = {};
	};
	static const Path path = []
	{
		Path result;
		const DWORD length = GetModuleFileNameA(nullptr, result.text, DWORD(sizeof(result.text)));
		if (length == 0 || length >= sizeof(result.text))
		{
			RUSH_LOG_ERROR("GetModuleFileNameA failed or the path is too long (%lu)", GetLastError());
			result.text[0] = '.';
			result.text[1] = 0;
			return result;
		}
		for (DWORD i = length; i-- > 0;)
		{
			if (result.text[i] == '/' || result.text[i] == '\\')
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
		Gfx_BeginFrame();

		MSG msg;
		// Unicode variants: WM_CHAR arrives as UTF-16
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		closeWindowOnTerminationSignal();

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
	return IsDebuggerPresent();
}

}

#endif
