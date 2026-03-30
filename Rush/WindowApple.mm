#include "WindowApple.h"
#include "MathCommon.h"
#include "UtilLog.h"

#if defined(RUSH_PLATFORM_MAC) || defined(RUSH_PLATFORM_IOS)

#import <QuartzCore/CAMetalLayer.h>

using namespace Rush;

#if defined(RUSH_PLATFORM_MAC)

@interface RushWindow: NSWindow
{
	@public WindowMac* parent;
}
@end

@implementation RushWindow
@end

@interface WindowMacInternal : NSObject<NSWindowDelegate>
{
	uint32_t windowCount;
}

+ (WindowMacInternal*)sharedDelegate;
- (id)init;
- (void)windowCreated:(NSWindow*)window;
- (void)windowWillClose:(NSNotification*)notification;
- (BOOL)windowShouldClose:(NSWindow*)window;
- (void)windowDidResize:(NSNotification*)notification;
- (void)windowDidEndLiveResize:(NSNotification*)notification;
- (void)windowDidChangeBackingProperties:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;

@end

@implementation WindowMacInternal

+ (WindowMacInternal*)sharedDelegate
{
	static id windowDelegate = [WindowMacInternal new];
	return windowDelegate;
}

- (id)init
{
	self = [super init];
	if (nil == self)
	{
		return nil;
	}

	self->windowCount = 0;
	return self;
}

- (void)windowCreated:(NSWindow*)window
{
	RUSH_ASSERT(window);

	[window setDelegate:self];

	RUSH_ASSERT(self->windowCount < ~0u);
	self->windowCount += 1;
}

- (void)windowWillClose:(NSNotification*)notification
{
	RUSH_UNUSED(notification);
}

- (BOOL)windowShouldClose:(NSWindow*)window
{
	RUSH_ASSERT(window);

	[window setDelegate:nil];

	RUSH_ASSERT(self->windowCount);
	self->windowCount -= 1;

	if (self->windowCount == 0)
	{
		[NSApp terminate:self];
		return false;
	}

	return true;
}

- (void)windowDidResize:(NSNotification*)notification
{
	RUSH_UNUSED(notification);
}

- (void)windowDidEndLiveResize:(NSNotification*)notification
{
	RushWindow* window = notification.object;
	NSView* contentView = [notification.object contentView];
	CALayer* layer = [contentView layer];
	CGSize frame = [layer frame].size;
	window->parent->updateResolutionScale();
	window->parent->processResize(frame.width, frame.height);
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
	RushWindow* window = notification.object;
	window->parent->updateResolutionScale();
}

- (void)windowDidBecomeKey:(NSNotification*)notification
{
	RushWindow* window = notification.object;
	window->parent->setFocused(true);
}

- (void)windowDidResignKey:(NSNotification*)notification
{
	RushWindow* window = notification.object;
	window->parent->setFocused(false);
}

@end

@interface RushView: NSView
@end

@implementation RushView
- (void)keyDown:(NSEvent *)event
{
	RUSH_UNUSED(event);
}
@end

