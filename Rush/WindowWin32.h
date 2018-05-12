#pragma once

#ifdef RUSH_PLATFORM_WINDOWS

#include "Window.h"

#include <string>
#include <windows.h>

namespace Rush
{

class WindowWin32 : public Window
{

public:
	WindowWin32(const WindowDesc& desc);
	virtual ~WindowWin32();

	virtual void* nativeHandle();
	virtual void  setCaption(const char* str);
	virtual void  setSize(const Tuple2i& size);
	virtual bool  setFullscreen(bool state);

private:
	bool processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	void processMouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
	void processKeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
	void processSizeEvent(WPARAM wparam, LPARAM lparam);

	static LRESULT APIENTRY windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	void finishResizing();

	HWND m_hwnd;

	std::string m_caption;

	Tuple2i m_pendingSize;

	bool m_maximized = false;
	bool m_minimized = false;
	bool m_resizing = false;
	bool m_fullscreen = false;
	Tuple2i m_windowedSize;
	Tuple2i m_windowedPos;

	u32 m_windowStyle = 0;
};
}

#endif // RUSH_PLATFORM_WINDOWS
