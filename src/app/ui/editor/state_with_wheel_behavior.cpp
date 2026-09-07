// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/editor/state_with_wheel_behavior.h"

#include "app/app.h"
#include "app/modules/palettes.h"
#include "app/pref/preferences.h"
#include "app/ui/color_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui_context.h"
#include "doc/palette.h"
#include "ui/message.h"
#include "ui/theme.h"

namespace app {

using namespace ui;

enum WHEEL_ACTION { WHEEL_NONE,
                    WHEEL_ZOOM,
                    WHEEL_VSCROLL,
                    WHEEL_HSCROLL,
                    WHEEL_FG,
                    WHEEL_BG };

bool StateWithWheelBehavior::onMouseWheel(Editor* editor, MouseMessage* msg)
{
  gfx::Point delta = msg->wheelDelta();
  double dz = delta.x + delta.y;
  WHEEL_ACTION wheelAction = WHEEL_NONE;

  // Alt+mouse wheel changes the fg/bg colors
  if (msg->altPressed()) {
    if (msg->shiftPressed())
      wheelAction = WHEEL_BG;
    else
      wheelAction = WHEEL_FG;
  }
  // A precise wheel comes from a touchpad/trackpad two-finger swipe:
  // it always freely pans the viewport on both axes (following the
  // physical swipe direction), it never zooms.
  else if (msg->preciseWheel()) {
    View* view = View::getView(editor);
    gfx::Point scroll = view->viewScroll();

    if (Preferences::instance().editor.invertHorizontalScroll())
      delta.x = -delta.x;
    if (Preferences::instance().editor.invertVerticalScroll())
      delta.y = -delta.y;

    editor->setEditorScroll(scroll+delta);
    return true;
  }
  // Regular wheel notches (mouse wheel or a trackpad reporting
  // itself as a mouse): vertical and horizontal movement get the
  // same treatment, either both pan or both zoom. Ctrl toggles
  // between the two modes configured by "Zoom with scroll wheel"
  // (the same modifier used for the pinch gesture in onTouchMagnify).
  else {
    bool zoomMode = Preferences::instance().editor.zoomWithWheel();
    if (msg->ctrlPressed())
      zoomMode = !zoomMode;

    if (zoomMode)
      wheelAction = WHEEL_ZOOM;
    else if (delta.x != 0 || msg->shiftPressed())
      wheelAction = WHEEL_HSCROLL;
    else
      wheelAction = WHEEL_VSCROLL;
  }

  switch (wheelAction) {

    case WHEEL_NONE:
      // Do nothing
      break;

    case WHEEL_FG:
      {
        int lastIndex = get_current_palette()->size()-1;
        int newIndex = ColorBar::instance()->getFgColor().getIndex() + int(dz);
        newIndex = MID(0, newIndex, lastIndex);
        ColorBar::instance()->setFgColor(app::Color::fromIndex(newIndex));
      }
      break;

    case WHEEL_BG:
      {
        int lastIndex = get_current_palette()->size()-1;
        int newIndex = ColorBar::instance()->getBgColor().getIndex() + int(dz);
        newIndex = MID(0, newIndex, lastIndex);
        ColorBar::instance()->setBgColor(app::Color::fromIndex(newIndex));
      }
      break;

    case WHEEL_ZOOM: {
      render::Zoom zoom = editor->zoom();
      zoom = render::Zoom::fromLinearScale(zoom.linearScale() - int(dz));
      setZoom(editor, zoom, msg->position());
      break;
    }

    case WHEEL_HSCROLL:
    case WHEEL_VSCROLL: {
      View* view = View::getView(editor);
      gfx::Point scroll = view->viewScroll();
      gfx::Rect vp = view->viewportBounds();

      if (wheelAction == WHEEL_HSCROLL) {
        delta.x = int(dz * vp.w) / 10;
        delta.y = 0;
      }
      else {
        delta.y = int(dz * vp.h) / 10;
        delta.x = 0;
      }

      if (Preferences::instance().editor.invertHorizontalScroll()) {
        delta.x = -delta.x;
      }
      if (Preferences::instance().editor.invertVerticalScroll()) {
        delta.y = -delta.y;
      }

      editor->setEditorScroll(scroll+delta);
      break;
    }

  }

  return true;
}

bool StateWithWheelBehavior::onTouchMagnify(Editor* editor, ui::TouchMessage* msg)
{
  render::Zoom zoom = editor->zoom();
  render::Zoom newZoom = render::Zoom::fromScale(
    zoom.internalScale() + zoom.internalScale() * msg->magnification());

  // Ctrl+pinch snaps to the closest integer zoom level instead of
  // zooming freely.
  if (msg->ctrlPressed())
    newZoom = render::Zoom::fromLinearScale(newZoom.linearScale());

  setZoom(editor, newZoom, msg->position());
  return true;
}

void StateWithWheelBehavior::setZoom(Editor* editor,
                                     const render::Zoom& zoom,
                                     const gfx::Point& mousePos)
{
  bool center = Preferences::instance().editor.zoomFromCenterWithWheel();

  editor->setZoomAndCenterInMouse(
    zoom, mousePos,
    (center ? Editor::ZoomBehavior::CENTER:
              Editor::ZoomBehavior::MOUSE));
}

} // namespace app
