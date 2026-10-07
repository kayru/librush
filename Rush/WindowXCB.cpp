#include "Rush.h"

#ifdef RUSH_PLATFORM_LINUX

#include "Platform.h"
#include "WindowXCB.h"
#include "UtilLog.h"

#include <xcb/xcb_keysyms.h>
// The generated XKB header is C: a struct field is named `explicit`
#define explicit explicit_
#include <xcb/xkb.h>
#undef explicit
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <X11/keysym.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <stdlib.h>

namespace Rush
{

namespace
{
	xcb_connection_t* g_xcbConnection = nullptr;
	xcb_screen_t* g_xcbScreen = nullptr;
	u32 g_xcbConnectionRefCount = 0;
	u32 g_xcbKeyMap[256];

	struct Atoms
	{
		xcb_atom_t wmProtocols;
		xcb_atom_t wmDeleteWindow;
		xcb_atom_t netSupported;
		xcb_atom_t netWmName;
		xcb_atom_t netWmState;
		xcb_atom_t netWmStateFullscreen;
		xcb_atom_t utf8String;
		xcb_atom_t clipboard;
		xcb_atom_t clipboardManager;
		xcb_atom_t targets;
		xcb_atom_t multiple;
		xcb_atom_t saveTargets;
		xcb_atom_t text;
		xcb_atom_t incr;
		xcb_atom_t atomPair;
		xcb_atom_t selectionProperty;
		xcb_atom_t timestampProperty;
	};
	Atoms g_atoms;

	// Keyboard layout and modifier state for text input (xkbcommon, tracking the server's keymap)
	xkb_context* g_xkbContext = nullptr;
	xkb_keymap* g_xkbKeymap = nullptr;
	xkb_state* g_xkbState = nullptr;
	xkb_compose_table* g_composeTable = nullptr;
	xkb_compose_state* g_composeState = nullptr;
	s32 g_xkbDeviceId = -1;
	u8 g_xkbEventBase = 0;

	// Bounds the wait for another client that may never answer a selection request
	constexpr auto kSelectionTimeout = std::chrono::seconds(2);

	void internAtoms()
	{
		const struct
		{
			xcb_atom_t* atom;
			const char* name;
		} atoms[] = {
			{&g_atoms.wmProtocols, "WM_PROTOCOLS"},
			{&g_atoms.wmDeleteWindow, "WM_DELETE_WINDOW"},
			{&g_atoms.netSupported, "_NET_SUPPORTED"},
			{&g_atoms.netWmName, "_NET_WM_NAME"},
			{&g_atoms.netWmState, "_NET_WM_STATE"},
			{&g_atoms.netWmStateFullscreen, "_NET_WM_STATE_FULLSCREEN"},
			{&g_atoms.utf8String, "UTF8_STRING"},
			{&g_atoms.clipboard, "CLIPBOARD"},
			{&g_atoms.clipboardManager, "CLIPBOARD_MANAGER"},
			{&g_atoms.targets, "TARGETS"},
			{&g_atoms.multiple, "MULTIPLE"},
			{&g_atoms.saveTargets, "SAVE_TARGETS"},
			{&g_atoms.text, "TEXT"},
			{&g_atoms.incr, "INCR"},
			{&g_atoms.atomPair, "ATOM_PAIR"},
			{&g_atoms.selectionProperty, "RUSH_SELECTION"},
			{&g_atoms.timestampProperty, "RUSH_TIMESTAMP"},
		};
		xcb_intern_atom_cookie_t cookies[std::size(atoms)];
		for (size_t i = 0; i < std::size(atoms); ++i)
		{
			cookies[i] = xcb_intern_atom(g_xcbConnection, 0, u16(strlen(atoms[i].name)), atoms[i].name);
		}
		for (size_t i = 0; i < std::size(atoms); ++i)
		{
			xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(g_xcbConnection, cookies[i], nullptr);
			*atoms[i].atom = reply ? reply->atom : XCB_ATOM_NONE;
			free(reply);
		}
	}

	bool updateKeymap()
	{
		xkb_keymap* keymap = xkb_x11_keymap_new_from_device(g_xkbContext, g_xcbConnection, g_xkbDeviceId, XKB_KEYMAP_COMPILE_NO_FLAGS);
		if (!keymap)
		{
			return false;
		}
		xkb_state* state = xkb_x11_state_new_from_device(keymap, g_xcbConnection, g_xkbDeviceId);
		if (!state)
		{
			xkb_keymap_unref(keymap);
			return false;
		}
		xkb_state_unref(g_xkbState);
		xkb_keymap_unref(g_xkbKeymap);
		g_xkbKeymap = keymap;
		g_xkbState = state;
		return true;
	}

	void releaseKeyboard()
	{
		xkb_compose_state_unref(g_composeState);
		xkb_compose_table_unref(g_composeTable);
		xkb_state_unref(g_xkbState);
		xkb_keymap_unref(g_xkbKeymap);
		xkb_context_unref(g_xkbContext);
		g_composeState = nullptr;
		g_composeTable = nullptr;
		g_xkbState = nullptr;
		g_xkbKeymap = nullptr;
		g_xkbContext = nullptr;
	}

