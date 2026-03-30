#pragma once

#if defined(RUSH_PLATFORM_MAC) || defined(RUSH_PLATFORM_IOS)

#include "Window.h"

#ifdef __OBJC__

#if defined(RUSH_PLATFORM_IOS)
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif // RUSH_PLATFORM_IOS

@class CAMetalLayer;

#if defined(RUSH_PLATFORM_IOS)

@interface RushMetalView : UIView
{
	@public Rush::Window* parent;
}
@end

@interface RushViewController : UIViewController
{
	@public Rush::Window* parent;
}
@end

#endif // RUSH_PLATFORM_IOS

#else // __OBJC__

typedef void CAMetalLayer;

#if defined(RUSH_PLATFORM_MAC)
typedef void NSWindow;
typedef void NSEvent;
#else
typedef void UIWindow;
typedef void RushViewController;
typedef void RushMetalView;
#endif // RUSH_PLATFORM_MAC

#endif // __OBJC__

namespace Rush
{

class WindowApple : public Window
{
public:
	WindowApple(const WindowDesc& desc) : Window(desc) {}

	CAMetalLayer* getMetalLayer() const { return m_metalLayer; }

protected:
	CAMetalLayer* m_metalLayer = nullptr;
};

#if defined(RUSH_PLATFORM_MAC)

class WindowMac : public WindowApple
{
public:
	WindowMac(const WindowDesc& desc);
	virtual ~WindowMac();

	virtual void*          nativeHandle() override;
	virtual void           setCaption(const char* str) override;
	virtual void           setSize(const Tuple2i& size) override;
	virtual void           setPosition(const Tuple2i& position) override;
	virtual void           setMouseLock(bool state) override;
	virtual bool           setFullscreen(bool state) override;
	virtual Box2 getSafeArea() const override;

	bool processEvent(NSEvent* event);
	void processResize(float newWidth, float newHeight);
	void updateResolutionScale();

private:
	NSWindow* m_nativeWindow = nullptr;
	Vec2 m_preLockMousePos = Vec2(0.0f);
	float m_scrollAccumH = 0.0f;
	float m_scrollAccumV = 0.0f;
	Tuple2i m_windowedSize;
	Tuple2i m_windowedPos;
	u32 m_windowedStyleMask = 0;
};

#elif defined(RUSH_PLATFORM_IOS)

class WindowIOS : public WindowApple
{
public:
	WindowIOS(const WindowDesc& desc);
	~WindowIOS();

	virtual void*          nativeHandle() override;
	virtual void           setCaption(const char*) override {}
	virtual void           setSize(const Tuple2i&) override {}
	virtual void           setPosition(const Tuple2i&) override {}
	virtual void           setMouseLock(bool) override {}
	virtual bool           setFullscreen(bool) override { return true; }
	virtual Box2 getSafeArea() const override;
	virtual ArrayView<const TouchPoint> getTouches() const override { return m_touches; }

	void setUIWindow(UIWindow* window) { m_uiWindow = window; }
	void setViewController(RushViewController* vc);
	void updateResolutionScale();

	void setKeyDown(Key key) { m_keyboard.keys[key] = true; }
	void setKeyUp(Key key) { m_keyboard.keys[key] = false; }

	u64  touchBegan(void* nativeId, const Vec2& pos);
	void touchMoved(void* nativeId, const Vec2& pos);
	void touchEnded(void* nativeId, const Vec2& pos);

private:
	int findTouchByNativeId(void* nativeId) const;
	int findTouchById(u64 id) const;

	DynamicArray<TouchPoint> m_touches;
	DynamicArray<void*>      m_nativeTouchIds;
	u64                      m_nextTouchId = 1;

	UIWindow* m_uiWindow = nullptr;
	RushViewController* m_viewController = nullptr;
};

#endif // RUSH_PLATFORM_MAC

}

#endif // RUSH_PLATFORM_MAC || RUSH_PLATFORM_IOS
