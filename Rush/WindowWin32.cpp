#include "Rush.h"

#ifdef RUSH_PLATFORM_WINDOWS

#include "Platform.h"
#include "WindowWin32.h"
#include "UtilLog.h"

#include <stdio.h>
#include <tchar.h>

#ifndef WM_NCMOUSEHOVER
#define WM_NCMOUSEHOVER 0x02A0
#endif // WM_NCMOUSEHOVER

#ifndef WM_NCMOUSELEAVE
#define WM_NCMOUSELEAVE 0x02A2
#endif // WM_NCMOUSELEAVE

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif // WM_MOUSEHWHEEL

namespace Rush
{
static Key translateKeyWin32(int key)
{
	switch (key)
	{
	case VK_SPACE: return Key_Space;
	case 0xBC: return Key_Comma;
	case 0xBD: return Key_Minus;
	case 0xBE: return Key_Period;
	case 0xBF: return Key_Slash;
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
	case 0xBA: return Key_Semicolon;
	case 0xBB: return Key_Equal;
	case 'A': return Key_A;
	case 'B': return Key_B;
	case 'C': return Key_C;
	case 'D': return Key_D;
	case 'E': return Key_E;
	case 'F': return Key_F;
	case 'G': return Key_G;
	case 'H': return Key_H;
	case 'I': return Key_I;
	case 'J': return Key_J;
	case 'K': return Key_K;
	case 'L': return Key_L;
	case 'M': return Key_M;
	case 'N': return Key_N;
	case 'O': return Key_O;
	case 'P': return Key_P;
	case 'Q': return Key_Q;
	case 'R': return Key_R;
	case 'S': return Key_S;
	case 'T': return Key_T;
	case 'U': return Key_U;
	case 'V': return Key_V;
	case 'W': return Key_W;
	case 'X': return Key_X;
	case 'Y': return Key_Y;
	case 'Z': return Key_Z;
	case 0xDB: return Key_LeftBracket;
	case 0xDC: return Key_Backslash;
	case 0xDD: return Key_RightBracket;
	case VK_ESCAPE: return Key_Escape;
	case VK_RETURN: return Key_Enter;
	case VK_TAB: return Key_Tab;
	case VK_BACK: return Key_Backspace;
	case VK_INSERT: return Key_Insert;
	case VK_DELETE: return Key_Delete;
	case VK_RIGHT: return Key_Right;
	case VK_LEFT: return Key_Left;
	case VK_DOWN: return Key_Down;
	case VK_UP: return Key_Up;
	case VK_PRIOR: return Key_PageUp;
	case VK_NEXT: return Key_PageDown;
	case VK_HOME: return Key_Home;
	case VK_END: return Key_End;
	case VK_CAPITAL: return Key_CapsLock;
	case VK_SCROLL: return Key_ScrollLock;
	case VK_NUMLOCK: return Key_NumLock;
	case VK_PRINT: return Key_PrintScreen;
	case VK_PAUSE: return Key_Pause;
	case VK_F1: return Key_F1;
	case VK_F2: return Key_F2;
	case VK_F3: return Key_F3;
	case VK_F4: return Key_F4;
	case VK_F5: return Key_F5;
	case VK_F6: return Key_F6;
	case VK_F7: return Key_F7;
	case VK_F8: return Key_F8;
	case VK_F9: return Key_F9;
	case VK_F10: return Key_F10;
	case VK_F11: return Key_F11;
	case VK_F12: return Key_F12;
	case VK_F13: return Key_F13;
	case VK_F14: return Key_F14;
	case VK_F15: return Key_F15;
	case VK_F16: return Key_F16;
	case VK_F17: return Key_F17;
	case VK_F18: return Key_F18;
	case VK_F19: return Key_F19;
	case VK_F20: return Key_F20;
	case VK_F21: return Key_F21;
	case VK_F22: return Key_F22;
	case VK_F23: return Key_F23;
	case VK_F24: return Key_F24;
	case VK_SHIFT: return Key_LeftShift;
	case VK_CONTROL: return Key_LeftControl;
	case VK_MENU: return Key_LeftAlt;
	default: return Key_Unknown;
	}
}

WindowWin32::WindowWin32(const WindowDesc& desc)
: Window(desc), m_hwnd(0), m_pendingSize(m_size), m_windowedSize(m_size)
{
	SetProcessDPIAware();

	// register window class

	WNDCLASSEX wc = {0};

	HINSTANCE hInst = GetModuleHandle(nullptr);

	// WNDPROC;

	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = windowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInst;
	wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOWFRAME);
	wc.lpszMenuName  = nullptr;
	wc.lpszClassName = _T("RushWindowWin32");
	wc.hIconSm       = wc.hIcon;

