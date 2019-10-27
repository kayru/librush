#include "Platform.h"
#include "WindowMac.h"
#include "UtilLog.h"
#include "GfxDevice.h"

#if defined(RUSH_PLATFORM_MAC)

#import <Cocoa/Cocoa.h>
#include <mach-o/dyld.h>
#include <limits.h>
#include <string_view>
#include <string>

using namespace Rush;

@interface AppDelegate : NSObject<NSApplicationDelegate>
{
	bool terminated;
}

+ (AppDelegate *)sharedDelegate;
- (id)init;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (bool)applicationHasTerminated;

@end

@implementation AppDelegate

+ (AppDelegate *)sharedDelegate
{
	static id delegate = [AppDelegate new];
	return delegate;
}

- (id)init
{
	self = [super init];
	
	if (nil == self)
	{
		return nil;
	}
	
	self->terminated = false;
	return self;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	RUSH_UNUSED(sender);
	self->terminated = true;
	return NSTerminateCancel;
}

- (bool)applicationHasTerminated
{
	return self->terminated;
}

@end

namespace Rush
{

static Window*     g_mainWindow     = nullptr;
static GfxDevice*  g_mainGfxDevice  = nullptr;
static GfxContext* g_mainGfxContext = nullptr;

Window*     Platform_CreateWindow(const WindowDesc& desc) { return new WindowMac(desc); }
void        Platform_TerminateProcess(int status) { exit(status); }
GfxDevice*  Platform_GetGfxDevice() { return g_mainGfxDevice; }
GfxContext* Platform_GetGfxContext() { return g_mainGfxContext; }
Window*     Platform_GetWindow() { return g_mainWindow; }

const char* Platform_GetExecutableDirectory()
{
	constexpr u32 maxLength = PATH_MAX + 1;
	static char result[maxLength];
	if (result[0] == 0)
	{
		u32 resultLength = maxLength;
		_NSGetExecutablePath(result, &resultLength);
		size_t lastSlash = std::string_view(result, resultLength).find_last_of("/");
		if (lastSlash != std::string::npos)
		{
			result[lastSlash] = 0;
		}
	}
	return result;
}

int Platform_Main(const AppConfig& cfg)
{
	WindowDesc windowDesc;
	windowDesc.width      = cfg.width;
	windowDesc.height     = cfg.height;
	windowDesc.resizable  = cfg.resizable;
	windowDesc.caption    = cfg.name;
	windowDesc.fullScreen = cfg.fullScreen;
	
	WindowMac* window = new WindowMac(windowDesc);
	
	g_mainWindow = window;
	
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
	
	if (cfg.onStartup)
	{
		cfg.onStartup(cfg.userData);
	}
	
	@autoreleasepool
	{
		[NSApplication sharedApplication];

		id dg = [AppDelegate sharedDelegate];
		[NSApp setDelegate:dg];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		[NSApp activateIgnoringOtherApps:YES];
		[NSApp finishLaunching];

		[[NSNotificationCenter defaultCenter]
			postNotificationName:NSApplicationWillFinishLaunchingNotification
			object:NSApp];

		[[NSNotificationCenter defaultCenter]
			postNotificationName:NSApplicationDidFinishLaunchingNotification
			object:NSApp];

		id quitMenuItem = [[NSMenuItem new]
			initWithTitle:@"Quit"
			action:@selector(terminate:)
			keyEquivalent:@"q"];

		id appMenu = [[NSMenu new] autorelease];
		[appMenu addItem:quitMenuItem];

		id appMenuItem = [[NSMenuItem new] autorelease];
		[appMenuItem setSubmenu:appMenu];

		id menubar = [[NSMenu new] autorelease];
		[menubar addItem:appMenuItem];
		[NSApp setMainMenu:menubar];

		while (![dg applicationHasTerminated])
		{
			@autoreleasepool
			{
				while (NSEvent* event = [NSApp
						nextEventMatchingMask:NSEventMaskAny
						untilDate:[NSDate distantPast]
						inMode:NSDefaultRunLoopMode
						dequeue:YES])
				{
					window->processEvent(event);
					[NSApp sendEvent:event];
					[NSApp updateWindows];
				}

				Gfx_BeginFrame();
				if (cfg.onUpdate)
				{
					cfg.onUpdate(cfg.userData);
				}
				Gfx_EndFrame();
				Gfx_Present();
			}
		}
	}
	
	if (cfg.onShutdown)
	{
		cfg.onShutdown(cfg.userData);
	}
	
	Gfx_Release(g_mainGfxContext);
	Gfx_Release(g_mainGfxDevice);
	
	window->release();
	
	return 0;
}

}
#endif
