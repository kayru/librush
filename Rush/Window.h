#pragma once

#include "Rush.h"

#include "MathTypes.h"
#include "UtilTuple.h"

#include <vector>
#include <cstring>

namespace Rush
{
struct WindowEvent;
class WindowEventListener;

enum Key
{
	Key_Unknown      = 0,
	Key_Space        = ' ',
	Key_Apostrophe   = '\'',
	Key_Comma        = ',',
	Key_Minus        = '-',
	Key_Period       = '.',
	Key_Slash        = '/',
	Key_0            = '0',
	Key_1            = '1',
	Key_2            = '2',
	Key_3            = '3',
	Key_4            = '4',
	Key_5            = '5',
	Key_6            = '6',
	Key_7            = '7',
	Key_8            = '8',
	Key_9            = '9',
	Key_Semicolon    = ';',
	Key_Equal        = '=',
	Key_A            = 'A',
	Key_B            = 'B',
	Key_C            = 'C',
	Key_D            = 'D',
	Key_E            = 'E',
	Key_F            = 'F',
	Key_G            = 'G',
	Key_H            = 'H',
	Key_I            = 'I',
	Key_J            = 'J',
	Key_K            = 'K',
	Key_L            = 'L',
	Key_M            = 'M',
	Key_N            = 'N',
	Key_O            = 'O',
	Key_P            = 'P',
	Key_Q            = 'Q',
	Key_R            = 'R',
	Key_S            = 'S',
	Key_T            = 'T',
	Key_U            = 'U',
	Key_V            = 'V',
	Key_W            = 'W',
	Key_X            = 'X',
	Key_Y            = 'Y',
	Key_Z            = 'Z',
	Key_LeftBracket  = '[',
	Key_Backslash    = '\\',
	Key_RightBracket = ']',
	Key_Escape,
	Key_Enter,
	Key_Tab,
	Key_Backspace,
	Key_Insert,
	Key_Delete,
	Key_Right,
	Key_Left,
	Key_Down,
	Key_Up,
	Key_PageUp,
	Key_PageDown,
	Key_Home,
	Key_End,
	Key_CapsLock,
	Key_ScrollLock,
	Key_NumLock,
	Key_PrintScreen,
	Key_Pause,
	Key_F1,
	Key_F2,
	Key_F3,
	Key_F4,
	Key_F5,
	Key_F6,
	Key_F7,
	Key_F8,
	Key_F9,
	Key_F10,
	Key_F11,
	Key_F12,
	Key_F13,
	Key_F14,
	Key_F15,
	Key_F16,
	Key_F17,
	Key_F18,
	Key_F19,
	Key_F20,
	Key_F21,
	Key_F22,
	Key_F23,
	Key_F24,
	Key_F25,
	Key_KP0,
	Key_KP1,
	Key_KP2,
	Key_KP3,
	Key_KP4,
	Key_KP5,
	Key_KP6,
	Key_KP7,
	Key_KP8,
	Key_KP9,
	Key_KPDecimal,
	Key_KPDivide,
	Key_KPMultiply,
	Key_KPSubtract,
	Key_KPAdd,
	Key_KPEnter,
	Key_KPEqual,
	Key_LeftShift,
	Key_LeftControl,
	Key_LeftAlt,
	Key_RightShift,
	Key_RightControl,
	Key_RightAlt,

	Key_COUNT,
};

class MouseState
{
public:
	MouseState() : doubleclick(false), pos(0, 0), wheelH(0), wheelV(0) { std::memset(buttons, 0, sizeof(buttons)); }

	bool buttons[10];
	bool doubleclick;

	Vec2 pos;
	int  wheelH;
	int  wheelV;
};

class KeyboardState
{
public:
	KeyboardState() { std::memset(keys, 0, sizeof(keys)); }

	bool isKeyDown(u8 code) const { return keys[code]; }

	bool keys[Key_COUNT];
};

class WindowMessageInterceptor
{
public:
	virtual ~WindowMessageInterceptor() {}
	virtual bool processEvent(const WindowEvent&) = 0;
};

struct WindowDesc
{
	const char* caption    = nullptr;
	int         width      = 0;
	int         height     = 0;
	bool        resizable  = false;
	bool        fullScreen = false;
};

class Window
{
	RUSH_DISALLOW_COPY_AND_ASSIGN(Window);

public:
	Window(const WindowDesc& desc);

	virtual void* nativeConnection() { return nullptr; }
	virtual void* nativeHandle()               = 0;
	virtual void  setCaption(const char* str)  = 0;
	virtual void  setSize(const Tuple2i& size) = 0;
	virtual void  setPosition(const Tuple2i& position){};
	virtual void  setMouseLock(bool state){};
	virtual bool  setFullscreen(bool state) { return false; }

	void close() { m_closed = true; }
	bool isClosed() const { return m_closed; }
	bool isFocused() const { return m_focused; }

	void setFocused(bool focused) { m_focused = focused; }

	int getWidth() const { return m_size.x; }
	int getHeight() const { return m_size.y; }

	int getFramebufferWidth() const { return int(float(m_size.x) * m_resolutionScale.x); }
	int getFramebufferHeight() const { return int(float(m_size.y) * m_resolutionScale.y); }

	Tuple2i getFramebufferSize() const { return {getFramebufferWidth(), getFramebufferHeight()}; }

	Vec2 getResolutionScale() const { return m_resolutionScale; }

	float getAspect() const { return float(m_size.x) / float(m_size.y); }

	const Vec2     getSizeFloat() const { return Vec2((float)m_size.x, (float)m_size.y); }
	const Tuple2i& getSize() const { return m_size; }
	const Tuple2i& getPosition() const { return m_pos; }

