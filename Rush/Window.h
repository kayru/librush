#pragma once

#include "Rush.h"

#include "MathTypes.h"
#include "UtilTuple.h"
#include "UtilArray.h"

#include "RushC.h"

#include <string.h>

namespace Rush
{
struct WindowEvent;
class WindowEventListener;

enum Key
{
	Key_Unknown      = RUSH_KEY_UNKNOWN,
	Key_Space        = RUSH_KEY_SPACE,
	Key_Apostrophe   = RUSH_KEY_APOSTROPHE,
	Key_Comma        = RUSH_KEY_COMMA,
	Key_Minus        = RUSH_KEY_MINUS,
	Key_Period       = RUSH_KEY_PERIOD,
	Key_Slash        = RUSH_KEY_SLASH,
	Key_0            = RUSH_KEY_0,
	Key_1            = RUSH_KEY_1,
	Key_2            = RUSH_KEY_2,
	Key_3            = RUSH_KEY_3,
	Key_4            = RUSH_KEY_4,
	Key_5            = RUSH_KEY_5,
	Key_6            = RUSH_KEY_6,
	Key_7            = RUSH_KEY_7,
	Key_8            = RUSH_KEY_8,
	Key_9            = RUSH_KEY_9,
	Key_Semicolon    = RUSH_KEY_SEMICOLON,
	Key_Equal        = RUSH_KEY_EQUAL,
	Key_A            = RUSH_KEY_A,
	Key_B            = RUSH_KEY_B,
	Key_C            = RUSH_KEY_C,
	Key_D            = RUSH_KEY_D,
	Key_E            = RUSH_KEY_E,
	Key_F            = RUSH_KEY_F,
	Key_G            = RUSH_KEY_G,
	Key_H            = RUSH_KEY_H,
	Key_I            = RUSH_KEY_I,
	Key_J            = RUSH_KEY_J,
	Key_K            = RUSH_KEY_K,
	Key_L            = RUSH_KEY_L,
	Key_M            = RUSH_KEY_M,
	Key_N            = RUSH_KEY_N,
	Key_O            = RUSH_KEY_O,
	Key_P            = RUSH_KEY_P,
	Key_Q            = RUSH_KEY_Q,
	Key_R            = RUSH_KEY_R,
	Key_S            = RUSH_KEY_S,
	Key_T            = RUSH_KEY_T,
	Key_U            = RUSH_KEY_U,
	Key_V            = RUSH_KEY_V,
	Key_W            = RUSH_KEY_W,
	Key_X            = RUSH_KEY_X,
	Key_Y            = RUSH_KEY_Y,
	Key_Z            = RUSH_KEY_Z,
	Key_LeftBracket  = RUSH_KEY_LEFT_BRACKET,
	Key_Backslash    = RUSH_KEY_BACKSLASH,
	Key_RightBracket = RUSH_KEY_RIGHT_BRACKET,
	Key_Backquote    = RUSH_KEY_BACKQUOTE,
	Key_Escape       = RUSH_KEY_ESCAPE,
	Key_Enter        = RUSH_KEY_ENTER,
	Key_Tab          = RUSH_KEY_TAB,
	Key_Backspace    = RUSH_KEY_BACKSPACE,
	Key_Insert       = RUSH_KEY_INSERT,
	Key_Delete       = RUSH_KEY_DELETE,
	Key_Right        = RUSH_KEY_RIGHT,
	Key_Left         = RUSH_KEY_LEFT,
	Key_Down         = RUSH_KEY_DOWN,
	Key_Up           = RUSH_KEY_UP,
	Key_PageUp       = RUSH_KEY_PAGE_UP,
	Key_PageDown     = RUSH_KEY_PAGE_DOWN,
	Key_Home         = RUSH_KEY_HOME,
	Key_End          = RUSH_KEY_END,
	Key_CapsLock     = RUSH_KEY_CAPS_LOCK,
	Key_ScrollLock   = RUSH_KEY_SCROLL_LOCK,
	Key_NumLock      = RUSH_KEY_NUM_LOCK,
	Key_PrintScreen  = RUSH_KEY_PRINT_SCREEN,
	Key_Pause        = RUSH_KEY_PAUSE,
	Key_F1           = RUSH_KEY_F1,
	Key_F2           = RUSH_KEY_F2,
	Key_F3           = RUSH_KEY_F3,
	Key_F4           = RUSH_KEY_F4,
	Key_F5           = RUSH_KEY_F5,
	Key_F6           = RUSH_KEY_F6,
	Key_F7           = RUSH_KEY_F7,
	Key_F8           = RUSH_KEY_F8,
	Key_F9           = RUSH_KEY_F9,
	Key_F10          = RUSH_KEY_F10,
	Key_F11          = RUSH_KEY_F11,
	Key_F12          = RUSH_KEY_F12,
	Key_F13          = RUSH_KEY_F13,
	Key_F14          = RUSH_KEY_F14,
	Key_F15          = RUSH_KEY_F15,
	Key_F16          = RUSH_KEY_F16,
	Key_F17          = RUSH_KEY_F17,
	Key_F18          = RUSH_KEY_F18,
	Key_F19          = RUSH_KEY_F19,
	Key_F20          = RUSH_KEY_F20,
	Key_F21          = RUSH_KEY_F21,
	Key_F22          = RUSH_KEY_F22,
	Key_F23          = RUSH_KEY_F23,
	Key_F24          = RUSH_KEY_F24,
	Key_F25          = RUSH_KEY_F25,
	Key_KP0          = RUSH_KEY_KP0,
	Key_KP1          = RUSH_KEY_KP1,
	Key_KP2          = RUSH_KEY_KP2,
	Key_KP3          = RUSH_KEY_KP3,
	Key_KP4          = RUSH_KEY_KP4,
	Key_KP5          = RUSH_KEY_KP5,
	Key_KP6          = RUSH_KEY_KP6,
	Key_KP7          = RUSH_KEY_KP7,
	Key_KP8          = RUSH_KEY_KP8,
	Key_KP9          = RUSH_KEY_KP9,
	Key_KPDecimal    = RUSH_KEY_KP_DECIMAL,
	Key_KPDivide     = RUSH_KEY_KP_DIVIDE,
	Key_KPMultiply   = RUSH_KEY_KP_MULTIPLY,
	Key_KPSubtract   = RUSH_KEY_KP_SUBTRACT,
	Key_KPAdd        = RUSH_KEY_KP_ADD,
	Key_KPEnter      = RUSH_KEY_KP_ENTER,
	Key_KPEqual      = RUSH_KEY_KP_EQUAL,
	Key_LeftShift    = RUSH_KEY_LEFT_SHIFT,
	Key_LeftControl  = RUSH_KEY_LEFT_CONTROL,
	Key_LeftAlt      = RUSH_KEY_LEFT_ALT,
	Key_RightShift   = RUSH_KEY_RIGHT_SHIFT,
	Key_RightControl = RUSH_KEY_RIGHT_CONTROL,
	Key_RightAlt     = RUSH_KEY_RIGHT_ALT,