namespace Rush
{

static Key translateKeyMac(const NSEvent* event)
{
	const u16 keyCode = [event keyCode];

	switch (keyCode)
	{
		case 82: return Key_KP0;
		case 83: return Key_KP1;
		case 84: return Key_KP2;
		case 85: return Key_KP3;
		case 86: return Key_KP4;
		case 87: return Key_KP5;
		case 88: return Key_KP6;
		case 89: return Key_KP7;
		case 91: return Key_KP8;
		case 92: return Key_KP9;
		case 65: return Key_KPDecimal;
		case 75: return Key_KPDivide;
		case 67: return Key_KPMultiply;
		case 78: return Key_KPSubtract;
		case 69: return Key_KPAdd;
		case 76: return Key_KPEnter;
		case 81: return Key_KPEqual;
		default: break;
	}

	NSString* key = [event charactersIgnoringModifiers];
	if ([key length] == 0)
	{
		return Key_Unknown;
	}

	unichar firstChar = [key characterAtIndex:0];
	switch(firstChar)
	{
		case ' ':				return Key_Space;
		case '\'':				return Key_Apostrophe;
		case ',':				return Key_Comma;
		case '-':				return Key_Minus;
		case '.':				return Key_Period;
		case '/':				return Key_Slash;
		case '0': 				return Key_0;
		case '1': 				return Key_1;
		case '2': 				return Key_2;
		case '3': 				return Key_3;
		case '4': 				return Key_4;
		case '5': 				return Key_5;
		case '6': 				return Key_6;
		case '7': 				return Key_7;
		case '8': 				return Key_8;
		case '9': 				return Key_9;
		case ';':				return Key_Semicolon;
		case '=':				return Key_Equal;
		case 'A':
		case 'a': 				return Key_A;
		case 'B':
		case 'b': 				return Key_B;
		case 'C':
		case 'c': 				return Key_C;
		case 'D':
		case 'd': 				return Key_D;
		case 'E':
		case 'e': 				return Key_E;
		case 'F':
		case 'f': 				return Key_F;
		case 'G':
		case 'g': 				return Key_G;
		case 'H':
		case 'h': 				return Key_H;
		case 'I':
		case 'i': 				return Key_I;
		case 'J':
		case 'j': 				return Key_J;
		case 'K':
		case 'k': 				return Key_K;
		case 'L':
		case 'l': 				return Key_L;
		case 'M':
		case 'm': 				return Key_M;
		case 'N':
		case 'n': 				return Key_N;
		case 'O':
		case 'o': 				return Key_O;
		case 'P':
		case 'p': 				return Key_P;
		case 'Q':
		case 'q': 				return Key_Q;
		case 'R':
		case 'r': 				return Key_R;
		case 'S':
		case 's': 				return Key_S;
		case 'T':
		case 't': 				return Key_T;
		case 'U':
		case 'u': 				return Key_U;
		case 'V':
		case 'v': 				return Key_V;
		case 'W':
		case 'w': 				return Key_W;
		case 'X':
		case 'x': 				return Key_X;
		case 'Y':
		case 'y': 				return Key_Y;
		case 'Z':
		case 'z': 				return Key_Z;
		case '[':				return Key_LeftBracket;
		case '\\':				return Key_Backslash;
		case ']':				return Key_RightBracket;
		case '\r':				return Key_Enter;
		case '\t':				return Key_Tab;
		case 27:				return Key_Escape;
		case 127:				return Key_Backspace;
		case NSDeleteFunctionKey:		return Key_Delete;
		case NSHomeFunctionKey:			return Key_Home;
		case NSEndFunctionKey:			return Key_End;
		case NSInsertFunctionKey:		return Key_Insert;
		case NSRightArrowFunctionKey: 	return Key_Right;
		case NSLeftArrowFunctionKey: 	return Key_Left;
		case NSDownArrowFunctionKey: 	return Key_Down;
		case NSUpArrowFunctionKey: 		return Key_Up;
		case NSF1FunctionKey: 			return Key_F1;
		case NSF2FunctionKey: 			return Key_F2;
		case NSF3FunctionKey: 			return Key_F3;
		case NSF4FunctionKey: 			return Key_F4;
		case NSF5FunctionKey: 			return Key_F5;
		case NSF6FunctionKey: 			return Key_F6;
		case NSF7FunctionKey: 			return Key_F7;
		case NSF8FunctionKey: 			return Key_F8;
		case NSF9FunctionKey: 			return Key_F9;
		case NSF10FunctionKey: 			return Key_F10;
		case NSF11FunctionKey: 			return Key_F11;
		case NSF12FunctionKey: 			return Key_F12;
		case NSF13FunctionKey: 			return Key_F13;
		case NSF14FunctionKey: 			return Key_F14;
		case NSF15FunctionKey: 			return Key_F15;
		case NSF16FunctionKey: 			return Key_F16;
		case NSF17FunctionKey: 			return Key_F17;
		case NSF18FunctionKey: 			return Key_F18;
		case NSF19FunctionKey: 			return Key_F19;
		case NSF20FunctionKey: 			return Key_F20;
		case NSF21FunctionKey: 			return Key_F21;
		case NSF22FunctionKey: 			return Key_F22;
		case NSF23FunctionKey: 			return Key_F23;
		case NSF24FunctionKey: 			return Key_F24;
		case NSPageUpFunctionKey: 		return Key_PageUp;
		case NSPageDownFunctionKey: 	return Key_PageDown;
		case NSPrintScreenFunctionKey: 	return Key_PrintScreen;
		case NSScrollLockFunctionKey: 	return Key_ScrollLock;
		case NSPauseFunctionKey: 		return Key_Pause;
		case '`':						return Key_Backquote;
		default:						return Key_Unknown;
	};
}

WindowMac::WindowMac(const WindowDesc& desc)
	: WindowApple(desc)
{
	u32 styleMask = NSWindowStyleMaskTitled
		| NSWindowStyleMaskClosable
		| NSWindowStyleMaskMiniaturizable;

	if (desc.resizable)
	{
		styleMask |= NSWindowStyleMaskResizable;
	}

	NSScreen* mainScreen = [NSScreen mainScreen];

	NSRect screenRect = [mainScreen frame];
	const float centerX = (screenRect.size.width  - (float)desc.width)*0.5f;
	const float centerY = (screenRect.size.height - (float)desc.height)*0.5f;
	NSRect rect = NSMakeRect(centerX, centerY, desc.width, desc.height);

	RushWindow* window = [[RushWindow alloc]
		initWithContentRect:rect
		styleMask:styleMask
		backing:NSBackingStoreBuffered
		defer:YES
	];
	m_nativeWindow = window;
	window->parent = this;

	[window.contentView setWantsLayer:YES];

	NSString* appName = (desc.caption && desc.caption[0])
		? [NSString stringWithUTF8String:desc.caption]
		: [[NSProcessInfo processInfo] processName];
	[window setTitle:appName];
	[window makeKeyAndOrderFront:window];
	[window setAcceptsMouseMovedEvents:YES];
	[window setBackgroundColor:[NSColor blackColor]];
	[[WindowMacInternal sharedDelegate] windowCreated:window];

	RushView* view = [[RushView alloc] init];
	[view setWantsLayer:YES];
	[window setContentView:view];
	[window makeFirstResponder:view];
	[view release];

	m_metalLayer = [CAMetalLayer layer];
	m_metalLayer.needsDisplayOnBoundsChange = YES;
	m_metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
	[window.contentView setLayer:m_metalLayer];
	updateResolutionScale();

	if (desc.fullScreen)
	{
		setFullscreen(true);
	}
	else if (desc.maximized)
	{
		[window setFrame:[mainScreen visibleFrame] display:YES animate:NO];
		NSSize contentSize = [[window contentView] frame].size;
		updateResolutionScale();
		processResize((float)contentSize.width, (float)contentSize.height);
	}
}

WindowMac::~WindowMac()
{
	[m_nativeWindow close];
}

void* WindowMac::nativeHandle()
{
	return m_nativeWindow;
}

void WindowMac::setCaption(const char* str)
{
	NSString* title = str ? [NSString stringWithUTF8String:str] : @"";
	[m_nativeWindow setTitle:title];
}

void WindowMac::setSize(const Tuple2i& size)
{
	[m_nativeWindow setContentSize:NSMakeSize(size.x, size.y)];
	processResize((float)size.x, (float)size.y);
}

void WindowMac::setPosition(const Tuple2i& position)
{
	const CGFloat screenHeight = [[NSScreen mainScreen] frame].size.height;
	const NSPoint point = NSMakePoint((CGFloat)position.x, screenHeight - (CGFloat)position.y);
	[m_nativeWindow setFrameTopLeftPoint:point];
	m_pos = position;
}

Box2 WindowMac::getSafeArea() const
{
	if (!m_nativeWindow)
	{
		return Box2(Vec2(0.0f), getSizeFloat());
	}

	if (@available(macOS 12.0, *))
	{
		NSRect safeRect = [m_nativeWindow contentLayoutRect];
		NSRect contentRect = [[m_nativeWindow contentView] bounds];
		float left   = (float)(safeRect.origin.x - contentRect.origin.x);
		float right  = (float)((contentRect.origin.x + contentRect.size.width) - (safeRect.origin.x + safeRect.size.width));
		// NSRect uses bottom-left origin; our convention is top-left
		float top    = (float)((contentRect.origin.y + contentRect.size.height) - (safeRect.origin.y + safeRect.size.height));
		float bottom = (float)(safeRect.origin.y - contentRect.origin.y);
		return Box2(Vec2(left, top), getSizeFloat() - Vec2(right, bottom));
	}

	return Box2(Vec2(0.0f), getSizeFloat());
}

void WindowMac::processResize(float newWidth, float newHeight)
{
	updateResolutionScale();
	Tuple2i pendingSize;
	pendingSize.x = int(newWidth);
	pendingSize.y = int(newHeight);

	if (m_size != pendingSize)
	{
		m_size = pendingSize;
		broadcast(WindowEvent::Resize(m_size.x, m_size.y));
	}
}

void WindowMac::updateResolutionScale()
{
	if (!m_nativeWindow)
	{
		return;
	}

	CGFloat scale = [m_nativeWindow backingScaleFactor];
	if (scale <= 0.0f)
	{
		scale = 1.0f;
	}

	const float scaleFloat = (float)scale;
	const bool scaleChanged = (m_resolutionScale.x != scaleFloat) || (m_resolutionScale.y != scaleFloat);
	m_resolutionScale = Vec2(scaleFloat, scaleFloat);

	bool sizeChanged = false;

	if (m_metalLayer)
	{
		NSView* contentView = [m_nativeWindow contentView];
		if (contentView)
		{
			const NSRect bounds = [contentView bounds];
			m_metalLayer.frame = bounds;
			const Tuple2i pendingSize{(int)bounds.size.width, (int)bounds.size.height};
			if (pendingSize.x > 0 && pendingSize.y > 0 && m_size != pendingSize)
			{
				m_size = pendingSize;
				sizeChanged = true;
			}
			m_metalLayer.contentsScale = scale;
			m_metalLayer.drawableSize = CGSizeMake(
				(CGFloat)m_size.x * scale,
				(CGFloat)m_size.y * scale);
		}
	}

	if (sizeChanged || scaleChanged)
	{
		broadcast(WindowEvent::Resize(m_size.x, m_size.y));
	}
}

void WindowMac::setMouseLock(bool state)
{
	if (m_mouseLocked == state)
	{
		return;
	}
	m_mouseLocked = state;
	CGAssociateMouseAndMouseCursorPosition(!state);
	if (state)
	{
		m_preLockMousePos = m_mouse.pos;
		[NSCursor hide];
	}
	else
	{
		m_mouse.pos = m_preLockMousePos;
		[NSCursor unhide];
	}
}

bool WindowMac::setFullscreen(bool state)
{
	if (state == m_fullScreen)
	{
		return true;
	}

	if (state)
	{
		m_windowedSize = m_size;

		const NSRect frame = [m_nativeWindow frame];
		m_windowedPos.x = (int)frame.origin.x;
		m_windowedPos.y = (int)frame.origin.y;
		m_windowedStyleMask = (u32)[m_nativeWindow styleMask];

		NSScreen* screen = [m_nativeWindow screen] ?: [NSScreen mainScreen];
		const NSRect screenFrame = [screen frame];

		[m_nativeWindow setStyleMask:NSWindowStyleMaskBorderless];
		[m_nativeWindow setFrame:screenFrame display:YES];
		[m_nativeWindow setLevel:NSMainMenuWindowLevel + 1];
	}
	else
	{
		[m_nativeWindow setStyleMask:(NSWindowStyleMask)m_windowedStyleMask];
		[m_nativeWindow setLevel:NSNormalWindowLevel];

		const NSRect restoreFrame = NSMakeRect(
			m_windowedPos.x, m_windowedPos.y,
			m_windowedSize.x, m_windowedSize.y);
		[m_nativeWindow setFrame:restoreFrame display:YES];
	}

	m_fullScreen = state;

	const NSRect contentRect = [[m_nativeWindow contentView] frame];
	processResize((float)contentRect.size.width, (float)contentRect.size.height);

	return true;
}

bool WindowMac::processEvent(NSEvent* event)
{
	NSEventType eventType = [event type];
	switch (eventType)
	{
		case NSEventTypeLeftMouseDragged:
		case NSEventTypeRightMouseDragged:
		case NSEventTypeOtherMouseDragged:
		case NSEventTypeMouseMoved:
		{
			if (m_mouseLocked)
			{
				const float dx = (float)[event deltaX];
				const float dy = (float)[event deltaY];
				m_mouse.pos += Vec2(dx, dy);
			}
			else
			{
				if ([event window] != m_nativeWindow)
				{
					return false;
				}

				NSPoint mouseLocation = [event locationInWindow];
				if (m_nativeWindow)
				{
					NSView* contentView = [m_nativeWindow contentView];
					if (contentView)
					{
						mouseLocation = [contentView convertPoint:mouseLocation fromView:nil];
					}
				}
				const Tuple2i size = getSize();
				const float xPos = clamp((float)mouseLocation.x, 0.0f, (float)size.x);
				const float yPos = clamp((float)size.y - (float)mouseLocation.y, 0.0f, (float)size.y);
				m_mouse.pos = Vec2((float)xPos, (float)yPos);
			}
			broadcast(WindowEvent::MouseMove(m_mouse.pos));
			return true;
		}
		case NSEventTypeLeftMouseDown:
		{
			const bool doubleClick = [event clickCount] >= 2;
			m_mouse.buttons[0] = true;
			m_mouse.doubleclick = doubleClick;
			broadcast(WindowEvent::MouseDown(m_mouse.pos, 0, doubleClick));
			return true;
		}
		case NSEventTypeLeftMouseUp:
		{
			m_mouse.buttons[0] = false;
			broadcast(WindowEvent::MouseUp(m_mouse.pos, 0));
			return true;
		}
		case NSEventTypeRightMouseDown:
		{
			const bool doubleClick = [event clickCount] >= 2;
			m_mouse.buttons[1] = true;
			m_mouse.doubleclick = doubleClick;
			broadcast(WindowEvent::MouseDown(m_mouse.pos, 1, doubleClick));
			return true;
		}
		case NSEventTypeRightMouseUp:
		{
			m_mouse.buttons[1] = false;
			broadcast(WindowEvent::MouseUp(m_mouse.pos, 1));
			return true;
		}
		case NSEventTypeOtherMouseDown:
		{
			const int button = (int)[event buttonNumber];
			if (button >= 0 && button < 10)
			{
				const bool doubleClick = [event clickCount] >= 2;
				m_mouse.buttons[button] = true;
				m_mouse.doubleclick = doubleClick;
				broadcast(WindowEvent::MouseDown(m_mouse.pos, button, doubleClick));
			}
			return true;
		}
		case NSEventTypeOtherMouseUp:
		{
			const int button = (int)[event buttonNumber];
			if (button >= 0 && button < 10)
			{
				m_mouse.buttons[button] = false;
				broadcast(WindowEvent::MouseUp(m_mouse.pos, button));
			}
			return true;
		}
		case NSEventTypeScrollWheel:
		{
			const float deltaX = [event deltaX] * 0.25f;
			const float deltaY = [event deltaY] * 0.25f;
			m_scrollAccumH += deltaX;
			m_scrollAccumV += deltaY;
			const int wholeH = (int)m_scrollAccumH;
			const int wholeV = (int)m_scrollAccumV;
			m_scrollAccumH -= (float)wholeH;
			m_scrollAccumV -= (float)wholeV;
			m_mouse.wheelH += wholeH;
			m_mouse.wheelV += wholeV;
			broadcast(WindowEvent::Scroll(deltaX, deltaY));
			return true;
		}
		case NSEventTypeKeyDown:
		{
			Key key = translateKeyMac(event);

			if (key == Key_Enter
				&& ([event modifierFlags] & NSEventModifierFlagOption)
				&& getDesc().handleShortcutFullScreen)
			{
				toggleFullscreen();
				return true;
			}

			m_keyboard.keys[key] = true;
			auto e = WindowEvent::KeyDown(key);
			broadcast(e);

			NSString* chars = [event characters];
			if ([chars length] != 0)
			{
				unichar ch = [chars characterAtIndex:0];
				if (ch >= 32 && ch != 127 && ch < 0xF700)
				{
					broadcast(WindowEvent::Char(ch));
				}
			}

			return true;
		}
		case NSEventTypeKeyUp:
		{
			Key key = translateKeyMac(event);
			m_keyboard.keys[key] = false;
			auto e = WindowEvent::KeyUp(key);
			broadcast(e);
			return true;
		}
		case NSEventTypeFlagsChanged:
		{
			u16 keyCode = [event keyCode];

			Key key = Key_Unknown;
			switch(keyCode)
			{
			case 56:
				key = Key_LeftShift;
				break;
			case 60:
				key = Key_RightShift;
				break;
			case 59:
				key = Key_LeftControl;
				break;
			case 55:
				key = Key_LeftSuper;
				break;
			case 54:
				key = Key_RightSuper;
				break;
			case 58:
				key = Key_LeftAlt;
				break;
			case 61:
				key = Key_RightAlt;
				break;
			case 62:
				key = Key_RightControl;
				break;
			case 57:
				key = Key_CapsLock;
				break;
			default:
				key = Key_Unknown;
				break;
			}

			if (key != Key_Unknown)
			{
				if (m_keyboard.keys[key])
				{
					auto e = WindowEvent::KeyUp(key);
					broadcast(e);
					m_keyboard.keys[key] = false;
				}
				else
				{
					auto e = WindowEvent::KeyDown(key);
					broadcast(e);
					m_keyboard.keys[key] = true;
				}
			}

			return true;
		}
		default:
			return false;
	}
}

} // namespace Rush