	void setupKeyboard()
	{
		u8 eventBase = 0;
		if (!xkb_x11_setup_xkb_extension(g_xcbConnection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
				XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, nullptr, nullptr, &eventBase, nullptr))
		{
			RUSH_LOG_WARNING("XKB extension unavailable: no text input");
			return;
		}
		g_xkbDeviceId = xkb_x11_get_core_keyboard_device_id(g_xcbConnection);
		g_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		if (g_xkbDeviceId == -1 || !g_xkbContext || !updateKeymap())
		{
			RUSH_LOG_WARNING("XKB keymap unavailable: no text input");
			releaseKeyboard();
			return;
		}

		// Keymap and modifier state changes, as in xkbcommon's X11 guide
		const u16 events = XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_STATE_NOTIFY;
		const u16 mapParts = XCB_XKB_MAP_PART_KEY_TYPES | XCB_XKB_MAP_PART_KEY_SYMS | XCB_XKB_MAP_PART_MODIFIER_MAP |
			XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS | XCB_XKB_MAP_PART_KEY_ACTIONS | XCB_XKB_MAP_PART_VIRTUAL_MODS |
			XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;
		const u16 stateParts = XCB_XKB_STATE_PART_MODIFIER_BASE | XCB_XKB_STATE_PART_MODIFIER_LATCH |
			XCB_XKB_STATE_PART_MODIFIER_LOCK | XCB_XKB_STATE_PART_GROUP_BASE | XCB_XKB_STATE_PART_GROUP_LATCH |
			XCB_XKB_STATE_PART_GROUP_LOCK;
		xcb_xkb_select_events_details_t details = {};
		details.affectNewKeyboard = XCB_XKB_NKN_DETAIL_KEYCODES;
		details.newKeyboardDetails = XCB_XKB_NKN_DETAIL_KEYCODES;
		details.affectState = stateParts;
		details.stateDetails = stateParts;
		xcb_xkb_select_events_aux(g_xcbConnection, xcb_xkb_device_spec_t(g_xkbDeviceId), events, 0, 0, mapParts, mapParts, &details);
		g_xkbEventBase = eventBase;

		// Dead keys and compose sequences of the user's locale
		const char* locale = nullptr;
		for (const char* name : {"LC_ALL", "LC_CTYPE", "LANG"})
		{
			locale = getenv(name);
			if (locale && locale[0])
			{
				break;
			}
		}
		g_composeTable = xkb_compose_table_new_from_locale(g_xkbContext, locale && locale[0] ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
		if (g_composeTable)
		{
			g_composeState = xkb_compose_state_new(g_composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
		}
	}

	void processXkbEvent(const xcb_generic_event_t* event)
	{
		struct Any
		{
			u8 response_type;
			u8 xkbType;
			u16 sequence;
			xcb_timestamp_t time;
			u8 deviceID;
		};
		const Any* any = reinterpret_cast<const Any*>(event);
		if (any->deviceID != g_xkbDeviceId)
		{
			return;
		}
		switch (any->xkbType)
		{
		case XCB_XKB_NEW_KEYBOARD_NOTIFY:
			if (reinterpret_cast<const xcb_xkb_new_keyboard_notify_event_t*>(event)->changed & XCB_XKB_NKN_DETAIL_KEYCODES)
			{
				updateKeymap();
			}
			break;
		case XCB_XKB_MAP_NOTIFY:
			updateKeymap();
			break;
		case XCB_XKB_STATE_NOTIFY:
		{
			const xcb_xkb_state_notify_event_t* state = reinterpret_cast<const xcb_xkb_state_notify_event_t*>(event);
			xkb_state_update_mask(g_xkbState, state->baseMods, state->latchedMods, state->lockedMods,
				xkb_layout_index_t(state->baseGroup), xkb_layout_index_t(state->latchedGroup), xkb_layout_index_t(state->lockedGroup));
			break;
		}
		default:
			break;
		}
	}

	// Calls fn for each code point of well-formed UTF-8 (as xkbcommon produces)
	template <typename Fn>
	void forEachCodePoint(const char* text, size_t length, Fn&& fn)
	{
		size_t i = 0;
		while (i < length)
		{
			const u8 lead = u8(text[i]);
			const u32 count = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
			if (i + count > length)
			{
				return;
			}
			u32 cp = count == 1 ? lead : lead & (0x7F >> count);
			for (u32 j = 1; j < count; ++j)
			{
				cp = (cp << 6) | (u8(text[i + j]) & 0x3F);
			}
			fn(cp);
			i += count;
		}
	}

	bool wmSupports(xcb_atom_t atom)
	{
		const xcb_get_property_cookie_t cookie =
			xcb_get_property(g_xcbConnection, 0, g_xcbScreen->root, g_atoms.netSupported, XCB_ATOM_ATOM, 0, 4096);
		xcb_get_property_reply_t* reply = xcb_get_property_reply(g_xcbConnection, cookie, nullptr);
		bool result = false;
		if (reply && reply->format == 32)
		{
			const xcb_atom_t* atoms = static_cast<const xcb_atom_t*>(xcb_get_property_value(reply));
			const int count = xcb_get_property_value_length(reply) / 4;
			result = std::find(atoms, atoms + count, atom) != atoms + count;
		}
		free(reply);
		return atom != XCB_ATOM_NONE && result;
	}

	// The largest property one request can carry (less the ChangeProperty header): larger text goes by INCR
	size_t maxPropertyBytes()
	{
		return size_t(xcb_get_maximum_request_length(g_xcbConnection)) * 4 - 32;
	}

	bool isLater(xcb_timestamp_t a, xcb_timestamp_t b) { return s32(a - b) > 0; }
}

WindowXCB::WindowXCB(const WindowDesc& desc)
: Window(desc)
, m_pendingSize(m_size)
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

		internAtoms();
		setupKeyboard();
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
		//XCB_EVENT_MASK_POINTER_MOTION_HINT |
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
		//XCB_EVENT_MASK_RESIZE_REDIRECT |
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
		XCB_EVENT_MASK_FOCUS_CHANGE |
		XCB_EVENT_MASK_PROPERTY_CHANGE |
		XCB_EVENT_MASK_COLOR_MAP_CHANGE |
		XCB_EVENT_MASK_OWNER_GRAB_BUTTON;

	xcb_create_window(g_xcbConnection, XCB_COPY_FROM_PARENT, m_nativeHandle, g_xcbScreen->root, 0, 0, u16(desc.width), u16(desc.height),
						0, XCB_WINDOW_CLASS_INPUT_OUTPUT, g_xcbScreen->root_visual, valueMask, valueList);

	if (g_atoms.wmProtocols != XCB_ATOM_NONE && g_atoms.wmDeleteWindow != XCB_ATOM_NONE)
	{
		xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, m_nativeHandle, g_atoms.wmProtocols,
			XCB_ATOM_ATOM, 32, 1, &g_atoms.wmDeleteWindow);
	}

