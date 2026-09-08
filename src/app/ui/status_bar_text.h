// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include <string>
#include <vector>

namespace app {

  // A piece of StatusBar::IndicatorsGeneration::add()'s input text: either
  // literal text to show as-is, or the name of an icon (SkinTheme part
  // "icon_<name>") to show in its place.
  struct StatusBarTextToken {
    enum class Kind { Text, Icon };
    Kind kind;
    std::string value;
  };

  // Splits `text` on ":name:" icon markers - a ':' at the start of the
  // string or right after a space, followed by a run of non-':' characters,
  // followed by a ':' that is itself at the end of the string or right
  // before a space - into a sequence of Text/Icon tokens. Anything that
  // doesn't match that exact shape (a lone trailing ':', a ':' with no
  // matching close, etc.) is left as literal text.
  //
  // Pulled out of IndicatorsGeneration::add() (which otherwise needs a live
  // SkinTheme::instance() and Indicators widget to look up/show each icon)
  // so the character-by-character scanning itself - the part regressed by
  // commit cba84ff20 ("Fix freeze and out-of-bounds memory read with : in
  // layer names") - is directly testable.
  std::vector<StatusBarTextToken> tokenizeStatusBarText(const std::string& text);

} // namespace app
