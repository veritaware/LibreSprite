// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include "tinyxml2.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace app::skin {

  // Parses the <skin><fonts> section of a skin.xml document into ordered
  // (file, size) candidate lists for the main and mini font families -
  // SkinTheme::loadFontFamiliesFromSkinXml()'s actual logic, pulled out so
  // it's reachable with an in-memory tinyxml2::XMLDocument and a fake
  // `resolveFontFile` instead of a real skins/fonts directory on disk.
  //
  // Within each <family>, the entry whose `lang` attribute list contains
  // `lang` goes first, then the language-less (default) entry, then any
  // remaining entries - same fallback order loadFont() then tries them in.
  // A candidate is only appended to the output list if resolveFontFile()
  // finds it (returns a path); candidates that don't resolve are dropped
  // silently, same as findSkinFontFile() failing in the real code path.
  void parseFontFamiliesFromSkinXml(
    const tinyxml2::XMLDocument& doc,
    const std::string& lang,
    const std::function<std::optional<std::string>(const std::string& fontName)>& resolveFontFile,
    std::vector<std::pair<std::string, size_t>>& mainFonts,
    std::vector<std::pair<std::string, size_t>>& miniFonts);

} // namespace app::skin
