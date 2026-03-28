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
	virtual void	setMouseLock(bool state) override;
	virtual bool	setFullscreen(bool state) override;

	bool processEvent(NSEvent* event);
	void processResize(float newWidth, float newHeight);
	void updateResolutionScale();

	CAMetalLayer* getMetalLayer() const { return m_metalLayer; }

private:

	NSWindow* m_nativeWindow = nullptr;
	CAMetalLayer* m_metalLayer = nullptr;
	Vec2 m_preLockMousePos = Vec2(0.0f);
	float m_scrollAccumH = 0.0f;
	float m_scrollAccumV = 0.0f;
	Tuple2i m_windowedSize;
	Tuple2i m_windowedPos;
	u32 m_windowedStyleMask = 0;
};

}

#endif
