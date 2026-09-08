// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include "app/color.h"
#include "app/pref/preferences.h"

namespace app {

  class Context;

  // Paints a pixel-perfect (non anti-aliased) band of the given width along
  // the edge of the current selection, positioned inside/outside/centered on
  // the selection edge, with the given color/opacity. Used by both Stroke
  // and Quick Stroke (StrokeCommand/QuickStrokeCommand in cmd_stroke.cpp);
  // exposed here (rather than kept file-local) so it can be exercised
  // directly by tests without needing a live ui::Manager to open
  // StrokeCommand's modal dialog.
  void stroke_mask(Context* context, const app::Color& color, int opacity,
                   int width, app::gen::StrokePosition position,
                   const char* actionName);

} // namespace app
