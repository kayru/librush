#include "Platform.h"

#ifdef RUSH_PLATFORM_WINDOWS

#include "WindowWin32.h"

namespace Rush
{

Window* Platform_CreateWindow(const WindowDesc& desc) { return new WindowWin32(desc); }

int Platform_Main(const AppConfig& cfg)
{
	WindowDesc windowDesc;
	windowDesc.width     = cfg.width;
	windowDesc.height    = cfg.height;
	windowDesc.resizable = cfg.resizable;
	windowDesc.caption   = cfg.name;
	Window* window       = Platform_CreateWindow(windowDesc);

	if (cfg.onStartup)
	{
		cfg.onStartup();
	}

	while (window->isClosed() == false)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (cfg.onShutdown)
	{
		cfg.onShutdown();
	}

	window->release();

	return 0;
}
}

#endif
