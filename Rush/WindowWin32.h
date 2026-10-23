#pragma once

#ifdef RUSH_PLATFORM_WINDOWS

#include "Window.h"
#include "UtilString.h"

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
	void applyMouseLock(bool state) override;

	void processMouseEvent(UINT message, WPARAM wparam, LPARAM lparam);
	void onMouseButton(u32 button, bool down, Vec2 pos);
	bool processKeyEvent(UINT message, WPARAM wparam, LPARAM lparam);
	void processChar(u32 unit);
	void processSizeEvent(WPARAM wparam, LPARAM lparam);

	static LRESULT APIENTRY windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	void finishResizing();

	HWND m_hwnd;

	String m_caption;

	Tuple2i m_pendingSize;

	bool m_maximized = false;
	bool m_minimized = false;
	bool m_resizing = false;
	Tuple2i m_windowedSize;
	Tuple2i m_windowedPos;

	u32 m_windowStyle = 0;

	u32 m_highSurrogate = 0; // the first half of a pair, until its second arrives
};
}

#endif // RUSH_PLATFORM_WINDOWS