	RegisterClassEx(&wc);

	m_windowStyle = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	if (desc.resizable)
		m_windowStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;

	// find screen center and create our window there

	int posX = GetSystemMetrics(SM_CXSCREEN) / 2 - m_size.x / 2;
	int posY = GetSystemMetrics(SM_CYSCREEN) / 2 - m_size.y / 2;

	// calculate window size for required client area

	RECT clientRect = {posX, posY, posX + m_size.x, posY + m_size.y};
	AdjustWindowRect(&clientRect, m_windowStyle, FALSE);

	m_windowedPos.x = posX;
	m_windowedPos.y = posY;

	// create window

	m_hwnd = CreateWindowA("RushWindowWin32",
	    desc.caption ? desc.caption : "",
	    m_windowStyle,
	    clientRect.left,
	    clientRect.top,
	    clientRect.right - clientRect.left,
	    clientRect.bottom - clientRect.top,
	    nullptr,
	    nullptr,
	    hInst,
	    nullptr);

	// setup window owner for message handling

	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

	if (desc.fullScreen)
	{
		setFullscreen(true);
	}

	ShowWindow(m_hwnd, SW_SHOWNORMAL);
	UpdateWindow(m_hwnd);
}

WindowWin32::~WindowWin32() {}

LRESULT APIENTRY WindowWin32::windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	WindowWin32* window = reinterpret_cast<WindowWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	bool handled = false;
	if (window)
	{
		handled = window->processMessage(hwnd, msg, wparam, lparam);
	}

	if (handled)
	{
		return 0;
	}
	else
	{
		return (LRESULT)DefWindowProc(hwnd, msg, wparam, lparam);
	}
}

void* WindowWin32::nativeHandle() { return (void*)&m_hwnd; }

void WindowWin32::setCaption(const char* str)
{
	if (str == nullptr)
	{
		str = "";
	}

	m_caption = str;
	SetWindowTextA(m_hwnd, str);
}

bool WindowWin32::processMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_NCMOUSELEAVE:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL: processMouseEvent(msg, wparam, lparam); return true;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_SYSCHAR: processKeyEvent(msg, wparam, lparam); return true;

	case WM_SIZE: processSizeEvent(wparam, lparam); return true;

	case WM_ENTERSIZEMOVE: m_resizing = true; break;

	case WM_EXITSIZEMOVE:
		m_resizing = false;
		finishResizing();
		break;

	case WM_CLOSE: close(); return true;
	}

	return false;
}

void WindowWin32::processMouseEvent(UINT message, WPARAM wparam, LPARAM lparam)
{
	if (message != WM_NCMOUSELEAVE)
	{
		int xPos    = LOWORD(lparam);
		int yPos    = HIWORD(lparam);
		m_mouse.pos = Vec2((float)xPos, (float)yPos);
		;
	}
	else
	{
		m_mouse.buttons[0] = false;
		m_mouse.buttons[1] = false;
		m_mouse.buttons[2] = false;
	}

	switch (message)
	{
	case WM_MOUSEMOVE: broadcast(WindowEvent::MouseMove(m_mouse.pos)); break;

	case WM_LBUTTONDOWN:
		m_mouse.buttons[0] = true;
		broadcast(WindowEvent::MouseDown(m_mouse.pos, 0, false));
		break;

	case WM_LBUTTONUP:
		m_mouse.buttons[0] = false;
		broadcast(WindowEvent::MouseUp(m_mouse.pos, 0));
		break;

	case WM_LBUTTONDBLCLK:
		m_mouse.buttons[0] = true;
		broadcast(WindowEvent::MouseDown(m_mouse.pos, 0, true));
		break;

	case WM_RBUTTONDOWN:
		m_mouse.buttons[1] = true;
		broadcast(WindowEvent::MouseDown(m_mouse.pos, 1, false));
		break;

	case WM_RBUTTONUP:
		m_mouse.buttons[1] = false;
		broadcast(WindowEvent::MouseUp(m_mouse.pos, 1));
		break;

	case WM_RBUTTONDBLCLK:
		m_mouse.buttons[1] = true;
		broadcast(WindowEvent::MouseDown(m_mouse.pos, 1, true));
		break;

	case WM_MBUTTONDOWN:
		m_mouse.buttons[2] = true;
		broadcast(WindowEvent::MouseDown(m_mouse.pos, 2, false));
		break;

	case WM_MBUTTONUP:
		m_mouse.buttons[2] = false;
		broadcast(WindowEvent::MouseUp(m_mouse.pos, 2));
		break;

	case WM_MBUTTONDBLCLK:
		m_mouse.buttons[2] = true;
		broadcast(WindowEvent::MouseDown(m_mouse.pos, 2, true));
		break;

	case WM_MOUSEHWHEEL:
		m_mouse.wheelH += (int)GET_WHEEL_DELTA_WPARAM(wparam);
		broadcast(WindowEvent::Scroll((float)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA, 0.0f));
		break;

	case WM_MOUSEWHEEL:
		m_mouse.wheelV += (int)GET_WHEEL_DELTA_WPARAM(wparam);
		broadcast(WindowEvent::Scroll(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA));
		break;
	}
}