#endif // RUSH_PLATFORM_MAC

#if defined(RUSH_PLATFORM_IOS)

static Key translateKeyIOS(UIKeyboardHIDUsage usage)
{
	switch (usage)
	{
		case UIKeyboardHIDUsageKeyboardA: return Key_A;
		case UIKeyboardHIDUsageKeyboardB: return Key_B;
		case UIKeyboardHIDUsageKeyboardC: return Key_C;
		case UIKeyboardHIDUsageKeyboardD: return Key_D;
		case UIKeyboardHIDUsageKeyboardE: return Key_E;
		case UIKeyboardHIDUsageKeyboardF: return Key_F;
		case UIKeyboardHIDUsageKeyboardG: return Key_G;
		case UIKeyboardHIDUsageKeyboardH: return Key_H;
		case UIKeyboardHIDUsageKeyboardI: return Key_I;
		case UIKeyboardHIDUsageKeyboardJ: return Key_J;
		case UIKeyboardHIDUsageKeyboardK: return Key_K;
		case UIKeyboardHIDUsageKeyboardL: return Key_L;
		case UIKeyboardHIDUsageKeyboardM: return Key_M;
		case UIKeyboardHIDUsageKeyboardN: return Key_N;
		case UIKeyboardHIDUsageKeyboardO: return Key_O;
		case UIKeyboardHIDUsageKeyboardP: return Key_P;
		case UIKeyboardHIDUsageKeyboardQ: return Key_Q;
		case UIKeyboardHIDUsageKeyboardR: return Key_R;
		case UIKeyboardHIDUsageKeyboardS: return Key_S;
		case UIKeyboardHIDUsageKeyboardT: return Key_T;
		case UIKeyboardHIDUsageKeyboardU: return Key_U;
		case UIKeyboardHIDUsageKeyboardV: return Key_V;
		case UIKeyboardHIDUsageKeyboardW: return Key_W;
		case UIKeyboardHIDUsageKeyboardX: return Key_X;
		case UIKeyboardHIDUsageKeyboardY: return Key_Y;
		case UIKeyboardHIDUsageKeyboardZ: return Key_Z;
		case UIKeyboardHIDUsageKeyboard1: return Key_1;
		case UIKeyboardHIDUsageKeyboard2: return Key_2;
		case UIKeyboardHIDUsageKeyboard3: return Key_3;
		case UIKeyboardHIDUsageKeyboard4: return Key_4;
		case UIKeyboardHIDUsageKeyboard5: return Key_5;
		case UIKeyboardHIDUsageKeyboard6: return Key_6;
		case UIKeyboardHIDUsageKeyboard7: return Key_7;
		case UIKeyboardHIDUsageKeyboard8: return Key_8;
		case UIKeyboardHIDUsageKeyboard9: return Key_9;
		case UIKeyboardHIDUsageKeyboard0: return Key_0;
		case UIKeyboardHIDUsageKeyboardReturnOrEnter: return Key_Enter;
		case UIKeyboardHIDUsageKeyboardEscape: return Key_Escape;
		case UIKeyboardHIDUsageKeyboardDeleteOrBackspace: return Key_Backspace;
		case UIKeyboardHIDUsageKeyboardTab: return Key_Tab;
		case UIKeyboardHIDUsageKeyboardSpacebar: return Key_Space;
		case UIKeyboardHIDUsageKeyboardHyphen: return Key_Minus;
		case UIKeyboardHIDUsageKeyboardEqualSign: return Key_Equal;
		case UIKeyboardHIDUsageKeyboardOpenBracket: return Key_LeftBracket;
		case UIKeyboardHIDUsageKeyboardCloseBracket: return Key_RightBracket;
		case UIKeyboardHIDUsageKeyboardBackslash: return Key_Backslash;
		case UIKeyboardHIDUsageKeyboardSemicolon: return Key_Semicolon;
		case UIKeyboardHIDUsageKeyboardQuote: return Key_Apostrophe;
		case UIKeyboardHIDUsageKeyboardGraveAccentAndTilde: return Key_Backquote;
		case UIKeyboardHIDUsageKeyboardComma: return Key_Comma;
		case UIKeyboardHIDUsageKeyboardPeriod: return Key_Period;
		case UIKeyboardHIDUsageKeyboardSlash: return Key_Slash;
		case UIKeyboardHIDUsageKeyboardCapsLock: return Key_CapsLock;
		case UIKeyboardHIDUsageKeyboardF1: return Key_F1;
		case UIKeyboardHIDUsageKeyboardF2: return Key_F2;
		case UIKeyboardHIDUsageKeyboardF3: return Key_F3;
		case UIKeyboardHIDUsageKeyboardF4: return Key_F4;
		case UIKeyboardHIDUsageKeyboardF5: return Key_F5;
		case UIKeyboardHIDUsageKeyboardF6: return Key_F6;
		case UIKeyboardHIDUsageKeyboardF7: return Key_F7;
		case UIKeyboardHIDUsageKeyboardF8: return Key_F8;
		case UIKeyboardHIDUsageKeyboardF9: return Key_F9;
		case UIKeyboardHIDUsageKeyboardF10: return Key_F10;
		case UIKeyboardHIDUsageKeyboardF11: return Key_F11;
		case UIKeyboardHIDUsageKeyboardF12: return Key_F12;
		case UIKeyboardHIDUsageKeyboardPrintScreen: return Key_PrintScreen;
		case UIKeyboardHIDUsageKeyboardScrollLock: return Key_ScrollLock;
		case UIKeyboardHIDUsageKeyboardPause: return Key_Pause;
		case UIKeyboardHIDUsageKeyboardInsert: return Key_Insert;
		case UIKeyboardHIDUsageKeyboardHome: return Key_Home;
		case UIKeyboardHIDUsageKeyboardPageUp: return Key_PageUp;
		case UIKeyboardHIDUsageKeyboardDeleteForward: return Key_Delete;
		case UIKeyboardHIDUsageKeyboardEnd: return Key_End;
		case UIKeyboardHIDUsageKeyboardPageDown: return Key_PageDown;
		case UIKeyboardHIDUsageKeyboardRightArrow: return Key_Right;
		case UIKeyboardHIDUsageKeyboardLeftArrow: return Key_Left;
		case UIKeyboardHIDUsageKeyboardDownArrow: return Key_Down;
		case UIKeyboardHIDUsageKeyboardUpArrow: return Key_Up;
		case UIKeyboardHIDUsageKeyboardLeftControl: return Key_LeftControl;
		case UIKeyboardHIDUsageKeyboardLeftShift: return Key_LeftShift;
		case UIKeyboardHIDUsageKeyboardLeftAlt: return Key_LeftAlt;
		case UIKeyboardHIDUsageKeyboardLeftGUI: return Key_LeftSuper;
		case UIKeyboardHIDUsageKeyboardRightControl: return Key_RightControl;
		case UIKeyboardHIDUsageKeyboardRightShift: return Key_RightShift;
		case UIKeyboardHIDUsageKeyboardRightAlt: return Key_RightAlt;
		case UIKeyboardHIDUsageKeyboardRightGUI: return Key_RightSuper;
		default: return Key_Unknown;
	}
}

