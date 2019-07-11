#pragma once

#include "Window.h"

#if defined(RUSH_PLATFORM_MAC)

#import <Cocoa/Cocoa.h>

namespace Rush
{
	
class WindowMac : public Window
{

public:
	
	WindowMac(const WindowDesc& desc);
	virtual ~WindowMac();

	virtual void*	nativeHandle() override;
	virtual void	setCaption(const char* str) override;
	virtual void	setSize(const Tuple2i& size) override;
	virtual void	setPosition(const Tuple2i& position) override;

	bool processEvent(NSEvent* event);
	void processResize(float newWidth, float newHeight);
private:

	NSWindow* m_nativeWindow = nullptr;

};

}

#endif
