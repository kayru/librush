#include "Window.h"

namespace Rush
{
Window::Window(const WindowDesc& desc)
: m_refs(1)
, m_size({desc.width, desc.height})
, m_pos({0, 0})
, m_resolutionScale(1, 1)
, m_closed(false)
, m_focused(true)
, m_interceptor(nullptr)
{
}

Window::~Window()
{
	// RUSH_ASSERT(m_refs == 0);
}

void Window::addListener(WindowEventListener* listener) { m_listeners.push_back(listener); }

void Window::removeListener(WindowEventListener* listener)
{
	for (size_t i = 0; i < m_listeners.size(); ++i)
	{
		if (m_listeners[i] == listener)
		{
			m_listeners[i] = m_listeners.back();
			m_listeners.pop_back();
			break;
		}
	}
}

void Window::broadcast(const WindowEvent& e)
{
	if (m_interceptor && m_interceptor->processEvent(e))
	{
		return;
	}

	for (size_t i = 0; i < m_listeners.size(); ++i)
	{
		WindowEventListener* listener = m_listeners[i];
		if (listener->mask & (1 << e.type))
		{
			listener->push_back(e);
		}
	}
}

void Window::retain() { m_refs++; }

u32 Window::release()
{
	u32 prevRefCount = m_refs--;
	if (prevRefCount == 1)
	{
		delete this;
	}
	return prevRefCount;
}

const char* toString(Key key)
{
	switch(key)
	{
	default:
	case Key_Unknown: return "Key_Unknown";
	case Key_Space: return "Key_Space";
	case Key_Apostrophe: return "Key_Apostrophe";
	case Key_Comma: return "Key_Comma";
	case Key_Minus: return "Key_Minus";
	case Key_Period: return "Key_Period";
	case Key_Slash: return "Key_Slash";
	case Key_0: return "Key_0";
	case Key_1: return "Key_1";
	case Key_2: return "Key_2";
	case Key_3: return "Key_3";
	case Key_4: return "Key_4";
	case Key_5: return "Key_5";
	case Key_6: return "Key_6";
	case Key_7: return "Key_7";
	case Key_8: return "Key_8";
	case Key_9: return "Key_9";
	case Key_Semicolon: return "Key_Semicolon";
	case Key_Equal: return "Key_Equal";
	case Key_A: return "Key_A";
	case Key_B: return "Key_B";
	case Key_C: return "Key_C";
	case Key_D: return "Key_D";
	case Key_E: return "Key_E";
	case Key_F: return "Key_F";
	case Key_G: return "Key_G";
	case Key_H: return "Key_H";
	case Key_I: return "Key_I";
	case Key_J: return "Key_J";
	case Key_K: return "Key_K";
	case Key_L: return "Key_L";
	case Key_M: return "Key_M";
	case Key_N: return "Key_N";
	case Key_O: return "Key_O";
	case Key_P: return "Key_P";
	case Key_Q: return "Key_Q";
	case Key_R: return "Key_R";
	case Key_S: return "Key_S";
	case Key_T: return "Key_T";
	case Key_U: return "Key_U";
	case Key_V: return "Key_V";
	case Key_W: return "Key_W";
	case Key_X: return "Key_X";
	case Key_Y: return "Key_Y";
	case Key_Z: return "Key_Z";
	case Key_LeftBracket: return "Key_LeftBracket";
	case Key_Backslash: return "Key_Backslash";
	case Key_RightBracket: return "Key_RightBracket";
	case Key_Backquote: return "Key_Backquote";
	case Key_Escape: return "Key_Escape";
	case Key_Enter: return "Key_Enter";
	case Key_Tab: return "Key_Tab";
	case Key_Backspace: return "Key_Backspace";
	case Key_Insert: return "Key_Insert";
	case Key_Delete: return "Key_Delete";
	case Key_Right: return "Key_Right";
	case Key_Left: return "Key_Left";
	case Key_Down: return "Key_Down";
	case Key_Up: return "Key_Up";
	case Key_PageUp: return "Key_PageUp";
	case Key_PageDown: return "Key_PageDown";
	case Key_Home: return "Key_Home";
	case Key_End: return "Key_End";
	case Key_CapsLock: return "Key_CapsLock";
	case Key_ScrollLock: return "Key_ScrollLock";
	case Key_NumLock: return "Key_NumLock";
	case Key_PrintScreen: return "Key_PrintScreen";
	case Key_Pause: return "Key_Pause";
	case Key_F1: return "Key_F1";
	case Key_F2: return "Key_F2";
	case Key_F3: return "Key_F3";
	case Key_F4: return "Key_F4";
	case Key_F5: return "Key_F5";
	case Key_F6: return "Key_F6";
	case Key_F7: return "Key_F7";
	case Key_F8: return "Key_F8";
	case Key_F9: return "Key_F9";
	case Key_F10: return "Key_F10";
	case Key_F11: return "Key_F11";
	case Key_F12: return "Key_F12";
	case Key_F13: return "Key_F13";
	case Key_F14: return "Key_F14";
	case Key_F15: return "Key_F15";
	case Key_F16: return "Key_F16";
	case Key_F17: return "Key_F17";
	case Key_F18: return "Key_F18";
	case Key_F19: return "Key_F19";
	case Key_F20: return "Key_F20";
	case Key_F21: return "Key_F21";
	case Key_F22: return "Key_F22";
	case Key_F23: return "Key_F23";
	case Key_F24: return "Key_F24";
	case Key_F25: return "Key_F25";
	case Key_KP0: return "Key_KP0";
	case Key_KP1: return "Key_KP1";
	case Key_KP2: return "Key_KP2";
	case Key_KP3: return "Key_KP3";
	case Key_KP4: return "Key_KP4";
	case Key_KP5: return "Key_KP5";
	case Key_KP6: return "Key_KP6";
	case Key_KP7: return "Key_KP7";
	case Key_KP8: return "Key_KP8";
	case Key_KP9: return "Key_KP9";
	case Key_KPDecimal: return "Key_KPDecimal";
	case Key_KPDivide: return "Key_KPDivide";
	case Key_KPMultiply: return "Key_KPMultiply";
	case Key_KPSubtract: return "Key_KPSubtract";
	case Key_KPAdd: return "Key_KPAdd";
	case Key_KPEnter: return "Key_KPEnter";
	case Key_KPEqual: return "Key_KPEqual";
	case Key_LeftShift: return "Key_LeftShift";
	case Key_LeftControl: return "Key_LeftControl";
	case Key_LeftAlt: return "Key_LeftAlt";
	case Key_RightShift: return "Key_RightShift";
	case Key_RightControl: return "Key_RightControl";
	case Key_RightAlt: return "Key_RightAlt";
	}
}

const char* toStringShort(Key key)
{
	const char* result = toString(key);
	return result + 4;
}

}