	Key_COUNT = RUSH_KEY_COUNT,
};

const char* toString(Key key);
const char* toStringShort(Key key);

class MouseState
{
public:
	MouseState() : doubleclick(false), pos(0, 0), wheelH(0), wheelV(0) { memset(buttons, 0, sizeof(buttons)); }

	bool buttons[10];
	bool doubleclick;

	Vec2 pos;
	int  wheelH;
	int  wheelV;
};
static_assert(sizeof(MouseState) == sizeof(rush_window_mouse_state), "");

class KeyboardState
{
public:
	KeyboardState() { memset(keys, 0, sizeof(keys)); }

	bool isKeyDown(u8 code) const { return keys[code]; }

	bool keys[Key_COUNT];
};
static_assert(sizeof(KeyboardState) == sizeof(rush_window_keyboard_state), "");

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
	bool        handleShortcutQuit = true;
	bool        handleShortcutFullScreen = true;
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
	virtual void  pollEvents() { }


	void close() { m_closed = true; }
	bool isClosed() const { return m_closed; }
	bool isFocused() const { return m_focused; }
	bool isFullscreen() const { return m_fullScreen; }
	void toggleFullscreen() { setFullscreen(!isFullscreen()); }

	void setFocused(bool focused) { m_focused = focused; }

	int getWidth() const { return m_size.x; }
	int getHeight() const { return m_size.y; }

