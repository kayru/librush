#pragma once

#include "Rush.h"

namespace Rush
{

class GfxContext;
class GfxDevice;
class KeyboardState;
class MouseState;
class Window;
struct GfxConfig;
struct WindowDesc;

typedef void (*PlatformCallback_Startup)(void* userData);
typedef void (*PlatformCallback_Update)(void* userData);
typedef void (*PlatformCallback_Shutdown)(void* userData);

struct AppConfig
{
	const char* name = "RushApp";

	int vsync = 1;

	int width  = 640;
	int height = 480;

	int maxWidth  = 0;
	int maxHeight = 0;

	bool fullScreen      = false;
	bool resizable       = false;
	bool debug           = false;
	bool warp            = false;
	bool minimizeLatency = false;

	int    argc = 0;
	char** argv = nullptr;

	const GfxConfig* gfxConfig = nullptr;

	void* userData = nullptr;

	PlatformCallback_Startup  onStartup = nullptr;
	PlatformCallback_Update   onUpdate = nullptr;
	PlatformCallback_Shutdown onShutdown = nullptr;
};

class Application
{
public:
	virtual ~Application() = default;
	virtual void update()  = 0;
};

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

	return Platform_Main(wrappedCfg);
}

const char* Platform_GetExecutableDirectory();
void        Platform_TerminateProcess(int status);

GfxDevice*  Platform_GetGfxDevice();
GfxContext* Platform_GetGfxContext();
Window*     Platform_GetWindow();
Window*     Platform_CreateWindow(const WindowDesc& desc);

}