	// Before mapping, the window states the window manager applies are set directly (EWMH)
	m_fullScreen = desc.fullScreen && wmSupports(g_atoms.netWmStateFullscreen);
	if (m_fullScreen)
	{
		xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, m_nativeHandle, g_atoms.netWmState,
			XCB_ATOM_ATOM, 32, 1, &g_atoms.netWmStateFullscreen);
	}

	xcb_map_window(g_xcbConnection, m_nativeHandle);

	//const uint32_t coords[] = {100, 100}; // TODO: place in the center of the screen
	//xcb_configure_window(g_xcbConnection, m_nativeHandle, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, coords);

	setCaption(desc.caption);
}

WindowXCB::~WindowXCB()
{
	saveClipboardToManager();
	for (xcb_generic_event_t* event : m_deferredEvents)
	{
		free(event);
	}

	if (m_pointerGrabbed)
	{
		xcb_ungrab_pointer(g_xcbConnection, XCB_CURRENT_TIME);
	}
	if (m_hiddenCursor)
	{
		xcb_free_cursor(g_xcbConnection, m_hiddenCursor);
	}
	xcb_destroy_window(g_xcbConnection, m_nativeHandle);

	RUSH_ASSERT(g_xcbConnectionRefCount != 0);
	if (--g_xcbConnectionRefCount == 0)
	{
		releaseKeyboard();
		xcb_disconnect(g_xcbConnection);
		g_xcbConnection = nullptr;
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

	const u32 length = u32(strlen(m_caption.c_str()));
	xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, m_nativeHandle, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
		8, length, m_caption.c_str());
	if (g_atoms.netWmName != XCB_ATOM_NONE && g_atoms.utf8String != XCB_ATOM_NONE)
	{
		xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, m_nativeHandle, g_atoms.netWmName, g_atoms.utf8String,
			8, length, m_caption.c_str());
	}
	xcb_flush(g_xcbConnection);
}

