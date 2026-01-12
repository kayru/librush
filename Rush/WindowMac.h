#pragma once

#include "Window.h"

#if defined(RUSH_PLATFORM_MAC)

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
@class CAMetalLayer;
#else
typedef void CAMetalLayer;
typedef void NSWindow;
typedef void NSEvent;
#endif

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
	void updateResolutionScale();

	CAMetalLayer* getMetalLayer() const { return m_metalLayer; }

private:

	NSWindow* m_nativeWindow = nullptr;
	CAMetalLayer* m_metalLayer = nullptr;
};

}

#endif
