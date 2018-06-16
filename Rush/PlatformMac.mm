#include "Platform.h"
#include "WindowMac.h"
#include "UtilLog.h"
#include "GfxDevice.h"

#if defined(RUSH_PLATFORM_MAC)

#import <Cocoa/Cocoa.h>

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
	return "."; // TODO
}

MessageBoxResult Platform_MessageBox(const char* text, const char* caption, MessageBoxType type)
{
	RUSH_LOG("MessageBox: %s\n%s", caption, text);
	if (type == MessageBoxType_RetryAbortIgnore)
	{
		return MessageBoxResult_Retry;
	}
	else
	{
		return MessageBoxResult_Ok;
	}
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
	
#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
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
#endif
	
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

#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
				Gfx_BeginFrame();
#endif
				if (cfg.onUpdate)
				{
					cfg.onUpdate(cfg.userData);
				}
#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
				Gfx_EndFrame();
				Gfx_Present();
#endif
			}
		}
	}
	
	if (cfg.onShutdown)
	{
		cfg.onShutdown(cfg.userData);
	}
	
#if RUSH_RENDER_API != RUSH_RENDER_API_NULL
	Gfx_Release(g_mainGfxContext);
	Gfx_Release(g_mainGfxDevice);
#endif
	
	window->release();
	
	return 0;
}

}
#endif
