// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include "app/color.h"

namespace app {

  class Context;

  // Fills the pixels of the active layer/cel that fall inside the current
  // selection with the given color/opacity. Used by both Fill and Quick
  // Fill (FillCommand/QuickFillCommand in cmd_fill.cpp); exposed here
  // (rather than kept file-local) so it can be exercised directly by tests
  // without needing a live ui::Manager to open FillCommand's modal
  // color/opacity dialog.
  void fill_mask(Context* context, const app::Color& color, int opacity,
                 const char* actionName);

} // namespace app