@implementation RushMetalView

+ (Class)layerClass
{
	return [CAMetalLayer class];
}

- (BOOL)canBecomeFirstResponder
{
	return YES;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
	WindowIOS* w = static_cast<WindowIOS*>(parent);
	if (!w) { return; }

	for (UITouch* touch in touches)
	{
		CGPoint loc = [touch locationInView:self];
		w->touchBegan((__bridge void*)touch, Vec2((float)loc.x, (float)loc.y));
	}
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
	WindowIOS* w = static_cast<WindowIOS*>(parent);
	if (!w) { return; }

	for (UITouch* touch in touches)
	{
		CGPoint loc = [touch locationInView:self];
		w->touchMoved((__bridge void*)touch, Vec2((float)loc.x, (float)loc.y));
	}
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
	WindowIOS* w = static_cast<WindowIOS*>(parent);
	if (!w) { return; }

	for (UITouch* touch in touches)
	{
		CGPoint loc = [touch locationInView:self];
		w->touchEnded((__bridge void*)touch, Vec2((float)loc.x, (float)loc.y));
	}
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
	[self touchesEnded:touches withEvent:event];
}

- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
	WindowIOS* w = static_cast<WindowIOS*>(parent);
	if (!w) { [super pressesBegan:presses withEvent:event]; return; }

	bool handled = false;
	for (UIPress* press in presses)
	{
		if (press.key)
		{
			const Key key = translateKeyIOS((UIKeyboardHIDUsage)press.key.keyCode);
			if (key != Key_Unknown)
			{
				w->setKeyDown(key);
				w->broadcast(WindowEvent::KeyDown(key));
				handled = true;
			}
		}
	}

	if (!handled) { [super pressesBegan:presses withEvent:event]; }
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
	WindowIOS* w = static_cast<WindowIOS*>(parent);
	if (!w) { [super pressesEnded:presses withEvent:event]; return; }

	bool handled = false;
	for (UIPress* press in presses)
	{
		if (press.key)
		{
			const Key key = translateKeyIOS((UIKeyboardHIDUsage)press.key.keyCode);
			if (key != Key_Unknown)
			{
				w->setKeyUp(key);
				w->broadcast(WindowEvent::KeyUp(key));
				handled = true;
			}
		}
	}

	if (!handled) { [super pressesEnded:presses withEvent:event]; }
}