// The window manager may refuse or adjust the size (tiling): m_size follows ConfigureNotify
void WindowXCB::setSize(const Tuple2i& size)
{
	const u32 values[] = {u32(std::max(size.x, 1)), u32(std::max(size.y, 1))};
	xcb_configure_window(g_xcbConnection, m_nativeHandle, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
	xcb_flush(g_xcbConnection);
}

// A request to the window manager (EWMH): false when it has no fullscreen state
bool WindowXCB::setFullscreen(bool wantFullScreen)
{
	if (!wmSupports(g_atoms.netWmStateFullscreen))
	{
		return false;
	}

	xcb_client_message_event_t event = {};
	event.response_type = XCB_CLIENT_MESSAGE;
	event.format = 32;
	event.window = m_nativeHandle;
	event.type = g_atoms.netWmState;
	event.data.data32[0] = wantFullScreen ? 1 : 0; // _NET_WM_STATE_ADD or _NET_WM_STATE_REMOVE
	event.data.data32[1] = g_atoms.netWmStateFullscreen;
	event.data.data32[3] = 1; // source: application
	xcb_send_event(g_xcbConnection, 0, g_xcbScreen->root,
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, reinterpret_cast<const char*>(&event));
	xcb_flush(g_xcbConnection);

	m_fullScreen = wantFullScreen;
	return true;
}

// The window manager's state, which also changes on its own (keyboard shortcuts)
void WindowXCB::updateFullscreenState()
{
	const xcb_get_property_cookie_t cookie =
		xcb_get_property(g_xcbConnection, 0, m_nativeHandle, g_atoms.netWmState, XCB_ATOM_ATOM, 0, 1024);
	xcb_get_property_reply_t* reply = xcb_get_property_reply(g_xcbConnection, cookie, nullptr);
	if (reply && reply->format == 32)
	{
		const xcb_atom_t* atoms = static_cast<const xcb_atom_t*>(xcb_get_property_value(reply));
		const int count = xcb_get_property_value_length(reply) / 4;
		m_fullScreen = std::find(atoms, atoms + count, g_atoms.netWmStateFullscreen) != atoms + count;
	}
	else if (reply)
	{
		m_fullScreen = false;
	}
	free(reply);
}

Vec2 WindowXCB::lockCenter() const
{
	return Vec2(float(m_size.x / 2), float(m_size.y / 2));
}

void WindowXCB::warpPointer(Vec2 pos)
{
	const xcb_void_cookie_t cookie = xcb_warp_pointer(
		g_xcbConnection, XCB_NONE, m_nativeHandle, 0, 0, 0, 0, s16(pos.x), s16(pos.y));
	m_warpSequence = u16(cookie.sequence);
	m_warpPending = true;
}

void WindowXCB::grabPointer()
{
	const u16 eventMask = XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;
	const xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer(g_xcbConnection, 1, m_nativeHandle, eventMask,
		XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, m_nativeHandle, m_hiddenCursor, XCB_CURRENT_TIME);
	xcb_grab_pointer_reply_t* reply = xcb_grab_pointer_reply(g_xcbConnection, cookie, nullptr);
	// Not fatal: the hidden window cursor and center warps still work while the
	// pointer is over the window; the grab is retried on the next focus-in
	m_pointerGrabbed = reply && reply->status == XCB_GRAB_STATUS_SUCCESS;
	free(reply);
}

void WindowXCB::setMouseLock(bool state)
{
	if (m_mouseLocked == state)
	{
		return;
	}
	m_mouseLocked = state;

	if (state)
	{
		if (!m_hiddenCursor)
		{
			// Blank 1x1 cursor: zero mask bit makes it fully transparent
			const xcb_pixmap_t pixmap = xcb_generate_id(g_xcbConnection);
			xcb_create_pixmap(g_xcbConnection, 1, pixmap, m_nativeHandle, 1, 1);
			const xcb_gcontext_t gc = xcb_generate_id(g_xcbConnection);
			const u32 foreground = 0;
			xcb_create_gc(g_xcbConnection, gc, pixmap, XCB_GC_FOREGROUND, &foreground);
			const xcb_rectangle_t rect = {0, 0, 1, 1};
			xcb_poly_fill_rectangle(g_xcbConnection, pixmap, gc, 1, &rect);
			xcb_free_gc(g_xcbConnection, gc);
			m_hiddenCursor = xcb_generate_id(g_xcbConnection);
			xcb_create_cursor(g_xcbConnection, m_hiddenCursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0);
			xcb_free_pixmap(g_xcbConnection, pixmap);
		}

		m_preLockMousePos = m_mouse.pos;
		m_lockPointerPos = m_mouse.pos;
		xcb_change_window_attributes(g_xcbConnection, m_nativeHandle, XCB_CW_CURSOR, &m_hiddenCursor);
		grabPointer();
		warpPointer(lockCenter());
	}
	else
	{
		if (m_pointerGrabbed)
		{
			xcb_ungrab_pointer(g_xcbConnection, XCB_CURRENT_TIME);
			m_pointerGrabbed = false;
		}
		const u32 defaultCursor = XCB_CURSOR_NONE;
		xcb_change_window_attributes(g_xcbConnection, m_nativeHandle, XCB_CW_CURSOR, &defaultCursor);
		warpPointer(m_preLockMousePos);
		m_warpPending = false; // unlocked motion is absolute
		m_mouse.pos = m_preLockMousePos;
	}
	xcb_flush(g_xcbConnection);
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

void WindowXCB::processKeyPress(const xcb_key_press_event_t* event)
{
	const Key key = translateKeyXCB(event->detail);
	m_keyboard.keys[key] = true;
	broadcast(WindowEvent::KeyDown(key));

	if (!g_xkbState)
	{
		return;
	}
	if (g_composeState &&
		xkb_compose_state_feed(g_composeState, xkb_state_key_get_one_sym(g_xkbState, event->detail)) == XKB_COMPOSE_FEED_ACCEPTED)
	{
		switch (xkb_compose_state_get_status(g_composeState))
		{
		case XKB_COMPOSE_COMPOSING:
			return;
		case XKB_COMPOSE_CANCELLED:
			xkb_compose_state_reset(g_composeState);
			return;
		case XKB_COMPOSE_COMPOSED:
		{
			char text[64];
			const int length = xkb_compose_state_get_utf8(g_composeState, text, sizeof(text));
			if (length > 0 && size_t(length) < sizeof(text))
			{
				forEachCodePoint(text, size_t(length), [&](u32 cp) { broadcast(WindowEvent::Char(cp)); });
			}
			else if (const u32 cp = xkb_keysym_to_utf32(xkb_compose_state_get_one_sym(g_composeState)))
			{
				broadcast(WindowEvent::Char(cp));
			}
			xkb_compose_state_reset(g_composeState);
			return;
		}
		case XKB_COMPOSE_NOTHING:
			break;
		}
	}
	if (const u32 cp = xkb_state_key_get_utf32(g_xkbState, event->detail))
	{
		broadcast(WindowEvent::Char(cp));
	}
}

void WindowXCB::pollEvents()
{
	// Events that came while waiting for a selection reply, kept in order
	DynamicArray<xcb_generic_event_t*> deferred = std::move(m_deferredEvents);
	m_deferredEvents.clear();
	for (xcb_generic_event_t* event : deferred)
	{
		processEvent(event);
		free(event);
	}

	while (xcb_generic_event_t* event = xcb_poll_for_event(g_xcbConnection))
	{
		processEvent(event);
		free(event);
	}

	// One warp per batch: re-center once the previous warp has taken effect
	if (m_mouseLocked && !m_warpPending)
	{
		const Vec2 center = lockCenter();
		if (m_lockPointerPos.x != center.x || m_lockPointerPos.y != center.y)
		{
			warpPointer(center);
			xcb_flush(g_xcbConnection);
		}
	}
}

void WindowXCB::processEvent(const xcb_generic_event_t* xcbEvent)
{
	const u32 mouseButtonRemap[4] = {0, 0, 2, 1};

	const u8 eventCode = xcbEvent->response_type & 0x7f;
	if (g_xkbState && eventCode == g_xkbEventBase)
	{
		processXkbEvent(xcbEvent);
		return;
	}
	if (processSelectionEvent(xcbEvent))
	{
		return;
	}

	switch (eventCode)
	{
	case XCB_KEY_PRESS:
	case XCB_KEY_RELEASE:
	case XCB_BUTTON_PRESS:
	case XCB_BUTTON_RELEASE:
	case XCB_MOTION_NOTIFY:
	case XCB_ENTER_NOTIFY:
	case XCB_LEAVE_NOTIFY:
		// Same layout for these: the time a selection request or ownership change refers to
		m_lastEventTime = reinterpret_cast<const xcb_key_press_event_t*>(xcbEvent)->time;
		break;
	case XCB_PROPERTY_NOTIFY:
		m_lastEventTime = reinterpret_cast<const xcb_property_notify_event_t*>(xcbEvent)->time;
		break;
	default:
		break;
	}

	const bool isInputEvent = eventCode == XCB_MOTION_NOTIFY || eventCode == XCB_BUTTON_PRESS ||
		eventCode == XCB_BUTTON_RELEASE || eventCode == XCB_KEY_PRESS || eventCode == XCB_KEY_RELEASE;
	if (isInputEvent && !m_osInputEnabled)
	{
		return;
	}
	switch (eventCode)
	{
		case XCB_EXPOSE:
		{
			break;
		}
		case XCB_CLIENT_MESSAGE:
		{
			const xcb_client_message_event_t* event = (const xcb_client_message_event_t*)xcbEvent;
			if (event->type == g_atoms.wmProtocols && event->data.data32[0] == g_atoms.wmDeleteWindow)
			{
				RUSH_LOG("Close requested: WM_DELETE_WINDOW from the window manager");
				requestClose();
			}
			break;
		}
		case XCB_PROPERTY_NOTIFY:
		{
			const xcb_property_notify_event_t* event = (const xcb_property_notify_event_t*)xcbEvent;
			if (event->window == m_nativeHandle && event->atom == g_atoms.netWmState)
			{
				updateFullscreenState();
			}
			break;
		}
		case XCB_CONFIGURE_NOTIFY:
		{
			const xcb_configure_notify_event_t* event = (const xcb_configure_notify_event_t *)xcbEvent;
			if (event->window == m_nativeHandle && event->width > 0 && event->height > 0)
			{
				m_pendingSize.x = event->width;
				m_pendingSize.y = event->height;
				if (m_size != m_pendingSize)
				{
					m_size = m_pendingSize;
					broadcast(WindowEvent::Resize(m_size.x, m_size.y));
				}
			}
			break;
		}
		case XCB_FOCUS_IN:
		case XCB_FOCUS_OUT:
		{
			const xcb_focus_in_event_t* event = (const xcb_focus_in_event_t*)xcbEvent;
			if (event->mode == XCB_NOTIFY_MODE_GRAB || event->mode == XCB_NOTIFY_MODE_UNGRAB)
			{
				break;
			}
			m_focused = eventCode == XCB_FOCUS_IN;
			if (g_composeState)
			{
				xkb_compose_state_reset(g_composeState);
			}
			if (m_focused && m_mouseLocked && !m_pointerGrabbed)
			{
				grabPointer();
			}
			break;
		}
		case XCB_MOTION_NOTIFY:
		{
			const xcb_motion_notify_event_t* event = (const xcb_motion_notify_event_t *)xcbEvent;
			const Vec2 pointerPos = Vec2(event->event_x, event->event_y);
			if (m_mouseLocked)
			{
				// Motion generated after the server processed the last warp is
				// relative to the warp target, earlier motion to the previous position
				if (m_warpPending && u16(event->sequence - m_warpSequence) < 0x8000)
				{
					m_warpPending = false;
					m_lockPointerPos = lockCenter();
				}
				const Vec2 delta = pointerPos - m_lockPointerPos;
				m_lockPointerPos = pointerPos;
				if (delta.x == 0.0f && delta.y == 0.0f)
				{
					break;
				}
				m_mouse.pos += delta;
			}
			else
			{
				m_mouse.pos = pointerPos;
			}
			broadcast(WindowEvent::MouseMove(Vec2(m_mouse.pos)));
			break;
		}
		case XCB_BUTTON_PRESS:
		{
			const xcb_button_press_event_t* event = (const xcb_button_press_event_t *)xcbEvent;
			if (event->detail > 0 && event->detail < 4)
			{
				u32 idx = mouseButtonRemap[event->detail];
				m_mouse.buttons[idx] = true;
				if (!m_mouseLocked)
				{
					m_mouse.pos.x = event->event_x;
					m_mouse.pos.y = event->event_y;
				}
				broadcast(WindowEvent::MouseDown(m_mouse.pos, idx, false));
			}
			else if(event->detail == 4) broadcast(WindowEvent::Scroll(0.0, 1.0));
			else if(event->detail == 5) broadcast(WindowEvent::Scroll(0.0, -1.0));
			else if(event->detail == 6) broadcast(WindowEvent::Scroll(1.0, 0.0));
			else if(event->detail == 7) broadcast(WindowEvent::Scroll(-1.0, 0.0));

			break;
		}
		case XCB_BUTTON_RELEASE:
		{
			const xcb_button_release_event_t* event = (const xcb_button_release_event_t *)xcbEvent;
			if (event->detail > 0 && event->detail <= 3)
			{
				u32 idx = mouseButtonRemap[event->detail];
				m_mouse.buttons[idx] = false;
				if (!m_mouseLocked)
				{
					m_mouse.pos.x = event->event_x;
					m_mouse.pos.y = event->event_y;
				}
				broadcast(WindowEvent::MouseUp(m_mouse.pos, idx));
			}
			break;
		}
		case XCB_KEY_PRESS:
		{
			processKeyPress((const xcb_key_press_event_t*)xcbEvent);
			break;
		}
		case XCB_KEY_RELEASE:
		{
			const xcb_key_release_event_t* event = (const xcb_key_release_event_t *)xcbEvent;
			Key key = translateKeyXCB(event->detail);
			m_keyboard.keys[key] = false;
			broadcast(WindowEvent::KeyUp(key));
			break;
		}
		default:
			break;
	}
}

// Clipboard: this window owns CLIPBOARD while it holds copied text and serves it to
// other clients from the event loop; pasting asks the owner and waits for its answer (ICCCM 2)

template <typename Accept>
xcb_generic_event_t* WindowXCB::waitForEvent(Accept&& accept)
{
	xcb_flush(g_xcbConnection);
	const auto deadline = std::chrono::steady_clock::now() + kSelectionTimeout;
	for (;;)
	{
		while (xcb_generic_event_t* event = xcb_poll_for_event(g_xcbConnection))
		{
			if (accept(event))
			{
				return event;
			}
			if (processSelectionEvent(event))
			{
				free(event);
			}
			else
			{
				m_deferredEvents.push_back(event);
			}
		}
		if (xcb_connection_has_error(g_xcbConnection))
		{
			return nullptr;
		}
		const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if (remaining.count() <= 0)
		{
			return nullptr;
		}
		pollfd fd = {xcb_get_file_descriptor(g_xcbConnection), POLLIN, 0};
		poll(&fd, 1, int(remaining.count()) + 1);
	}
}

// The time of the last event, else the server's current time: ICCCM asks for a real
// timestamp, which a zero-length property append yields in its PropertyNotify
xcb_timestamp_t WindowXCB::eventTime()
{
	if (m_lastEventTime != XCB_CURRENT_TIME)
	{
		return m_lastEventTime;
	}
	xcb_change_property(g_xcbConnection, XCB_PROP_MODE_APPEND, m_nativeHandle, g_atoms.timestampProperty,
		XCB_ATOM_INTEGER, 32, 0, nullptr);
	xcb_generic_event_t* event = waitForEvent([this](const xcb_generic_event_t* e) {
		const xcb_property_notify_event_t* notify = reinterpret_cast<const xcb_property_notify_event_t*>(e);
		return (e->response_type & 0x7f) == XCB_PROPERTY_NOTIFY && notify->window == m_nativeHandle &&
			notify->atom == g_atoms.timestampProperty;
	});
	if (event)
	{
		m_lastEventTime = reinterpret_cast<const xcb_property_notify_event_t*>(event)->time;
		free(event);
	}
	return m_lastEventTime;
}

void WindowXCB::setClipboardText(const char* text)
{
	m_clipboardText = text;
	m_clipboardTime = eventTime();
	xcb_set_selection_owner(g_xcbConnection, m_nativeHandle, g_atoms.clipboard, m_clipboardTime);
	const xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(g_xcbConnection, g_atoms.clipboard);
	xcb_get_selection_owner_reply_t* reply = xcb_get_selection_owner_reply(g_xcbConnection, cookie, nullptr);
	m_ownsClipboard = reply && reply->owner == m_nativeHandle;
	free(reply);
	if (!m_ownsClipboard)
	{
		RUSH_LOG_WARNING("Clipboard: could not take ownership of CLIPBOARD");
		m_clipboardText = String();
	}
}

String WindowXCB::getClipboardText()
{
	// Asked of the server: a copy elsewhere may come before its SelectionClear is processed
	if (m_ownsClipboard)
	{
		const xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(g_xcbConnection, g_atoms.clipboard);
		xcb_get_selection_owner_reply_t* reply = xcb_get_selection_owner_reply(g_xcbConnection, cookie, nullptr);
		m_ownsClipboard = reply && reply->owner == m_nativeHandle;
		free(reply);
		if (m_ownsClipboard)
		{
			return m_clipboardText;
		}
		m_clipboardText = String();
	}
	DynamicArray<char> data;
	if (!readSelection(g_atoms.utf8String, data))
	{
		// Latin-1 from clients without UTF-8
		DynamicArray<char> latin1;
		if (!readSelection(XCB_ATOM_STRING, latin1))
		{
			return String();
		}
		data.clear();
		for (const char c : latin1)
		{
			const u8 byte = u8(c);
			if (byte < 0x80)
			{
				data.push_back(c);
			}
			else
			{
				data.push_back(char(0xC0 | (byte >> 6)));
				data.push_back(char(0x80 | (byte & 0x3F)));
			}
		}
	}
	return data.empty() ? String() : String(data.data(), data.size());
}

// Reads and deletes the selection property, appending its bytes
bool WindowXCB::takeProperty(DynamicArray<char>& out, xcb_atom_t& type)
{
	const xcb_get_property_cookie_t cookie = xcb_get_property(g_xcbConnection, 1, m_nativeHandle,
		g_atoms.selectionProperty, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX / 4);
	xcb_get_property_reply_t* reply = xcb_get_property_reply(g_xcbConnection, cookie, nullptr);
	if (!reply)
	{
		return false;
	}
	type = reply->type;
	const size_t length = size_t(xcb_get_property_value_length(reply));
	const size_t offset = out.size();
	out.resize(offset + length);
	if (length)
	{
		memcpy(out.data() + offset, xcb_get_property_value(reply), length);
	}
	free(reply);
	return true;
}

bool WindowXCB::readSelection(xcb_atom_t target, DynamicArray<char>& out)
{
	out.clear();
	if (target == XCB_ATOM_NONE)
	{
		return false;
	}
	const xcb_timestamp_t time = eventTime();
	xcb_delete_property(g_xcbConnection, m_nativeHandle, g_atoms.selectionProperty);
	xcb_convert_selection(g_xcbConnection, m_nativeHandle, g_atoms.clipboard, target, g_atoms.selectionProperty, time);
	xcb_generic_event_t* notify = waitForEvent([this](const xcb_generic_event_t* e) {
		const xcb_selection_notify_event_t* n = reinterpret_cast<const xcb_selection_notify_event_t*>(e);
		return (e->response_type & 0x7f) == XCB_SELECTION_NOTIFY && n->requestor == m_nativeHandle &&
			n->selection == g_atoms.clipboard;
	});
	if (!notify)
	{
		RUSH_LOG_WARNING("Clipboard: no answer from the owner");
		return false;
	}
	const xcb_atom_t property = reinterpret_cast<const xcb_selection_notify_event_t*>(notify)->property;
	free(notify);
	xcb_atom_t type = XCB_ATOM_NONE;
	if (property == XCB_ATOM_NONE || !takeProperty(out, type))
	{
		return false;
	}
	if (type != g_atoms.incr)
	{
		return true;
	}

	// INCR: deleting the property asked for the first chunk; an empty one ends the transfer
	out.clear();
	for (;;)
	{
		xcb_generic_event_t* event = waitForEvent([this](const xcb_generic_event_t* e) {
			const xcb_property_notify_event_t* n = reinterpret_cast<const xcb_property_notify_event_t*>(e);
			return (e->response_type & 0x7f) == XCB_PROPERTY_NOTIFY && n->window == m_nativeHandle &&
				n->atom == g_atoms.selectionProperty && n->state == XCB_PROPERTY_NEW_VALUE;
		});
		if (!event)
		{
			RUSH_LOG_WARNING("Clipboard: incremental transfer stalled");
			return false;
		}
		free(event);
		const size_t before = out.size();
		if (!takeProperty(out, type))
		{
			return false;
		}
		if (out.size() == before)
		{
			return true;
		}
	}
}

bool WindowXCB::processSelectionEvent(const xcb_generic_event_t* event)
{
	switch (event->response_type & 0x7f)
	{
	case XCB_SELECTION_REQUEST:
		serveSelectionRequest(reinterpret_cast<const xcb_selection_request_event_t*>(event));
		return true;
	case XCB_SELECTION_CLEAR:
	{
		const xcb_selection_clear_event_t* clear = reinterpret_cast<const xcb_selection_clear_event_t*>(event);
		if (clear->owner != m_nativeHandle || clear->selection != g_atoms.clipboard)
		{
			return false;
		}
		m_ownsClipboard = false;
		m_clipboardText = String();
		return true;
	}
	case XCB_PROPERTY_NOTIFY:
	{
		const xcb_property_notify_event_t* notify = reinterpret_cast<const xcb_property_notify_event_t*>(event);
		if (notify->window == m_nativeHandle)
		{
			return false;
		}
		continueTransfer(notify);
		return true;
	}
	case XCB_DESTROY_NOTIFY:
	{
		const xcb_window_t window = reinterpret_cast<const xcb_destroy_notify_event_t*>(event)->window;
		if (window == m_nativeHandle)
		{
			return false;
		}
		for (size_t i = m_transfers.size(); i-- > 0;)
		{
			if (m_transfers[i].requestor == window)
			{
				if (i + 1 != m_transfers.size())
				{
					m_transfers[i] = std::move(m_transfers.back());
				}
				m_transfers.pop_back();
			}
		}
		return true;
	}
	default:
		return false;
	}
}

void WindowXCB::serveSelectionRequest(const xcb_selection_request_event_t* request)
{
	// Obsolete clients name no property: the target doubles as one
	xcb_atom_t property = request->property == XCB_ATOM_NONE ? request->target : request->property;
	const bool owned = m_ownsClipboard && request->owner == m_nativeHandle && request->selection == g_atoms.clipboard &&
		(request->time == XCB_CURRENT_TIME || m_clipboardTime == XCB_CURRENT_TIME || !isLater(m_clipboardTime, request->time));
	const bool multipleWithoutProperty = request->target == g_atoms.multiple && request->property == XCB_ATOM_NONE;
	if (!owned || multipleWithoutProperty || !convertSelection(request->requestor, request->target, property, false))
	{
		property = XCB_ATOM_NONE;
	}

	xcb_selection_notify_event_t notify = {};
	notify.response_type = XCB_SELECTION_NOTIFY;
	notify.time = request->time;
	notify.requestor = request->requestor;
	notify.selection = request->selection;
	notify.target = request->target;
	notify.property = property;
	xcb_send_event(g_xcbConnection, 0, request->requestor, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char*>(&notify));
	xcb_flush(g_xcbConnection);
}

// Writes the clipboard as target into the requestor's property; false for targets it cannot give
bool WindowXCB::convertSelection(xcb_window_t requestor, xcb_atom_t target, xcb_atom_t property, bool nested)
{
	if (target == XCB_ATOM_NONE)
	{
		return false;
	}
	if (target == g_atoms.targets)
	{
		const xcb_atom_t targets[] = {g_atoms.targets, g_atoms.multiple, g_atoms.utf8String, g_atoms.text};
		xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, requestor, property, XCB_ATOM_ATOM, 32,
			u32(std::size(targets)), targets);
		return true;
	}
	if (target == g_atoms.utf8String || target == g_atoms.text)
	{
		const size_t length = m_clipboardText.length();
		if (length <= maxPropertyBytes())
		{
			xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, requestor, property, g_atoms.utf8String, 8,
				u32(length), m_clipboardText.c_str());
			return true;
		}
		// Too large for one request: announce INCR with the size, then send chunks as the requestor deletes them
		const u32 eventMask = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		xcb_change_window_attributes(g_xcbConnection, requestor, XCB_CW_EVENT_MASK, &eventMask);
		const u32 size = u32(std::min<size_t>(length, UINT32_MAX));
		xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, requestor, property, g_atoms.incr, 32, 1, &size);
		Transfer transfer;
		transfer.requestor = requestor;
		transfer.property = property;
		transfer.data = m_clipboardText;
		m_transfers.push_back(std::move(transfer));
		return true;
	}
	if (target == g_atoms.multiple && !nested)
	{
		// The property lists (target, property) pairs; ones that fail are set to None
		const xcb_get_property_cookie_t cookie =
			xcb_get_property(g_xcbConnection, 0, requestor, property, g_atoms.atomPair, 0, UINT32_MAX / 4);
		xcb_get_property_reply_t* reply = xcb_get_property_reply(g_xcbConnection, cookie, nullptr);
		if (!reply || reply->format != 32 || reply->type != g_atoms.atomPair)
		{
			free(reply);
			return false;
		}
		xcb_atom_t* pairs = static_cast<xcb_atom_t*>(xcb_get_property_value(reply));
		const u32 count = u32(xcb_get_property_value_length(reply) / 4) & ~1u;
		for (u32 i = 0; i < count; i += 2)
		{
			if (pairs[i + 1] == XCB_ATOM_NONE || !convertSelection(requestor, pairs[i], pairs[i + 1], true))
			{
				pairs[i + 1] = XCB_ATOM_NONE;
			}
		}
		xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, requestor, property, g_atoms.atomPair, 32, count, pairs);
		free(reply);
		return true;
	}
	return false;
}

