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
	virtual void  setMouseLock(bool state) override;

private:

	void setCaptionInternal(const char* str);
	void grabPointer();
	void warpPointer(Vec2 pos);
	Vec2 lockCenter() const;

	xcb_window_t m_nativeHandle = 0;
	String m_caption;
	Tuple2i m_pendingSize;
	xcb_intern_atom_reply_t* m_closeReply = nullptr;

	// Mouse lock: hidden, confined cursor warped back to the window center;
	// motion accumulates into m_mouse.pos as relative deltas
	xcb_cursor_t m_hiddenCursor = 0;
	bool m_pointerGrabbed = false;
	Vec2 m_preLockMousePos = Vec2(0.0f);
	Vec2 m_lockPointerPos = Vec2(0.0f); // last known real pointer position
	bool m_warpPending = false;
	u16 m_warpSequence = 0; // motion events from this request on are post-warp
};
}

#endif // RUSH_PLATFORM_LINUX
