#include "Rush.h"

#ifdef RUSH_PLATFORM_LINUX

#include "Platform.h"
#include "WindowXCB.h"
#include "UtilLog.h"

#include <xcb/xcb.h>

//#include <X11/Xutil.h>
//#include <X11/Xlib.h>

namespace Rush
{

namespace
{
	xcb_connection_t* g_xcbConnection = nullptr;
	xcb_screen_t* g_xcbScreen = nullptr;
	u32 g_xcbConnectionRefCount = 0;
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
	}

	g_xcbConnectionRefCount++;

	m_nativeHandle = xcb_generate_id(g_xcbConnection);

	uint32_t value_mask, value_list[32];
	value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	value_list[0] = g_xcbScreen->black_pixel;
	value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

	xcb_create_window(g_xcbConnection, XCB_COPY_FROM_PARENT, m_nativeHandle, g_xcbScreen->root, 0, 0, u16(desc.width), u16(desc.height),
						0, XCB_WINDOW_CLASS_INPUT_OUTPUT, g_xcbScreen->root_visual, value_mask, value_list);

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
}

#else // RUSH_PLATFORM_LINUX

char WindowXCB_cpp_dummy; // suppress linker warning

#endif // RUSH_PLATFORM_LINUX