- (void)pressesCancelled:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
	[self pressesEnded:presses withEvent:event];
}

@end

@implementation RushViewController

- (BOOL)prefersStatusBarHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (BOOL)shouldAutorotate { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }

- (void)viewDidLayoutSubviews
{
	[super viewDidLayoutSubviews];
	if (parent)
	{
		static_cast<WindowIOS*>(parent)->updateResolutionScale();
	}
}

@end

namespace Rush
{

WindowIOS::WindowIOS(const WindowDesc& desc)
	: WindowApple(desc)
{
	m_fullScreen = true;
	m_focused = true;
}

WindowIOS::~WindowIOS()
{
}

void* WindowIOS::nativeHandle()
{
	return m_uiWindow;
}

void WindowIOS::setViewController(RushViewController* vc)
{
	m_viewController = vc;
	vc->parent = this;

	RushMetalView* metalView = (RushMetalView*)vc.view;
	metalView->parent = this;

	m_metalLayer = (CAMetalLayer*)metalView.layer;
	m_metalLayer.framebufferOnly = NO;

	updateResolutionScale();
}

Box2 WindowIOS::getSafeArea() const
{
	if (!m_viewController)
	{
		return Box2(Vec2(0.0f), getSizeFloat());
	}

	UIEdgeInsets insets = m_viewController.view.safeAreaInsets;
	return Box2(
		Vec2((float)insets.left, (float)insets.top),
		getSizeFloat() - Vec2((float)insets.right, (float)insets.bottom));
}

int WindowIOS::findTouchByNativeId(void* nativeId) const
{
	for (size_t i = 0; i < m_nativeTouchIds.size(); ++i)
	{
		if (m_nativeTouchIds[i] == nativeId)
		{
			return (int)i;
		}
	}
	return -1;
}

int WindowIOS::findTouchById(u64 id) const
{
	for (size_t i = 0; i < m_touches.size(); ++i)
	{
		if (m_touches[i].id == id)
		{
			return (int)i;
		}
	}
	return -1;
}

u64 WindowIOS::touchBegan(void* nativeId, const Vec2& pos)
{
	const u64 id = m_nextTouchId++;
	const bool isFirst = m_touches.empty();

	m_touches.push_back({id, pos});
	m_nativeTouchIds.push_back(nativeId);

	if (isFirst)
	{
		m_mouse.pos = pos;
		m_mouse.buttons[0] = true;
		broadcast(WindowEvent::MouseMove(pos));
		broadcast(WindowEvent::MouseDown(pos, 0, false));
	}

	return id;
}

void WindowIOS::touchMoved(void* nativeId, const Vec2& pos)
{
	const int idx = findTouchByNativeId(nativeId);
	if (idx < 0)
	{
		return;
	}

	m_touches[idx].pos = pos;

	if (idx == 0)
	{
		m_mouse.pos = pos;
		broadcast(WindowEvent::MouseMove(pos));
	}
}

void WindowIOS::touchEnded(void* nativeId, const Vec2& pos)
{
	const int idx = findTouchByNativeId(nativeId);
	if (idx < 0)
	{
		return;
	}

	if (idx == 0)
	{
		m_mouse.pos = pos;
		m_mouse.buttons[0] = false;
		broadcast(WindowEvent::MouseUp(pos, 0));
	}

	const int last = (int)m_touches.size() - 1;
	if (idx != last)
	{
		m_touches[idx] = m_touches[last];
		m_nativeTouchIds[idx] = m_nativeTouchIds[last];
	}
	m_touches.pop_back();
	m_nativeTouchIds.pop_back();
}

void WindowIOS::updateResolutionScale()
{
	if (!m_metalLayer || !m_viewController)
	{
		return;
	}

	UIView* view = m_viewController.view;
	const CGFloat scale = view.contentScaleFactor;

	const CGRect bounds = view.bounds;
	const Tuple2i pendingSize{(int)bounds.size.width, (int)bounds.size.height};
	const float scaleFloat = (float)scale;

	m_resolutionScale = Vec2(scaleFloat, scaleFloat);
	m_metalLayer.contentsScale = scale;

	if (pendingSize.x > 0 && pendingSize.y > 0)
	{
		m_metalLayer.drawableSize = CGSizeMake(
			(CGFloat)pendingSize.x * scale,
			(CGFloat)pendingSize.y * scale);

		if (m_size != pendingSize)
		{
			m_size = pendingSize;
			broadcast(WindowEvent::Resize(m_size.x, m_size.y));
		}
	}
}

} // namespace Rush

#endif // RUSH_PLATFORM_IOS

#endif // RUSH_PLATFORM_MAC || RUSH_PLATFORM_IOS
