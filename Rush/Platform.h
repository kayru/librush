#pragma once

#include "Rush.h"
#include "UtilString.h"

namespace Rush
{

class GfxContext;
class GfxDevice;
class KeyboardState;
class MouseState;
class Window;
struct GfxConfig;
struct WindowDesc;
enum class GfxTimingLevel : u8;
enum class WindowCloseBehavior : u8;

typedef void (*PlatformCallback_Startup)(void* userData);
typedef void (*PlatformCallback_Update)(void* userData);
typedef void (*PlatformCallback_Shutdown)(void* userData);
typedef void (*PlatformCallback_Suspend)(void* userData);
typedef void (*PlatformCallback_Resume)(void* userData);

struct AppConfig
{
	const char* name = "RushApp";

	int vsync = 1;

	int width  = 640;
	int height = 480;

	int maxWidth  = 0;
	int maxHeight = 0;

	bool fullScreen      = false;
	bool maximized       = false;
	bool resizable       = false;
	bool debug           = false;
	bool warp            = false;
	bool minimizeLatency = false;
	bool headless        = false;
	bool background      = false; // unattended: the window must not take focus or come to the front

	WindowCloseBehavior closeBehavior = {}; // WindowCloseBehavior::Close

	GfxTimingLevel timingLevel = {}; // GfxTimingLevel::Frame

	int    argc = 0;
	char** argv = nullptr;

	const GfxConfig* gfxConfig = nullptr;

	void* userData = nullptr;

	PlatformCallback_Startup  onStartup = nullptr;
	PlatformCallback_Update   onUpdate = nullptr;
	PlatformCallback_Shutdown onShutdown = nullptr;

	// iOS: the app left / returns to the foreground. No frames run in between.
	PlatformCallback_Suspend onSuspend = nullptr;
	PlatformCallback_Resume  onResume  = nullptr;
};

void Platform_Startup(const AppConfig& cfg);
void Platform_Run(PlatformCallback_Update frameFn, void* userData);
void Platform_Shutdown();

// Request the main loop to exit. Needed for headless runs (no window to close).
void Platform_RequestExit();
bool Platform_IsExitRequested();

class Application
{
public:
	virtual ~Application() = default;
	virtual void update()  = 0;
	virtual void suspend() {}
	virtual void resume() {}
};

// Convenience wrappers over explicit startup/run/shutdown API
int Platform_Main(const AppConfig& cfg);

template <typename T> inline int Platform_Main(AppConfig cfg)
{
	struct Context
	{
		Application* app = nullptr;
	} context;

	AppConfig wrappedCfg = cfg;

	wrappedCfg.userData   = &context;
	wrappedCfg.onStartup  = [](void* context) { reinterpret_cast<Context*>(context)->app = new T; };
	wrappedCfg.onShutdown = [](void* context) { delete reinterpret_cast<Context*>(context)->app; };
	wrappedCfg.onUpdate   = [](void* context) { reinterpret_cast<Context*>(context)->app->update(); };
	wrappedCfg.onSuspend  = [](void* context) { reinterpret_cast<Context*>(context)->app->suspend(); };
	wrappedCfg.onResume   = [](void* context) { reinterpret_cast<Context*>(context)->app->resume(); };

	return Platform_Main(wrappedCfg);
}

const char* Platform_GetExecutableDirectory();
void        Platform_TerminateProcess(int status);
void        Platform_TerminateProcessFatal(int status);

GfxDevice*  Platform_GetGfxDevice();
GfxContext* Platform_GetGfxContext();
Window*     Platform_GetWindow();
Window*     Platform_CreateWindow(const WindowDesc& desc);

// System clipboard text, UTF-8
void   Platform_SetClipboardText(const char* text);
String Platform_GetClipboardText();

bool Platform_IsDebuggerPresent();

}