void WindowWin32::processKeyEvent(UINT message, WPARAM wparam, LPARAM lparam)
{
	RUSH_UNUSED(wparam);
	RUSH_UNUSED(lparam);

	Key key = translateKeyWin32((int)wparam);

	switch (message)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		m_keyboard.keys[key] = true;
		broadcast(WindowEvent::KeyDown(key));
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		m_keyboard.keys[key] = false;
		broadcast(WindowEvent::KeyUp(key));
		break;

	case WM_SYSCHAR:
	case WM_CHAR: broadcast(WindowEvent::Char((u32)wparam)); break;
	}
}

void WindowWin32::finishResizing()
{
	if (m_size != m_pendingSize)
	{
		m_size = m_pendingSize;
		broadcast(WindowEvent::Resize(m_size.x, m_size.y));
	}
}

void WindowWin32::processSizeEvent(WPARAM wparam, LPARAM lparam)
{
	m_pendingSize.x = LOWORD(lparam);
	m_pendingSize.y = HIWORD(lparam);

	if (wparam == SIZE_MINIMIZED)
	{
		m_maximized = false;
		m_minimized = true;
	}
	if (wparam == SIZE_MAXIMIZED)
	{
		m_maximized = true;
		m_minimized = false;
		finishResizing();
	}
	if (wparam == SIZE_RESTORED)
	{
		m_minimized = false;
		m_maximized = false;

		if (m_resizing == false)
		{
			finishResizing();
		}
	}
}

void WindowWin32::setSize(const Tuple2i& size)
{
	RECT rect = {0, 0, size.x, size.y};
	AdjustWindowRect(&rect, m_windowStyle, FALSE);

	SetWindowPos(m_hwnd, HWND_TOP, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
	    SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);

	if (!m_fullscreen)
	{
		m_windowedSize = size;
	}
}
bool WindowWin32::setFullscreen(bool wantFullScreen)
{
	if (wantFullScreen == m_fullscreen)
	{
		return true;
	}

	if (wantFullScreen)
	{
		m_windowedSize = m_size;

		RECT rect = {};
		GetWindowRect(m_hwnd, &rect);
		m_windowedPos.x = rect.left;
		m_windowedPos.y = rect.top;

		int w = GetSystemMetrics(SM_CXSCREEN);
		int h = GetSystemMetrics(SM_CYSCREEN);
		SetWindowLongPtr(m_hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
		SetWindowPos(m_hwnd, HWND_TOP, 0, 0, w, h, SWP_FRAMECHANGED);
	}
	else
	{
		SetWindowLongPtr(m_hwnd, GWL_STYLE, WS_VISIBLE | m_windowStyle);
		RECT rect = { 0, 0, m_windowedSize.x, m_windowedSize.y };
		AdjustWindowRect(&rect, m_windowStyle, FALSE);
		SetWindowPos(m_hwnd, HWND_TOP,
			m_windowedPos.x, m_windowedPos.y,
			rect.right - rect.left, rect.bottom - rect.top,
			SWP_FRAMECHANGED);
	}

	m_fullscreen = wantFullScreen;

	return true;
}
}

#else // RUSH_PLATFORM_WINDOWS

char WindowWin32_cpp_dummy; // suppress linker warning

#endif // RUSH_PLATFORM_WINDOWS