void WindowXCB::continueTransfer(const xcb_property_notify_event_t* event)
{
	if (event->state != XCB_PROPERTY_DELETE)
	{
		return;
	}
	for (size_t i = 0; i < m_transfers.size(); ++i)
	{
		Transfer& transfer = m_transfers[i];
		if (transfer.requestor != event->window || transfer.property != event->atom)
		{
			continue;
		}
		const size_t chunk = std::min(transfer.data.length() - transfer.offset, maxPropertyBytes());
		xcb_change_property(g_xcbConnection, XCB_PROP_MODE_REPLACE, transfer.requestor, transfer.property,
			g_atoms.utf8String, 8, u32(chunk), transfer.data.c_str() + transfer.offset);
		transfer.offset += chunk;
		if (chunk == 0)
		{
			endTransfer(i);
		}
		xcb_flush(g_xcbConnection);
		return;
	}
}

void WindowXCB::endTransfer(size_t index)
{
	const xcb_window_t requestor = m_transfers[index].requestor;
	if (index + 1 != m_transfers.size())
	{
		m_transfers[index] = std::move(m_transfers.back());
	}
	m_transfers.pop_back();
	const bool requestorStillUsed = std::any_of(m_transfers.begin(), m_transfers.end(),
		[requestor](const Transfer& t) { return t.requestor == requestor; });
	if (!requestorStillUsed)
	{
		const u32 eventMask = XCB_EVENT_MASK_NO_EVENT;
		xcb_change_window_attributes(g_xcbConnection, requestor, XCB_CW_EVENT_MASK, &eventMask);
	}
}

