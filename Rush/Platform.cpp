#include "Platform.h"
#include "GfxDevice.h"
#include "UtilLog.h"
#include "Window.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace Rush
{

Window*     g_mainWindow     = nullptr;
GfxDevice*  g_mainGfxDevice  = nullptr;
GfxContext* g_mainGfxContext = nullptr;

static bool g_exitRequested = false;

void Platform_TerminateProcessFatal(int status)
{
	// No static destructors: other threads may still be running
	std::fflush(nullptr);
	std::_Exit(status);
}

void Platform_RequestExit() { g_exitRequested = true; }
bool Platform_IsExitRequested() { return g_exitRequested; }

// Handlers may run on any thread (a new one on Windows): volatile sig_atomic_t covers only the interrupted thread
static std::atomic<int> g_terminationSignal = 0;
static_assert(std::atomic<int>::is_always_lock_free);

static void terminationSignalHandler(int signal)
{
	g_terminationSignal = signal;
	std::signal(signal, SIG_DFL);
}

static const struct
{
	int signal;
	const char* name;
} kTerminationSignals[] = {
	{SIGINT, "SIGINT"},
	{SIGTERM, "SIGTERM"},
#if defined(SIGHUP)
	{SIGHUP, "SIGHUP"},
#endif
#if defined(SIGBREAK)
	{SIGBREAK, "SIGBREAK"},
#endif
};

void closeWindowOnTerminationSignal()
{
	const int signal = g_terminationSignal.exchange(0);
	if (signal == 0)
	{
		return;
	}
	for (const auto& it : kTerminationSignals)
	{
		if (it.signal == signal)
		{
			RUSH_LOG("Closing: %s received", it.name);
		}
	}
	if (g_mainWindow)
	{
		g_mainWindow->close();
	}
}

void Platform_Startup(const AppConfig& cfg)
{
	RUSH_ASSERT(g_mainWindow == nullptr);
	RUSH_ASSERT(g_mainGfxDevice == nullptr);
	RUSH_ASSERT(g_mainGfxContext == nullptr);

	g_exitRequested = false;

	GfxConfig gfxConfig;
	if (cfg.gfxConfig)
	{
		gfxConfig = *cfg.gfxConfig;
	}
	else
	{
		gfxConfig = GfxConfig(cfg);
	}
	gfxConfig.headless = gfxConfig.headless || cfg.headless;

	Window* window = nullptr;
	if (!gfxConfig.headless)
	{
		WindowDesc windowDesc;
		windowDesc.width      = cfg.width;
		windowDesc.height     = cfg.height;
		windowDesc.resizable  = cfg.resizable;
		windowDesc.caption    = cfg.name;
		windowDesc.fullScreen = cfg.fullScreen;
		windowDesc.maximized  = cfg.maximized;
		windowDesc.background = cfg.background;

		window = Platform_CreateWindow(windowDesc);
	}

	g_mainWindow = window;

	g_terminationSignal = 0;
	if (window)
	{
		for (const auto& it : kTerminationSignals)
		{
			std::signal(it.signal, terminationSignalHandler);
		}
	}

	g_mainGfxDevice  = Gfx_CreateDevice(window, gfxConfig);
	g_mainGfxContext = Gfx_AcquireContext();
}

void Platform_Shutdown()
{
	RUSH_ASSERT(g_mainGfxDevice != nullptr);
	RUSH_ASSERT(g_mainGfxContext != nullptr);
	
	Gfx_Release(g_mainGfxContext);
	Gfx_Release(g_mainGfxDevice);

	if (g_mainWindow)
	{
		g_mainWindow->release();
		g_mainWindow = nullptr;
	}

	g_mainGfxDevice = nullptr;
	g_mainGfxContext = nullptr;
}

#if !defined(RUSH_PLATFORM_SPECIFIC_MAIN)
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
#endif

}
