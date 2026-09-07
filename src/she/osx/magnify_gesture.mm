// SHE library
// Copyright (C) 2021 LibreSprite contributors
// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// SDL2's Cocoa backend never forwards trackpad pinch (magnify) gestures
// as an SDL event, so we swizzle NSView::magnifyWithEvent: on SDL's own
// content view to queue our own TouchMagnify events instead.

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#else
#include <SDL.h>
#include <SDL_syswm.h>
#endif

#include "gfx/point.h"
#include "she/event.h"
#include "she/event_queue.h"
#include "she/sdl2/sdl2_display.h"

namespace {

IMP g_originalMagnifyWithEvent = nullptr;

void custom_magnifyWithEvent(id self, SEL _cmd, NSEvent* event)
{
  int scale = she::unique_display ? she::unique_display->scale() : 1;
  NSView* view = (NSView*)self;
  NSPoint point = [view convertPoint:[event locationInWindow] fromView:nil];

  she::Event ev;
  ev.setType(she::Event::TouchMagnify);
  ev.setMagnification(event.magnification);
  ev.setPointerType(she::PointerType::Multitouch);
  ev.setPosition(gfx::Point(
    point.x / scale,
    (view.bounds.size.height - point.y) / scale));

  she::queue_event(ev);

  if (g_originalMagnifyWithEvent)
    ((void (*)(id, SEL, NSEvent*))g_originalMagnifyWithEvent)(self, _cmd, event);
}

} // anonymous namespace

namespace osx_magnify {

bool init(SDL_Window* window)
{
  if (!window)
    return false;

  SDL_SysWMinfo wmInfo;
  SDL_VERSION(&wmInfo.version);
  if (!SDL_GetWindowWMInfo(window, &wmInfo))
    return false;
  if (wmInfo.subsystem != SDL_SYSWM_COCOA)
    return false;

  NSWindow* nsWindow = wmInfo.info.cocoa.window;
  NSView* contentView = nsWindow ? [nsWindow contentView] : nil;
  if (!contentView)
    return false;

  Class viewClass = [contentView class];
  SEL magnifySel = @selector(magnifyWithEvent:);
  Method existingMethod = class_getInstanceMethod(viewClass, magnifySel);
  if (existingMethod) {
    g_originalMagnifyWithEvent =
      method_setImplementation(existingMethod, (IMP)custom_magnifyWithEvent);
  }
  else {
    class_addMethod(viewClass, magnifySel, (IMP)custom_magnifyWithEvent, "v@:@");
  }

  if (@available(macOS 10.12.2, *)) {
    contentView.allowedTouchTypes = NSTouchTypeMaskDirect | NSTouchTypeMaskIndirect;
  }

  return true;
}

} // namespace osx_magnify
