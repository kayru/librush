#include "Rush.h"

#ifdef RUSH_PLATFORM_LINUX

#include "Platform.h"
#include "WindowXCB.h"
#include "UtilLog.h"

#include <xcb/xcb_keysyms.h>
#include <X11/Xutil.h>

namespace Rush
{

namespace
{
	xcb_connection_t* g_xcbConnection = nullptr;
	xcb_screen_t* g_xcbScreen = nullptr;
	u32 g_xcbConnectionRefCount = 0;
	u32 g_xcbKeyMap[256];

}

WindowXCB::WindowXCB(const WindowDesc& desc)
: Window(desc)
{
	if (g_xcbConnectionRefCount == 0)
	{
		int screen = 0;

		g_xcbConnection = xcb_connect(nullptr, &screen);
		if (!g_xcbConnection)
		{
			RUSH_LOG_FATAL("xcb_connect failed");
		}

		if (xcb_connection_has_error(g_xcbConnection))
		{
			RUSH_LOG_FATAL("xcb connection is invalid");
		}

		const xcb_setup_t* setup = xcb_get_setup(g_xcbConnection);
		xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
		while (screen-- > 0) xcb_screen_next(&iter);

		g_xcbScreen = iter.data;

		RUSH_ASSERT(g_xcbScreen);

		xcb_key_symbols_t* symbols = xcb_key_symbols_alloc(g_xcbConnection);
		for (u32 i=0; i<256; ++i)
		{
			g_xcbKeyMap[i] = xcb_key_symbols_get_keysym(symbols, i, 0);
		}
		xcb_key_symbols_free(symbols);
	}

	g_xcbConnectionRefCount++;

	m_nativeHandle = xcb_generate_id(g_xcbConnection);

	uint32_t valueMask, valueList[32];
	valueMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	valueList[0] = g_xcbScreen->black_pixel;
	valueList[1] =
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE |
		XCB_EVENT_MASK_ENTER_WINDOW |
		XCB_EVENT_MASK_LEAVE_WINDOW |
		XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_POINTER_MOTION_HINT |
		XCB_EVENT_MASK_BUTTON_1_MOTION |
		XCB_EVENT_MASK_BUTTON_2_MOTION |
		XCB_EVENT_MASK_BUTTON_3_MOTION |
		XCB_EVENT_MASK_BUTTON_4_MOTION |
		XCB_EVENT_MASK_BUTTON_5_MOTION |
		XCB_EVENT_MASK_BUTTON_MOTION |
		XCB_EVENT_MASK_KEYMAP_STATE |
		XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_VISIBILITY_CHANGE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_RESIZE_REDIRECT |
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
		XCB_EVENT_MASK_FOCUS_CHANGE |
		XCB_EVENT_MASK_PROPERTY_CHANGE |
		XCB_EVENT_MASK_COLOR_MAP_CHANGE |
		XCB_EVENT_MASK_OWNER_GRAB_BUTTON;

	xcb_create_window(g_xcbConnection, XCB_COPY_FROM_PARENT, m_nativeHandle, g_xcbScreen->root, 0, 0, u16(desc.width), u16(desc.height),
						0, XCB_WINDOW_CLASS_INPUT_OUTPUT, g_xcbScreen->root_visual, valueMask, valueList);

	xcb_map_window(g_xcbConnection, m_nativeHandle);

	//const uint32_t coords[] = {100, 100}; // TODO: place in the center of the screen
	//xcb_configure_window(g_xcbConnection, m_nativeHandle, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, coords);

	xcb_flush(g_xcbConnection);
}

WindowXCB::~WindowXCB()
{
	xcb_destroy_window(g_xcbConnection, m_nativeHandle);

	RUSH_ASSERT(g_xcbConnectionRefCount != 0);
	if (--g_xcbConnectionRefCount == 0)
	{
		xcb_disconnect(g_xcbConnection);
	}
}

void* WindowXCB::nativeConnection()
{
	return g_xcbConnection;
}

void WindowXCB::setCaption(const char* str)
{
	if (str == nullptr)
	{
		str = "";
	}

	m_caption = str;

	RUSH_LOG_FATAL("%s is not implemented", __PRETTY_FUNCTION__); // TODO
}

void WindowXCB::setSize(const Tuple2i& size)
{
	RUSH_LOG_FATAL("%s is not implemented", __PRETTY_FUNCTION__); // TODO
}

bool WindowXCB::setFullscreen(bool wantFullScreen)
{
	RUSH_LOG_FATAL("%s is not implemented", __PRETTY_FUNCTION__); // TODO

	return false;
}

Key translateKeyXCB(xcb_keycode_t code)
{
	xcb_keysym_t sym = g_xcbKeyMap[code];
	switch (sym)
	{
	case XK_space: return Key_Space;
	case XK_comma: return Key_Comma;
	case XK_minus: return Key_Minus;
	case XK_period: return Key_Period;
	case XK_slash: return Key_Slash;
	case '0': return Key_0;
	case '1': return Key_1;
	case '2': return Key_2;
	case '3': return Key_3;
	case '4': return Key_4;
	case '5': return Key_5;
	case '6': return Key_6;
	case '7': return Key_7;
	case '8': return Key_8;
	case '9': return Key_9;
	case XK_semicolon: return Key_Semicolon;
	case XK_equal: return Key_Equal;
	case 'a': return Key_A;
	case 'b': return Key_B;
	case 'c': return Key_C;
	case 'd': return Key_D;
	case 'e': return Key_E;
	case 'f': return Key_F;
	case 'g': return Key_G;
	case 'h': return Key_H;
	case 'i': return Key_I;
	case 'j': return Key_J;
	case 'k': return Key_K;
	case 'l': return Key_L;
	case 'm': return Key_M;
	case 'n': return Key_N;
	case 'o': return Key_O;
	case 'p': return Key_P;
	case 'q': return Key_Q;
	case 'r': return Key_R;
	case 's': return Key_S;
	case 't': return Key_T;
	case 'u': return Key_U;
	case 'v': return Key_V;
	case 'w': return Key_W;
	case 'x': return Key_X;
	case 'y': return Key_Y;
	case 'z': return Key_Z;
	case XK_bracketleft: return Key_LeftBracket;
	case XK_backslash: return Key_Backslash;
	case XK_bracketright: return Key_RightBracket;
	case XK_Escape: return Key_Escape;
	case XK_Return: return Key_Enter;
	case XK_Tab: return Key_Tab;
	case XK_BackSpace: return Key_Backspace;
	case XK_Insert: return Key_Insert;
	case XK_Delete: return Key_Delete;
	case XK_Right: return Key_Right;
	case XK_Left: return Key_Left;
	case XK_Down: return Key_Down;
	case XK_Up: return Key_Up;
	case XK_Page_Up: return Key_PageUp;
	case XK_Page_Down: return Key_PageDown;
	case XK_Home: return Key_Home;
	case XK_End: return Key_End;
	case XK_Caps_Lock: return Key_CapsLock;
	case XK_Scroll_Lock: return Key_ScrollLock;
	case XK_Num_Lock: return Key_NumLock;
	case XK_Print: return Key_PrintScreen;
	case XK_Pause: return Key_Pause;
	case XK_F1: return Key_F1;
	case XK_F2: return Key_F2;
	case XK_F3: return Key_F3;
	case XK_F4: return Key_F4;
	case XK_F5: return Key_F5;
	case XK_F6: return Key_F6;
	case XK_F7: return Key_F7;
	case XK_F8: return Key_F8;
	case XK_F9: return Key_F9;
	case XK_F10: return Key_F10;
	case XK_F11: return Key_F11;
	case XK_F12: return Key_F12;
	case XK_F13: return Key_F13;
	case XK_F14: return Key_F14;
	case XK_F15: return Key_F15;
	case XK_F16: return Key_F16;
	case XK_F17: return Key_F17;
	case XK_F18: return Key_F18;
	case XK_F19: return Key_F19;
	case XK_F20: return Key_F20;
	case XK_F21: return Key_F21;
	case XK_F22: return Key_F22;
	case XK_F23: return Key_F23;
	case XK_F24: return Key_F24;
	case XK_Shift_L: return Key_LeftShift;
	case XK_Control_L: return Key_LeftControl;
	case XK_Alt_L: return Key_LeftAlt;

	default: return Key_Unknown;
	}
}

void WindowXCB::pollEvents()
{
	for(;;)
	{
		xcb_generic_event_t* xcbEvent = xcb_poll_for_event(g_xcbConnection);
		if (!xcbEvent) break;

		const u8 eventCode = xcbEvent->response_type & 0x7f;
		switch (eventCode) 
		{
			case XCB_KEY_PRESS:
			{
				const xcb_key_press_event_t* event = (const xcb_key_press_event_t *)xcbEvent;
				RUSH_LOG("press state: %d %d", event->state, event->time);
				Key key = translateKeyXCB(event->detail);
				m_keyboard.keys[key] = true;
				broadcast(WindowEvent::KeyDown(key));
			} break;
			case XCB_KEY_RELEASE:
			{
				const xcb_key_release_event_t* event = (const xcb_key_release_event_t *)xcbEvent;
				RUSH_LOG("release state: %d %d", event->state, event->time);
				Key key = translateKeyXCB(event->detail);
				m_keyboard.keys[key] = false;
				broadcast(WindowEvent::KeyUp(key));
			} break;
			default:
				break;
		}

		free(xcbEvent);
	}
}

}

#else // RUSH_PLATFORM_LINUX

char WindowXCB_cpp_dummy; // suppress linker warning

#endif // RUSH_PLATFORM_LINUX
