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
				if (Platform_IsExitRequested() || [dg applicationHasTerminated] || (g_mainWindow && g_mainWindow->isClosed()))
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

static CADisplayLink* g_displayLink = nil;
static RushViewController* g_viewController = nil; // outlives scenes: iOS may reconnect one

static void shutdownIOS()
{
	[g_displayLink invalidate];
	g_displayLink = nil;

	if (g_appConfig.onShutdown)
	{
		g_appConfig.onShutdown(g_appConfig.userData);
	}

	Platform_Shutdown();
}

@interface RushSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow* window;
@end

@implementation RushSceneDelegate

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions
{
	UIWindowScene* windowScene = (UIWindowScene*)scene;
	CGRect screenBounds = windowScene.coordinateSpace.bounds;

	self.window = [[UIWindow alloc] initWithWindowScene:windowScene];

	if (g_viewController)
	{
		self.window.rootViewController = g_viewController;
		[self.window makeKeyAndVisible];
		[g_viewController.view becomeFirstResponder];
		return;
	}

	RushViewController* vc = [[RushViewController alloc] initWithNibName:nil bundle:nil];
	RushMetalView* metalView = [[RushMetalView alloc] initWithFrame:screenBounds];
	metalView.contentScaleFactor = windowScene.screen.nativeScale;
	metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
	metalView.multipleTouchEnabled = YES;
	vc.view = metalView;
	g_viewController = vc;

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

	g_displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(renderFrame:)];
	if (g_appConfig.vsync > 0)
	{
		g_displayLink.preferredFramesPerSecond = 60 / g_appConfig.vsync;
	}
	[g_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)renderFrame:(CADisplayLink*)displayLink
{
	@autoreleasepool
	{
		if (Platform_IsExitRequested() || (g_mainWindow && g_mainWindow->isClosed()))
		{
			shutdownIOS();
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

@end

@interface RushAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation RushAppDelegate

- (UISceneConfiguration*)application:(UIApplication*)application
	configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession
	options:(UISceneConnectionOptions*)options
{
	UISceneConfiguration* config = [[[UISceneConfiguration alloc] initWithName:nil sessionRole:connectingSceneSession.role] autorelease];
	config.delegateClass = [RushSceneDelegate class];
	return config;
}

- (void)applicationWillTerminate:(UIApplication*)application
{
	shutdownIOS();
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
