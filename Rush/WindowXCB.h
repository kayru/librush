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

	// The CLIPBOARD selection, served from this window's event loop (Platform_SetClipboardText)
	void   setClipboardText(const char* text);
	String getClipboardText();

private:

	void processEvent(const xcb_generic_event_t* event);
	void processKeyPress(const xcb_key_press_event_t* event);
	void updateFullscreenState();
	void grabPointer();
	void warpPointer(Vec2 pos);
	Vec2 lockCenter() const;

	// Clipboard (ICCCM CLIPBOARD selection)
	bool processSelectionEvent(const xcb_generic_event_t* event); // true: a selection event, handled
	void serveSelectionRequest(const xcb_selection_request_event_t* request);
	bool convertSelection(xcb_window_t requestor, xcb_atom_t target, xcb_atom_t property, bool nested);
	void continueTransfer(const xcb_property_notify_event_t* event);
	void endTransfer(size_t index);
	bool readSelection(xcb_atom_t target, DynamicArray<char>& out);
	bool takeProperty(DynamicArray<char>& out, xcb_atom_t& type);
	void saveClipboardToManager();
	xcb_timestamp_t eventTime();
	template <typename Accept> xcb_generic_event_t* waitForEvent(Accept&& accept);

	xcb_window_t m_nativeHandle = 0;
	String m_caption;
	Tuple2i m_pendingSize;

	// Events that arrived while waiting for a selection reply, processed by the next pollEvents
	DynamicArray<xcb_generic_event_t*> m_deferredEvents;
	xcb_timestamp_t m_lastEventTime = XCB_CURRENT_TIME;

	String m_clipboardText; // served while m_ownsClipboard
	bool m_ownsClipboard = false;
	xcb_timestamp_t m_clipboardTime = XCB_CURRENT_TIME;

	// INCR transfers: text too large for one request goes in chunks, each after the requestor deletes the last
	struct Transfer
	{
		xcb_window_t requestor = 0;
		xcb_atom_t property = 0;
		String data;
		size_t offset = 0;
	};
	DynamicArray<Transfer> m_transfers;

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
