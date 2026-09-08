// Aseprite    | Copyright (C) 2001-2016  David Capello
// LibreSprite | Copyright (C)      2024  LibreSprite contributors
// Besprited   | Copyright (C)      2026  Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include "app/ui/skin/skin_property.h"
#include "base/exception.h"
#include "gfx/rect.h"
#include "ui/base.h"

namespace ui {
  class ButtonBase;
  class CheckBox;
  class Message;
  class RadioButton;
  class Widget;
  class Window;
}

namespace app {
  class Command;
  class Document;
  class Params;

  namespace tools {
    class Tool;
  }

  int init_module_gui();
  void exit_module_gui();

  // First-run UI-scale guess: picks a scale from the primary display's
  // resolution, keeping the post-scale (logical) screen height usable for
  // the editor (~400px is the practical minimum). `screenH` is the real
  // desktop height when known; when it isn't (<= 0), `fallbackH` (the
  // initial small window's height) is used instead. Thresholds are
  // deliberately conservative since on Hi-DPI setups SDL may report either
  // pixels or points depending on the platform.
  int guessUiScale(int screenH, int fallbackH);

  void update_screen_for_document(const Document* document);

  void load_window_pos(ui::Widget* window, const char *section);
  void save_window_pos(ui::Widget* window, const char *section);

  ui::Widget* setup_mini_font(ui::Widget* widget);
  ui::Widget* setup_mini_look(ui::Widget* widget);
  ui::Widget* setup_look(ui::Widget* widget, skin::LookType lookType);
  void setup_bevels(ui::Widget* widget, int b1, int b2, int b3, int b4);

  ui::CheckBox* check_button_new(const char* text, int b1, int b2, int b3, int b4);

  // This function can be used to reinvalidate a specific rectangle if
  // we weren't able to validate it on a onPaint() event. E.g. Because
  // the current document was locked.
  void defer_invalid_rect(const gfx::Rect& rc);

} // namespace app
