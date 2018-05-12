#pragma once

#ifdef RUSH_PLATFORM_LINUX

#include "Window.h"

#include <string>
#include <xcb/xcb.h>

namespace Rush
{

class WindowXCB : public Window
{

public:
	WindowXCB(const WindowDesc& desc);
	virtual ~WindowXCB();

	virtual void* nativeConnection() override;
	virtual void* nativeHandle() override { return (void*)uintptr_t(m_nativeHandle); };
	virtual void  setCaption(const char* str) override;
	virtual void  setSize(const Tuple2i& size) override;
	virtual bool  setFullscreen(bool state) override;

private:
	xcb_window_t m_nativeHandle = 0;
	std::string m_caption;
};
}

#endif // RUSH_PLATFORM_LINUX
