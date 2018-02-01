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

typedef void (*PlatformCallback_Startup)();
typedef void (*PlatformCallback_Update)();
typedef void (*PlatformCallback_Shutdown)();

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

	PlatformCallback_Startup  onStartup;
	PlatformCallback_Update   onUpdate;
	PlatformCallback_Shutdown onShutdown;
};

int Platform_Main(const AppConfig& cfg);

const char* Platform_GetExecutableDirectory();
void        Platform_TerminateProcess(int status);

GfxDevice*  Platform_GetGfxDevice();
GfxContext* Platform_GetGfxContext();
Window*     Platform_GetWindow();
Window*     Platform_CreateWindow(const WindowDesc& desc);

enum MessageBoxType
{
	MessageBoxType_RetryAbortIgnore,
	MessageBoxType_OkCancel,
	MessageBoxType_Ok,
};

enum MessageBoxResult
{
	MessageBoxResult_Ok,
	MessageBoxResult_Cancel,
	MessageBoxResult_Retry,
	MessageBoxResult_Abort,
	MessageBoxResult_Ignore
};

MessageBoxResult Platform_MessageBox(
    const char* text, const char* caption, MessageBoxType type = MessageBoxType_RetryAbortIgnore);
}