	int getFramebufferWidth() const { return int(float(m_size.x) * m_resolutionScale.x); }
	int getFramebufferHeight() const { return int(float(m_size.y) * m_resolutionScale.y); }

	Tuple2i getFramebufferSize() const { return {getFramebufferWidth(), getFramebufferHeight()}; }

	Vec2 getResolutionScale() const { return m_resolutionScale; }

	float getAspect() const { return float(m_size.x) / float(m_size.y); }

	const WindowDesc& getDesc() const { return m_desc; }

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

	WindowDesc m_desc;

	u32 m_refs;

	Tuple2i m_size;
	Tuple2i m_pos;
	Vec2    m_resolutionScale;

	bool m_closed;
	bool m_focused;
	bool m_fullScreen;

	MouseState    m_mouse;
	KeyboardState m_keyboard;

	DynamicArray<WindowEventListener*> m_listeners;

private:
	WindowMessageInterceptor* m_interceptor;
};

enum WindowEventType : u32
{
	WindowEventType_Unknown   = RUSH_WINDOW_EVENT_TYPE_UNKNOWN,
	WindowEventType_KeyDown   = RUSH_WINDOW_EVENT_TYPE_KEY_DOWN,
	WindowEventType_KeyUp     = RUSH_WINDOW_EVENT_TYPE_KEY_UP,
	WindowEventType_Resize    = RUSH_WINDOW_EVENT_TYPE_RESIZE,
	WindowEventType_Char      = RUSH_WINDOW_EVENT_TYPE_CHAR,
	WindowEventType_MouseDown = RUSH_WINDOW_EVENT_TYPE_MOUSE_DOWN,
	WindowEventType_MouseUp   = RUSH_WINDOW_EVENT_TYPE_MOUSE_UP,
	WindowEventType_MouseMove = RUSH_WINDOW_EVENT_TYPE_MOUSE_MOVE,
	WindowEventType_Scroll    = RUSH_WINDOW_EVENT_TYPE_SCROLL,

	WindowEventType_COUNT     = RUSH_WINDOW_EVENT_TYPE_COUNT
};

enum WindowEventMask : u32
{
	WindowEventMask_KeyDown   = RUSH_WINDOW_EVENT_MASK_KEY_DOWN,
	WindowEventMask_KeyUp     = RUSH_WINDOW_EVENT_MASK_KEY_UP,
	WindowEventMask_Resize    = RUSH_WINDOW_EVENT_MASK_RESIZE,
	WindowEventMask_Char      = RUSH_WINDOW_EVENT_MASK_CHAR,
	WindowEventMask_MouseDown = RUSH_WINDOW_EVENT_MASK_MOUSE_DOWN,
	WindowEventMask_MouseUp   = RUSH_WINDOW_EVENT_MASK_MOUSE_UP,
	WindowEventMask_MouseMove = RUSH_WINDOW_EVENT_MASK_MOUSE_MOVE,
	WindowEventMask_Scroll    = RUSH_WINDOW_EVENT_MASK_SCROLL,
	WindowEventMask_Key       = RUSH_WINDOW_EVENT_MASK_KEY,
	WindowEventMask_Mouse     = RUSH_WINDOW_EVENT_MASK_MOUSE,
	WindowEventMask_All       = RUSH_WINDOW_EVENT_MASK_ALL,
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
static_assert(sizeof(WindowEvent) == sizeof(rush_window_event), "");

class WindowEventListener : public DynamicArray<WindowEvent>
{
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
		DynamicArray<WindowEvent>::operator=(rhs);
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

	void clear() { DynamicArray<WindowEvent>::clear(); }

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
