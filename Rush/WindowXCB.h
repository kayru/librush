#pragma once

#ifdef RUSH_PLATFORM_LINUX

#include "Window.h"
#include "UtilString.h"

#include <xcb/xcb.h>

namespace Rush
{

class WindowXCB final : public Window
{

public:
	WindowXCB(const WindowDesc& desc);
	virtual ~WindowXCB();

	virtual void* nativeConnection() override;
	virtual void* nativeHandle() override { return (void*)uintptr_t(m_nativeHandle); };
	virtual void  setCaption(const char* str) override;
	virtual void  setSize(const Tuple2i& size) override;
	virtual bool  setFullscreen(bool state) override;
	virtual void  pollEvents() override;

private:

	void setCaptionInternal(const char* str);

	xcb_window_t m_nativeHandle = 0;
	String m_caption;
	Tuple2i m_pendingSize;
	xcb_intern_atom_reply_t* m_closeReply = nullptr;
};
}

#endif // RUSH_PLATFORM_LINUX
