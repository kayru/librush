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
}
