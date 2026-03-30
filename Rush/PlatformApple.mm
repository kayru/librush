#include "Platform.h"
#include "WindowApple.h"
#include "UtilLog.h"
#include "GfxDevice.h"

#if defined(RUSH_PLATFORM_MAC) || defined(RUSH_PLATFORM_IOS)

#include <sys/sysctl.h>
#include <unistd.h>
#include <cstdlib>

using namespace Rush;

namespace Rush
{

extern Window*     g_mainWindow;
extern GfxDevice*  g_mainGfxDevice;
extern GfxContext* g_mainGfxContext;

void        Platform_TerminateProcess(int status) { exit(status); }
GfxDevice*  Platform_GetGfxDevice() { return g_mainGfxDevice; }
GfxContext* Platform_GetGfxContext() { return g_mainGfxContext; }
Window*     Platform_GetWindow() { return g_mainWindow; }

bool Platform_IsDebuggerPresent()
{
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
	struct kinfo_proc info = {};
	size_t size = sizeof(info);
	sysctl(mib, 4, &info, &size, nullptr, 0);
	return (info.kp_proc.p_flag & P_TRACED) != 0;
}

const char* Platform_GetExecutableDirectory()
{
	static char result[4096] = {};
	if (result[0] == 0)
	{
		NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
		strncpy(result, [bundlePath UTF8String], sizeof(result) - 1);
	}
	return result;
}

}

#if defined(RUSH_PLATFORM_MAC)

#import <Cocoa/Cocoa.h>

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

Window* Platform_CreateWindow(const WindowDesc& desc) { return new WindowMac(desc); }

void Platform_Run(PlatformCallback_Update onUpdate, void* userData)
{
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

		while (true)
		{
			@autoreleasepool
			{
				if ([dg applicationHasTerminated] || (g_mainWindow && g_mainWindow->isClosed()))
				{
					break;
				}

				while (NSEvent* event = [NSApp
						nextEventMatchingMask:NSEventMaskAny
						untilDate:[NSDate distantPast]
						inMode:NSDefaultRunLoopMode
						dequeue:YES])
				{
					if (g_mainWindow)
					{
						WindowMac* window = reinterpret_cast<WindowMac*>(g_mainWindow);
						window->processEvent(event);
					}
					[NSApp sendEvent:event];
					[NSApp updateWindows];
				}

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
}

}

#endif // RUSH_PLATFORM_MAC

#if defined(RUSH_PLATFORM_IOS)

#import <UIKit/UIKit.h>

static AppConfig g_appConfig;
static WindowIOS* g_pendingWindow = nullptr;

namespace Rush
{

Window* Platform_CreateWindow(const WindowDesc& desc)
{
	if (g_pendingWindow)
	{
		WindowIOS* w = g_pendingWindow;
		g_pendingWindow = nullptr;
		return w;
	}
	return new WindowIOS(desc);
}

void Platform_Run(PlatformCallback_Update, void*)
{
	RUSH_ASSERT_MSG(false, "Platform_Run should not be called directly on iOS");
}

}

@interface RushAppDelegate : UIResponder <UIApplicationDelegate>
{
	CADisplayLink* m_displayLink;
}
@property (strong, nonatomic) UIWindow* window;
@end

@implementation RushAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
	CGRect screenBounds = [[UIScreen mainScreen] bounds];

	self.window = [[UIWindow alloc] initWithFrame:screenBounds];

	RushViewController* vc = [[RushViewController alloc] initWithNibName:nil bundle:nil];
	RushMetalView* metalView = [[RushMetalView alloc] initWithFrame:screenBounds];
	metalView.contentScaleFactor = [UIScreen mainScreen].nativeScale;
	metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	metalView.multipleTouchEnabled = YES;
	vc.view = metalView;

	self.window.rootViewController = vc;
	[self.window makeKeyAndVisible];
	[metalView becomeFirstResponder];

	// Create the Rush window and set up its metal layer BEFORE Platform_Startup,
	// so the GfxDevice sees a valid CAMetalLayer during initialization.
	WindowDesc windowDesc;
	windowDesc.width = (int)screenBounds.size.width;
	windowDesc.height = (int)screenBounds.size.height;
	windowDesc.caption = g_appConfig.name;

	WindowIOS* rushWindow = new WindowIOS(windowDesc);
	rushWindow->setUIWindow(self.window);
	rushWindow->setViewController(vc);

	g_pendingWindow = rushWindow;

	Platform_Startup(g_appConfig);

	if (g_appConfig.onStartup)
	{
		g_appConfig.onStartup(g_appConfig.userData);
	}

	m_displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(renderFrame:)];
	if (g_appConfig.vsync > 0)
	{
		m_displayLink.preferredFramesPerSecond = 60 / g_appConfig.vsync;
	}
	[m_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];

	return YES;
}

- (void)renderFrame:(CADisplayLink*)displayLink
{
	@autoreleasepool
	{
		if (g_mainWindow && g_mainWindow->isClosed())
		{
			[m_displayLink invalidate];
			m_displayLink = nil;

			if (g_appConfig.onShutdown)
			{
				g_appConfig.onShutdown(g_appConfig.userData);
			}

			Platform_Shutdown();
			Platform_TerminateProcess(0);
			return;
		}

		Gfx_BeginFrame();
		if (g_appConfig.onUpdate)
		{
			g_appConfig.onUpdate(g_appConfig.userData);
		}
		Gfx_EndFrame();
		Gfx_Present();
	}
}

- (void)applicationWillTerminate:(UIApplication*)application
{
	[m_displayLink invalidate];
	m_displayLink = nil;

	if (g_appConfig.onShutdown)
	{
		g_appConfig.onShutdown(g_appConfig.userData);
	}

	Platform_Shutdown();
}

@end

namespace Rush
{

int Platform_Main(const AppConfig& cfg)
{
	g_appConfig = cfg;

	@autoreleasepool
	{
		return UIApplicationMain(cfg.argc, cfg.argv, nil, NSStringFromClass([RushAppDelegate class]));
	}
}

}

#endif // RUSH_PLATFORM_IOS

#endif // RUSH_PLATFORM_MAC || RUSH_PLATFORM_IOS