// Copied text outlives the app when a clipboard manager takes it over (freedesktop ClipboardManager)
void WindowXCB::saveClipboardToManager()
{
	if (!m_ownsClipboard || g_atoms.clipboardManager == XCB_ATOM_NONE || g_atoms.saveTargets == XCB_ATOM_NONE)
	{
		return;
	}
	const xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(g_xcbConnection, g_atoms.clipboardManager);
	xcb_get_selection_owner_reply_t* reply = xcb_get_selection_owner_reply(g_xcbConnection, cookie, nullptr);
	const bool hasManager = reply && reply->owner != XCB_WINDOW_NONE;
	free(reply);
	if (!hasManager)
	{
		return;
	}
	xcb_convert_selection(g_xcbConnection, m_nativeHandle, g_atoms.clipboardManager, g_atoms.saveTargets,
		g_atoms.selectionProperty, eventTime());
	if (xcb_generic_event_t* event = waitForEvent([this](const xcb_generic_event_t* e) {
		const xcb_selection_notify_event_t* n = reinterpret_cast<const xcb_selection_notify_event_t*>(e);
		return (e->response_type & 0x7f) == XCB_SELECTION_NOTIFY && n->requestor == m_nativeHandle &&
			n->selection == g_atoms.clipboardManager;
	}))
	{
		free(event);
	}
}

}

#else // RUSH_PLATFORM_LINUX

char WindowXCB_cpp_dummy; // suppress linker warning

#endif // RUSH_PLATFORM_LINUX