	const MouseState&    getMouseState() const { return m_mouse; }
	const KeyboardState& getKeyboardState() const { return m_keyboard; }

	void setMessageInterceptor(WindowMessageInterceptor* ptr) { m_interceptor = ptr; }

	void addListener(WindowEventListener* listener);
	void removeListener(WindowEventListener* listener);

	void broadcast(const WindowEvent& e);

	// Reference counting

	void retain();
	u32  release();

protected:
	virtual ~Window();

	u32 m_refs;

	Tuple2i m_size;
	Tuple2i m_pos;
	Vec2    m_resolutionScale;

	bool m_closed;
	bool m_focused;

	MouseState    m_mouse;
	KeyboardState m_keyboard;

	std::vector<WindowEventListener*> m_listeners;

private:
	WindowMessageInterceptor* m_interceptor;
};

enum WindowEventType
{
	WindowEventType_Unknown,

	WindowEventType_KeyDown,
	WindowEventType_KeyUp,
	WindowEventType_Resize,
	WindowEventType_Char,
	WindowEventType_MouseDown,
	WindowEventType_MouseUp,
	WindowEventType_MouseMove,
	WindowEventType_Scroll,

	WindowEventType_COUNT
};

enum WindowEventMask : u32
{
	WindowEventMask_KeyDown   = 1 << WindowEventType_KeyDown,
	WindowEventMask_KeyUp     = 1 << WindowEventType_KeyUp,
	WindowEventMask_Resize    = 1 << WindowEventType_Resize,
	WindowEventMask_Char      = 1 << WindowEventType_Char,
	WindowEventMask_MouseDown = 1 << WindowEventType_MouseDown,
	WindowEventMask_MouseUp   = 1 << WindowEventType_MouseUp,
	WindowEventMask_MouseMove = 1 << WindowEventType_MouseMove,
	WindowEventMask_Scroll    = 1 << WindowEventType_Scroll,

	WindowEventMask_Key   = WindowEventMask_KeyDown | WindowEventMask_KeyUp,
	WindowEventMask_Mouse = WindowEventMask_MouseDown | WindowEventMask_MouseUp | WindowEventMask_MouseMove,

	WindowEventMask_All = ~0U
};

struct WindowEvent
{
	WindowEventType type = WindowEventType_Unknown;

	Key code      = Key_Unknown;
	u32 character = 0;
	u32 modifiers = 0;

	u32 width  = 0;
	u32 height = 0;

	Vec2 pos         = Vec2(0.0f);
	u32  button      = 0;
	bool doubleClick = false;

	Vec2 scroll = Vec2(0.0f);

	static WindowEvent Resize(u32 _width, u32 _height)
	{
		WindowEvent e;
		e.type   = WindowEventType_Resize;
		e.width  = _width;
		e.height = _height;
		return e;
	}

	static WindowEvent Char(u32 _code)
	{
		WindowEvent e;
		e.type      = WindowEventType_Char;
		e.character = _code;
		return e;
	}

	static WindowEvent KeyDown(Key _code)
	{
		WindowEvent e;
		e.type = WindowEventType_KeyDown;
		e.code = _code;
		return e;
	}

	static WindowEvent KeyUp(Key _code)
	{
		WindowEvent e;
		e.type = WindowEventType_KeyUp;
		e.code = _code;
		return e;
	}

	static WindowEvent MouseDown(const Vec2& _pos, u32 _button, bool _doubleclick)
	{
		WindowEvent e;
		e.type        = WindowEventType_MouseDown;
		e.pos         = _pos;
		e.doubleClick = _doubleclick;
		e.button      = _button;
		return e;
	}

	static WindowEvent MouseUp(const Vec2& _pos, u32 _button)
	{
		WindowEvent e;
		e.type   = WindowEventType_MouseUp;
		e.pos    = _pos;
		e.button = _button;
		return e;
	}

	static WindowEvent MouseMove(const Vec2& _pos)
	{
		WindowEvent e;
		e.type = WindowEventType_MouseMove;
		e.pos  = _pos;
		return e;
	}

	static WindowEvent Scroll(float x, float y)
	{
		WindowEvent e;
		e.type     = WindowEventType_Scroll;
		e.scroll.x = x;
		e.scroll.y = y;
		return e;
	}
};

class WindowEventListener : public std::vector<WindowEvent>
{
public:
	template <typename IDT> struct ID
	{
	};

public:
	WindowEventListener(u32 _mask = WindowEventMask_All) : owner(nullptr), mask(_mask) {}

	WindowEventListener(Window* _owner, u32 _mask = WindowEventMask_All) : owner(_owner), mask(_mask)
	{
		registerListener();
	}

	WindowEventListener(const WindowEventListener& rhs) : owner(rhs.owner), mask(rhs.mask) { registerListener(); }

	virtual ~WindowEventListener() { unregisterListener(); }

	WindowEventListener& operator=(const WindowEventListener& rhs)
	{
		owner = rhs.owner;
		registerListener();
		std::vector<WindowEvent>::operator=(rhs);
		return *this;
	}

	void setOwner(Window* _owner)
	{
		Window* oldOwner = owner;
		if (oldOwner)
		{
			oldOwner->retain();
		}

		unregisterListener();
		owner = _owner;
		registerListener();

		if (oldOwner)
		{
			oldOwner->release();
		}
	}

	void clear() { std::vector<WindowEvent>::clear(); }

private:
	void registerListener()
	{
		if (owner)
		{
			owner->addListener(this);
			owner->retain();
		}
	}

	void unregisterListener()
	{
		if (owner)
		{
			owner->removeListener(this);
			owner->release();
			owner = nullptr;
		}
	}

public:
	Window* owner;
	u32     mask;
};
}
