// Aseprite    | Copyright (C) 2001-2016 David Capello
// LibreSprite | Copyright (C) 2016-2026 LibreSprite contributors
// Besprited   | Copyright (C) 2026      Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <optional>
#include <iostream>

#include "app/file_system.h"
#include "app/modules/gui.h"
#include "app/modules/i18n.h"
#include "app/pref/preferences.h"
#include "app/resource_finder.h"
#include "app/ui/app_menuitem.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/skin/button_icon_impl.h"
#include "app/ui/skin/skin_property.h"
#include "app/ui/skin/skin_slider_property.h"
#include "app/ui/skin/skin_style_property.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/skin/skin_theme_fonts.h"

#include <ranges>

#include "app/ui/skin/style.h"
#include "app/ui/skin/style_sheet.h"
#include "app/xml_document.h"
#include "base/fs.h"
#include "base/path.h"
#include "base/string.h"
#include "css/sheet.h"
#include "gfx/border.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "she/font.h"
#include "she/surface.h"
#include "she/system.h"
#include "ui/intern.h"
#include "ui/ui.h"

#include "tinyxml2.h"

#define BGCOLOR (getWidgetBgColor(widget))

namespace app::skin
{
  using namespace gfx;
  using namespace ui;

  const char* SkinTheme::kThemeCloseButtonId = "theme_close_button";

  // Controls the "X" button in a window to close it.
  class WindowCloseButton : public Button {
  public:
    WindowCloseButton() : Button("") {
      setup_bevels(this, 0, 0, 0, 0);
      setDecorative(true);
      setId(SkinTheme::kThemeCloseButtonId);
    }

  protected:
    void onClick(Event& ev) override {
      Button::onClick(ev);
      closeWindow();
    }

    void onPaint(PaintEvent& ev) override {
      dynamic_cast<SkinTheme*>(theme())->paintWindowButton(ev);
    }

    bool onProcessMessage(Message* msg) override {
      switch (msg->type()) {
        case kSetCursorMessage:
          set_mouse_cursor(kArrowCursor);
          return true;
        case kKeyDownMessage:
          if (window()->isForeground() &&
            dynamic_cast<KeyMessage*>(msg)->scancode() == kKeyEsc) {
            setSelected(true);
            return true;
          }
          break;
        case kKeyUpMessage:
          if (window()->isForeground() &&
            dynamic_cast<KeyMessage*>(msg)->scancode() == kKeyEsc) {
            if (isSelected()) {
              setSelected(false);
              closeWindow();
              return true;
            }
          }
          break;
        default: break;
      }

      return Button::onProcessMessage(msg);
    }
  };

  static const char* cursor_names[kCursorTypes] = {
    "null",                       // kNoCursor
    "normal",                     // kArrowCursor
    "normal_add",                 // kArrowPlusCursor
    "forbidden",                  // kForbiddenCursor
    "hand",                       // kHandCursor
    "scroll",                     // kScrollCursor
    "move",                       // kMoveCursor

    "size_ns",                    // kSizeNSCursor
    "size_we",                    // kSizeWECursor

    "size_n",                     // kSizeNCursor
    "size_ne",                    // kSizeNECursor
    "size_e",                     // kSizeECursor
    "size_se",                    // kSizeSECursor
    "size_s",                     // kSizeSCursor
    "size_sw",                    // kSizeSWCursor
    "size_w",                     // kSizeWCursor
    "size_nw",                    // kSizeNWCursor

    "rotate_n",                   // kRotateNCursor
    "rotate_ne",                  // kRotateNECursor
    "rotate_e",                   // kRotateECursor
    "rotate_se",                  // kRotateSECursor
    "rotate_s",                   // kRotateSCursor
    "rotate_sw",                  // kRotateSWCursor
    "rotate_w",                   // kRotateWCursor
    "rotate_nw",                  // kRotateNWCursor

    "eyedropper",                 // kEyedropperCursor
    "magnifier"                   // kMagnifierCursor
  };

  static css::Value value_or_none(const char* valueStr)
  {
    if (strcmp(valueStr, "none") == 0)
      return {};

    return css::Value(valueStr);
  }

  // static
  SkinTheme* SkinTheme::instance()
  {
    return dynamic_cast<SkinTheme*>(Manager::getDefault()->theme());
  }

  SkinTheme::SkinTheme()
    : m_cursors(kCursorTypes, nullptr)
  {
    m_defaultFont = nullptr;
    m_miniFont = nullptr;

    // Initialize all graphics in nullptr (these bitmaps are loaded from the skin)
    m_sheet = nullptr;
  }

  SkinTheme::~SkinTheme()
  {
    // Delete all cursors.
    for (const auto& m_cursor : m_cursors)
      delete m_cursor;

    for (const auto& val : m_toolicon | std::views::values) {
      val->dispose();
    }

    if (m_sheet)
      m_sheet->dispose();

    m_parts_by_id.clear();
  }

  gfx::Color SkinTheme::getColorById(const std::string& id) {
    auto it = m_colors_by_id.find(id);
    const auto end = m_colors_by_id.end();
    if (it != end)
      return it->second;
    std::string found;
    if (it == end && id.rfind("_face") != std::string::npos) {
      found = "face";
      it = m_colors_by_id.find(found);
    }
    if (it == end && id.rfind("_text") != std::string::npos) {
      found = "text";
      it = m_colors_by_id.find(found);
    }
    if (it != end) {
      m_colors_by_id[id] = it->second;
      std::cout << "assigning color " << found << " to " << id << std::endl;
      return it->second;
    }
    std::cout << "Could not find color " << id << std::endl;
    return 0xFF00FFFF;
  }

  void SkinTheme::onRegenerate()
  {
    // First we load the skin from default theme, which is more proper
    // to have every single needed skin part/color/dimension.
    // Then we load the selected theme to redefine default theme parts.
    if (Preferences& pref = Preferences::instance(); pref.theme.selected.defaultValue() != pref.theme.selected()) {
      auto skinId = pref.theme.selected();
      try {
        loadSheet(skinId);
      } catch (const base::Exception&) {
        loadSheet(pref.theme.selected.defaultValue());
      }
      loadXml(pref.theme.selected.defaultValue());
      try { loadXml(skinId); } catch (const base::Exception&) {}
    } else {
      loadSheet(pref.theme.selected.defaultValue());
      loadXml(pref.theme.selected.defaultValue());
    }
  }

  void SkinTheme::loadSheet(const std::string& skinId)
  {
    TRACE("SkinTheme::loadSheet(%s)\n", skinId.c_str());

    if (m_sheet) {
      m_sheet->dispose();
      m_sheet = nullptr;
    }

    // Load the skin sheet
    const std::string sheet_filename("skins/" + skinId + "/sheet.png");

    ResourceFinder rf;
    rf.includeDataDir(sheet_filename.c_str());
    if (!rf.findFirst())
      throw base::Exception("File %s not found", sheet_filename.c_str());

    try {
      m_sheet = she::instance()->loadRgbaSurface(rf.filename().c_str());
    }
    catch (...) {
      throw base::Exception("Error loading %s file", sheet_filename.c_str());
    }
  }

  void SkinTheme::loadFonts(const std::string& skinId)
  {
    typedef std::vector<std::pair<std::string, size_t>> FontList;
    TRACE("SkinTheme::loadFonts(%s)\n", skinId.c_str());
    Preferences& pref = Preferences::instance();
    auto findResources = [&](const std::string& path, const size_t fontSize, std::vector<std::pair<std::string, size_t>>& output) {
      ResourceFinder rf;
      rf.includeDataDir(path.c_str());
      while (rf.next()) {
        output.emplace_back(rf.filename(), fontSize);
      }
    };

    FontList customMain, customMini;
    loadFontFamiliesFromSkinXml(skinId, customMain, customMini);

    // ToDo: add the ability to set preferred font size in skin XML + application preferences
    {
      FontList fonts{};

      if (auto userFont = pref.theme.font(); !userFont.empty()) {
        ResourceFinder rf;
        rf.addPath(userFont);
        while (rf.next()) {
          fonts.emplace_back(rf.filename(), 8);
        }
      }

      if (!customMain.empty()) {
        fonts.insert(fonts.end(), customMain.begin(), customMain.end());
      } else {
        FontList dataDirs {
          std::make_pair("skins/" + skinId + "/font-" + getLanguage() + ".otf", 8),
          std::make_pair("skins/" + skinId + "/font-" + getLanguage() + ".ttf", 8),
          std::make_pair("skins/" + skinId + "/font-" + getLanguage() + ".png", 7),
          std::make_pair("fonts/noto-" + getLanguage() + ".ttf", 8),
          std::make_pair("skins/" + skinId + "/font.otf", 8),
          std::make_pair("skins/" + skinId + "/font.ttf", 8),
          std::make_pair("skins/" + skinId + "/font.png", 7),
          std::make_pair("fonts/pixeloid-sans.ttf", 9),
        };
        for (auto& [path, size] : dataDirs) {
          findResources(path, size, fonts);
        }
      }

      m_defaultFont = loadFont(fonts);
    }

    {
      FontList fonts;
      if (auto userFont = pref.theme.miniFont(); !userFont.empty()) {
        ResourceFinder rf;
        rf.addPath(userFont);
        while (rf.next()) {
          fonts.emplace_back(rf.filename(), 8);
        }
      }

      if (!customMini.empty()) {
        fonts.insert(fonts.end(), customMini.begin(), customMini.end());
      } else {
        FontList dataDirs {
          std::make_pair("skins/" + skinId + "/minifont-" + getLanguage() + ".otf", 6),
          std::make_pair("skins/" + skinId + "/minifont-" + getLanguage() + ".ttf", 6),
          std::make_pair("skins/" + skinId + "/minifont-" + getLanguage() + ".png", 6),
          std::make_pair("skins/" + skinId + "/font-" + getLanguage() + ".otf", 6),
          std::make_pair("skins/" + skinId + "/font-" + getLanguage() + ".ttf", 6),
          std::make_pair("fonts/noto-" + getLanguage() + ".ttf", 6),
          std::make_pair("skins/" + skinId + "/minifont.otf", 6),
          std::make_pair("skins/" + skinId + "/minifont.ttf", 6),
          std::make_pair("skins/" + skinId + "/minifont.png", 6),
          std::make_pair("skins/" + skinId + "/font.otf", 6),
          std::make_pair("skins/" + skinId + "/font.ttf", 6),
          std::make_pair("fonts/tiny5.ttf", 8),
        };
        for (auto& [path, size] : dataDirs) {
          findResources(path, size, fonts);
        }
      }

      m_miniFont = loadFont(fonts);
    }
  }

  static std::optional<std::string> findFile(const std::string& name) {
    {
      ResourceFinder rf;
      rf.includeDataDir(name.c_str());
      if (rf.findFirst()) {
        return {rf.filename()};
      }
    }
    return {};
  }

  namespace {

    std::vector<std::string> splitCommaList(const std::string& value) {
      std::vector<std::string> items;
      size_t start = 0;
      while (start <= value.size()) {
        const size_t comma = value.find(',', start);
        const size_t end = (comma == std::string::npos) ? value.size() : comma;
        const size_t first = value.find_first_not_of(" \t", start);
        if (first != std::string::npos && first < end) {
          const size_t last = value.find_last_not_of(" \t", end - 1);
          items.push_back(value.substr(first, last - first + 1));
        }
        if (comma == std::string::npos)
          break;
        start = comma + 1;
      }
      return items;
    }

    // Resolves a font's <name> attribute to a file: the skin's own
    // directory takes precedence, falling back to the shared data/fonts
    // directory so skins can reuse the bundled fonts without shipping them.
    std::optional<std::string> findSkinFontFile(const std::string& skinId, const std::string& name) {
      if (auto path = findFile("skins/" + skinId + "/" + name))
        return path;
      return findFile("fonts/" + name);
    }

    // Appends the font list for a <family> element: the entry whose `lang`
    // list contains the active language goes first, then the language-less
    // (default) entry, then any remaining entries so loadFont() still has
    // fallbacks to try if the preferred file can't be loaded.
    void collectFontFamily(const tinyxml2::XMLElement* family,
                            const std::string& lang,
                            const std::function<std::optional<std::string>(const std::string&)>& resolveFontFile,
                            std::vector<std::pair<std::string, size_t>>& output) {
      struct Entry {
        std::string name;
        size_t size;
        std::vector<std::string> langs;
      };
      std::vector<Entry> entries;

      for (const auto* font = family->FirstChildElement("font"); font;
           font = font->NextSiblingElement("font")) {
        const char* name = font->Attribute("name");
        if (!name)
          continue;
        // Note: tinyxml2's Attribute(name, value) two-arg overload does NOT
        // mean "get with default" -- it returns the value only if it equals
        // `value`, and null otherwise. So the default has to be applied by
        // hand using the single-arg overload.
        const char* size = font->Attribute("size");
        const char* lang = font->Attribute("lang");
        entries.push_back(Entry{
          name,
          static_cast<size_t>(strtoul(size ? size : "8", nullptr, 10)),
          splitCommaList(lang ? lang : "")
        });
      }

      const Entry* chosen = nullptr;
      const Entry* defaultEntry = nullptr;
      for (const auto& entry : entries) {
        if (!entry.langs.empty()) {
          if (std::ranges::find(entry.langs, lang) != entry.langs.end()) {
            chosen = &entry;
            break;
          }
        } else if (!defaultEntry) {
          defaultEntry = &entry;
        }
      }
      if (!chosen)
        chosen = defaultEntry;

      auto addEntry = [&](const Entry* entry) {
        if (!entry)
          return;
        if (auto path = resolveFontFile(entry->name))
          output.emplace_back(*path, entry->size);
      };

      addEntry(chosen);
      for (const auto& entry : entries) {
        if (&entry != chosen)
          addEntry(&entry);
      }
    }

  } // anonymous namespace

  void parseFontFamiliesFromSkinXml(
    const tinyxml2::XMLDocument& doc,
    const std::string& lang,
    const std::function<std::optional<std::string>(const std::string&)>& resolveFontFile,
    std::vector<std::pair<std::string, size_t>>& mainFonts,
    std::vector<std::pair<std::string, size_t>>& miniFonts)
  {
    const tinyxml2::XMLElement* fonts = tinyxml2::XMLHandle(const_cast<tinyxml2::XMLDocument*>(&doc))
                                        .FirstChildElement("skin")
                                        .FirstChildElement("fonts").ToElement();
    if (!fonts)
      return;

    for (const auto* family = fonts->FirstChildElement("family"); family;
         family = family->NextSiblingElement("family")) {
      const char* id = family->Attribute("id");
      if (!id)
        continue;
      if (std::strcmp(id, "main_font") == 0)
        collectFontFamily(family, lang, resolveFontFile, mainFonts);
      else if (std::strcmp(id, "mini_font") == 0)
        collectFontFamily(family, lang, resolveFontFile, miniFonts);
    }
  }

  void SkinTheme::loadFontFamiliesFromSkinXml(const std::string& skinId,
                                               std::vector<std::pair<std::string, size_t>>& mainFonts,
                                               std::vector<std::pair<std::string, size_t>>& miniFonts) const
  {
    const auto filename = findFile("skins/" + skinId + "/skin.xml");
    if (!filename)
      return;

    XmlDocumentRef doc;
    try {
      doc = open_xml(*filename);
    } catch (const base::Exception&) {
      return;
    }

    parseFontFamiliesFromSkinXml(
      *doc, getLanguage(),
      [&skinId](const std::string& name) { return findSkinFontFile(skinId, name); },
      mainFonts, miniFonts);
  }

  void SkinTheme::loadXml(const std::string& skinId)
  {
    TRACE("SkinTheme::loadXml(%s)\n", skinId.c_str());
    // Load the skin XML
    if (const auto filename = findFile("skins/" + skinId + "/skin.xml")) {
      loadFonts(skinId);
      loadSkinXml(*filename);
      return;
    }

    if (const auto filename = findFile("skins/" + skinId + "/theme.xml")) {
      loadThemeXml(*filename);
    }
  }

  void SkinTheme::loadThemeXml(const std::string& filename) {
    XmlDocumentRef doc = open_xml(filename);
    tinyxml2::XMLHandle handle(doc.get());

    // Load fonts
    {
      Preferences& pref = Preferences::instance();
      tinyxml2::XMLElement* xmlDim = handle
                                     .FirstChildElement("theme")
                                     .FirstChildElement("fonts")
                                     .FirstChildElement("font").ToElement();

      std::unordered_map<std::string, std::shared_ptr<she::Font>> known;
      if (!m_defaultFont)
        loadFonts(pref.theme.selected.defaultValue());
      if (m_defaultFont)
        known["Aseprite"] = m_defaultFont;
      if (m_miniFont)
        known["Aseprite Mini"] = m_miniFont;

      for (;xmlDim; xmlDim = xmlDim->NextSiblingElement()) {
        // Note: tinyxml2's Attribute(name, value) two-arg overload does NOT
        // mean "get with default" -- it returns the value only if it equals
        // `value`, and null otherwise. So the default has to be applied by
        // hand using the single-arg overload.
        const char* idAttr = xmlDim->Attribute("id");
        const char* fileAttr = xmlDim->Attribute("file");
        const char* nameAttr = xmlDim->Attribute("name");
        const char* fontAttr = xmlDim->Attribute("font");
        std::string id = idAttr ? idAttr : "";
        std::string file = fileAttr ? fileAttr : "";
        std::string name = nameAttr ? nameAttr : "";
        std::string fontattr = fontAttr ? fontAttr : "";
        std::string user;
        std::shared_ptr<she::Font> font{};
        if (auto it = known.find(fontattr); it != known.end()) {
          font = it->second;
        } else if (!file.empty()) {
          const char* sizeAttr = xmlDim->Attribute("size");
          uint32_t size = strtol(sizeAttr ? sizeAttr : "8", nullptr, 10);
          font = loadFont({std::make_pair(file, size)});
          if (!font) {
            std::cerr << "Could not load font " << file << std::endl;
          }
        }

        if (font) {
          if (!name.empty()) {
            known[name] = font;
          }
          if (id == "default") {
            m_defaultFont = font;
          } else if (id == "mini") {
            m_miniFont = font;
          }
        }
      }
    }

    // Load dimension
    {
      tinyxml2::XMLElement* xmlDim = handle
                                     .FirstChildElement("theme")
                                     .FirstChildElement("dimensions")
                                     .FirstChildElement("dim").ToElement();
      while (xmlDim) {
        std::string id = xmlDim->Attribute("id");
        uint32_t value = strtol(xmlDim->Attribute("value"), nullptr, 10);

        LOG("Loading dimension '%s'...\n", id.c_str());

        m_dimensions_by_id[id] = static_cast<int>(value);
        xmlDim = xmlDim->NextSiblingElement();
      }
    }

    // Load colors
    {
      m_colors_by_id.clear();

      tinyxml2::XMLElement* xmlColor = handle
                                       .FirstChildElement("theme")
                                       .FirstChildElement("colors")
                                       .FirstChildElement("color").ToElement();
      while (xmlColor) {
        std::string id = xmlColor->Attribute("id");
        uint32_t value = strtol(xmlColor->Attribute("value")+1, nullptr, 16);
        gfx::Color color = gfx::rgba(
          (value & 0xff0000) >> 16,
          (value & 0xff00) >> 8,
          (value & 0xff));

        LOG("Loading color '%s'...\n", id.c_str());

        m_colors_by_id[id] = color;
        xmlColor = xmlColor->NextSiblingElement();
      }
    }

    // Load cursors
    {
      tinyxml2::XMLElement* xmlPart = handle
                                      .FirstChildElement("theme")
                                      .FirstChildElement("parts")
                                      .FirstChildElement("part").ToElement();

      std::string cursor_prefix = "cursor_";
      std::string tool_prefix = "tool_";

      for (;xmlPart; xmlPart = xmlPart->NextSiblingElement()) {
        std::string id = xmlPart->Attribute("id");
        if (id.substr(0, cursor_prefix.size()) == cursor_prefix) {
          id = id.substr(cursor_prefix.size());

          int x = static_cast<int>(strtol(xmlPart->Attribute("x"), nullptr, 10));
          int y = static_cast<int>(strtol(xmlPart->Attribute("y"), nullptr, 10));
          int w = static_cast<int>(strtol(xmlPart->Attribute("w"), nullptr, 10));
          int h = static_cast<int>(strtol(xmlPart->Attribute("h"), nullptr, 10));
          int focusx = static_cast<int>(strtol(xmlPart->Attribute("focusx"), nullptr, 10));
          int focusy = static_cast<int>(strtol(xmlPart->Attribute("focusy"), nullptr, 10));
          int c;

          LOG("Loading cursor '%s'...\n", id.c_str());

          for (c=0; c<kCursorTypes; ++c) {
            if (id != cursor_names[c])
              continue;

            delete m_cursors[c];
            m_cursors[c] = nullptr;

            she::Surface* slice = sliceSheet(nullptr, Rect(x, y, w, h));

            m_cursors[c] = new Cursor(slice,
                                      Point(focusx*guiscale(), focusy*guiscale()));
            break;
          }

        } else if (id.substr(0, tool_prefix.size()) == tool_prefix) {
          id = id.substr(cursor_prefix.size());

          int x = static_cast<int>(strtol(xmlPart->Attribute("x"), nullptr, 10));
          int y = static_cast<int>(strtol(xmlPart->Attribute("y"), nullptr, 10));
          int w = static_cast<int>(strtol(xmlPart->Attribute("w"), nullptr, 10));
          int h = static_cast<int>(strtol(xmlPart->Attribute("h"), nullptr, 10));

          LOG("Loading tool icon '%s'...\n", id.c_str());

          // Crop the tool-icon from the sheet
          m_toolicon[id] = sliceSheet(m_toolicon[id], gfx::Rect(x, y, w, h));
        } else {
          int x = static_cast<int>(strtol(xmlPart->Attribute("x"), nullptr, 10));
          int y = static_cast<int>(strtol(xmlPart->Attribute("y"), nullptr, 10));
          int w = xmlPart->Attribute("w")
            ? static_cast<int>(strtol(xmlPart->Attribute("w"), nullptr, 10))
            : 0;
          int h = xmlPart->Attribute("h")
            ? static_cast<int>(strtol(xmlPart->Attribute("h"), nullptr, 10))
            : 0;

          LOG("Loading part '%s'...\n", id.c_str());

          SkinPartPtr part = m_parts_by_id[id];
          if (!part)
            part = m_parts_by_id[id] = SkinPartPtr(new SkinPart);

          if (w > 0 && h > 0) {
            part->setBitmap(0, sliceSheet(part->bitmap(0), Rect(x, y, w, h)));
          }
          else if (xmlPart->Attribute("w1")) { // 3x3-1 part (NW, N, NE, E, SE, S, SW, W)
            int w1 = static_cast<int>(strtol(xmlPart->Attribute("w1"), nullptr, 10));
            int w2 = static_cast<int>(strtol(xmlPart->Attribute("w2"), nullptr, 10));
            int w3 = static_cast<int>(strtol(xmlPart->Attribute("w3"), nullptr, 10));
            int h1 = static_cast<int>(strtol(xmlPart->Attribute("h1"), nullptr, 10));
            int h2 = static_cast<int>(strtol(xmlPart->Attribute("h2"), nullptr, 10));
            int h3 = static_cast<int>(strtol(xmlPart->Attribute("h3"), nullptr, 10));

            part->setBitmap(0, sliceSheet(part->bitmap(0), Rect(x, y, w1, h1))); // NW
            part->setBitmap(1, sliceSheet(part->bitmap(1), Rect(x+w1, y, w2, h1))); // N
            part->setBitmap(2, sliceSheet(part->bitmap(2), Rect(x+w1+w2, y, w3, h1))); // NE
            part->setBitmap(3, sliceSheet(part->bitmap(3), Rect(x+w1+w2, y+h1, w3, h2))); // E
            part->setBitmap(4, sliceSheet(part->bitmap(4), Rect(x+w1+w2, y+h1+h2, w3, h3))); // SE
            part->setBitmap(5, sliceSheet(part->bitmap(5), Rect(x+w1, y+h1+h2, w2, h3))); // S
            part->setBitmap(6, sliceSheet(part->bitmap(6), Rect(x, y+h1+h2, w1, h3))); // SW
            part->setBitmap(7, sliceSheet(part->bitmap(7), Rect(x, y+h1, w1, h2))); // W
          }
        }
      }
    }

    // Load styles
    {
      tinyxml2::XMLElement* xmlStyle = handle
                                       .FirstChildElement("skin")
                                       .FirstChildElement("styles")
                                       .FirstChildElement("style").ToElement();
      while (xmlStyle) {
        const char* style_id = xmlStyle->Attribute("id");
        const char* base_id = xmlStyle->Attribute("base");
        const css::Style* base = nullptr;
        if (!base_id)
          base_id = xmlStyle->Attribute("extends");
        if (base_id)
          base = m_stylesheet.getCssStyle(base_id);

        auto style = new css::Style(style_id, base);
        m_stylesheet.addCssStyle(style);

        tinyxml2::XMLElement* xmlRule = xmlStyle->FirstChildElement();
        while (xmlRule) {
          const std::string ruleName = xmlRule->Value();

          LOG("- Rule '%s' for '%s'\n", ruleName.c_str(), style_id);

          // TODO This code design to read styles could be improved.

          const char* part_id = xmlRule->Attribute("part");
          const char* color_id = xmlRule->Attribute("color");

          // Style align
          int align = 0;
          const char* halign = xmlRule->Attribute("align");
          const char* valign = xmlRule->Attribute("valign");
          const char* wordwrap = xmlRule->Attribute("wordwrap");
          if (halign) {
            if (strcmp(halign, "left") == 0) align |= LEFT;
            else if (strcmp(halign, "right") == 0) align |= RIGHT;
            else if (strcmp(halign, "center") == 0) align |= CENTER;
          }
          if (valign) {
            if (strcmp(valign, "top") == 0) align |= TOP;
            else if (strcmp(valign, "bottom") == 0) align |= BOTTOM;
            else if (strcmp(valign, "middle") == 0) align |= MIDDLE;
          }
          if (wordwrap && strcmp(wordwrap, "true") == 0)
            align |= WORDWRAP;

          if (ruleName == "background") {
            const char* repeat_id = xmlRule->Attribute("repeat");

            if (color_id) (*style)[StyleSheet::backgroundColorRule()] = value_or_none(color_id);
            if (part_id) (*style)[StyleSheet::backgroundPartRule()] = value_or_none(part_id);
            if (repeat_id) (*style)[StyleSheet::backgroundRepeatRule()] = value_or_none(repeat_id);
          }
          else if (ruleName == "icon") {
            if (align) (*style)[StyleSheet::iconAlignRule()] = css::Value(align);
            if (part_id) (*style)[StyleSheet::iconPartRule()] = css::Value(part_id);

            const char* x = xmlRule->Attribute("x");
            const char* y = xmlRule->Attribute("y");

            if (x) (*style)[StyleSheet::iconXRule()] = css::Value(strtol(x, nullptr, 10));
            if (y) (*style)[StyleSheet::iconYRule()] = css::Value(strtol(y, nullptr, 10));
          }
          else if (ruleName == "text") {
            if (color_id) (*style)[StyleSheet::textColorRule()] = css::Value(color_id);
            if (align) (*style)[StyleSheet::textAlignRule()] = css::Value(align);

            const char* l = xmlRule->Attribute("padding-left");
            const char* t = xmlRule->Attribute("padding-top");
            const char* r = xmlRule->Attribute("padding-right");
            const char* b = xmlRule->Attribute("padding-bottom");

            if (l) (*style)[StyleSheet::paddingLeftRule()] = css::Value(strtol(l, nullptr, 10));
            if (t) (*style)[StyleSheet::paddingTopRule()] = css::Value(strtol(t, nullptr, 10));
            if (r) (*style)[StyleSheet::paddingRightRule()] = css::Value(strtol(r, nullptr, 10));
            if (b) (*style)[StyleSheet::paddingBottomRule()] = css::Value(strtol(b, nullptr, 10));
          }

          xmlRule = xmlRule->NextSiblingElement();
        }

        xmlStyle = xmlStyle->NextSiblingElement();
      }
    }

    SkinFile<SkinTheme>::updateInternals();
  }

  void SkinTheme::loadSkinXml(const std::string& filename) {
    const XmlDocumentRef doc = open_xml(filename);
    tinyxml2::XMLHandle handle(doc.get());

    // Load dimension
    {
      tinyxml2::XMLElement* xmlDim = handle
                                     .FirstChildElement("skin")
                                     .FirstChildElement("dimensions")
                                     .FirstChildElement("dim").ToElement();
      while (xmlDim) {
        std::string id = xmlDim->Attribute("id");
        const uint32_t value = strtol(xmlDim->Attribute("value"), nullptr, 10);

        LOG("Loading dimension '%s'...\n", id.c_str());

        m_dimensions_by_id[id] = value;
        xmlDim = xmlDim->NextSiblingElement();
      }
    }

    // Load colors
    {
      tinyxml2::XMLElement* xmlColor = handle
                                       .FirstChildElement("skin")
                                       .FirstChildElement("colors")
                                       .FirstChildElement("color").ToElement();
      while (xmlColor) {
        std::string id = xmlColor->Attribute("id");
        const uint32_t value = strtol(xmlColor->Attribute("value")+1, nullptr, 16);
        const gfx::Color color = gfx::rgba(
          (value & 0xff0000) >> 16,
          (value & 0xff00) >> 8,
          value & 0xff);

        LOG("Loading color '%s'...\n", id.c_str());

        m_colors_by_id[id] = color;
        xmlColor = xmlColor->NextSiblingElement();
      }
    }

    // Load cursors
    {
      tinyxml2::XMLElement* xmlCursor = handle
                                        .FirstChildElement("skin")
                                        .FirstChildElement("cursors")
                                        .FirstChildElement("cursor").ToElement();
      while (xmlCursor) {
        std::string id = xmlCursor->Attribute("id");
        int x = strtol(xmlCursor->Attribute("x"), nullptr, 10);
        int y = strtol(xmlCursor->Attribute("y"), nullptr, 10);
        int w = strtol(xmlCursor->Attribute("w"), nullptr, 10);
        int h = strtol(xmlCursor->Attribute("h"), nullptr, 10);
        const int focusx = strtol(xmlCursor->Attribute("focusx"), nullptr, 10);
        const int focusy = strtol(xmlCursor->Attribute("focusy"), nullptr, 10);
        int c;

        LOG("Loading cursor '%s'...\n", id.c_str());

        for (c=0; c<kCursorTypes; ++c) {
          if (id != cursor_names[c])
            continue;

          delete m_cursors[c];
          m_cursors[c] = nullptr;

          she::Surface* slice = sliceSheet(nullptr, gfx::Rect(x, y, w, h));

          m_cursors[c] = new Cursor(slice,
                                    Point(focusx*guiscale(), focusy*guiscale()));
          break;
        }

        if (c == kCursorTypes) {
          throw base::Exception("Unknown cursor specified in '%s':\n"
                                "<cursor id='%s' ... />\n", filename.c_str(), id.c_str());
        }

        xmlCursor = xmlCursor->NextSiblingElement();
      }
    }

    // Load tool icons
    {
      tinyxml2::XMLElement* xmlIcon = handle
                                      .FirstChildElement("skin")
                                      .FirstChildElement("tools")
                                      .FirstChildElement("tool").ToElement();
      while (xmlIcon) {
        // Get the tool-icon rectangle
        const char* id = xmlIcon->Attribute("id");
        int x = strtol(xmlIcon->Attribute("x"), nullptr, 10);
        int y = strtol(xmlIcon->Attribute("y"), nullptr, 10);
        int w = strtol(xmlIcon->Attribute("w"), nullptr, 10);
        int h = strtol(xmlIcon->Attribute("h"), nullptr, 10);

        LOG("Loading tool icon '%s'...\n", id);

        // Crop the tool-icon from the sheet
        m_toolicon[id] = sliceSheet(m_toolicon[id], Rect(x, y, w, h));

        xmlIcon = xmlIcon->NextSiblingElement();
      }
    }

    // Load parts
    {
      tinyxml2::XMLElement* xmlPart = handle
                                      .FirstChildElement("skin")
                                      .FirstChildElement("parts")
                                      .FirstChildElement("part").ToElement();
      while (xmlPart) {
        // Get the tool-icon rectangle
        const char* part_id = xmlPart->Attribute("id");
        int x = strtol(xmlPart->Attribute("x"), nullptr, 10);
        int y = strtol(xmlPart->Attribute("y"), nullptr, 10);
        int w = xmlPart->Attribute("w") ? strtol(xmlPart->Attribute("w"), nullptr, 10): 0;
        int h = xmlPart->Attribute("h") ? strtol(xmlPart->Attribute("h"), nullptr, 10): 0;

        LOG("Loading part '%s'...\n", part_id);

        SkinPartPtr part = m_parts_by_id[part_id];
        if (!part)
          part = m_parts_by_id[part_id] = SkinPartPtr(new SkinPart);

        if (w > 0 && h > 0) {
          part->setBitmap(0,
                          sliceSheet(part->bitmap(0), Rect(x, y, w, h)));
        }
        else if (xmlPart->Attribute("w1")) { // 3x3-1 part (NW, N, NE, E, SE, S, SW, W)
          int w1 = strtol(xmlPart->Attribute("w1"), nullptr, 10);
          int w2 = strtol(xmlPart->Attribute("w2"), nullptr, 10);
          int w3 = strtol(xmlPart->Attribute("w3"), nullptr, 10);
          int h1 = strtol(xmlPart->Attribute("h1"), nullptr, 10);
          int h2 = strtol(xmlPart->Attribute("h2"), nullptr, 10);
          int h3 = strtol(xmlPart->Attribute("h3"), nullptr, 10);

          part->setBitmap(0, sliceSheet(part->bitmap(0), Rect(x, y, w1, h1))); // NW
          part->setBitmap(1, sliceSheet(part->bitmap(1), Rect(x+w1, y, w2, h1))); // N
          part->setBitmap(2, sliceSheet(part->bitmap(2), Rect(x+w1+w2, y, w3, h1))); // NE
          part->setBitmap(3, sliceSheet(part->bitmap(3), Rect(x+w1+w2, y+h1, w3, h2))); // E
          part->setBitmap(4, sliceSheet(part->bitmap(4), Rect(x+w1+w2, y+h1+h2, w3, h3))); // SE
          part->setBitmap(5, sliceSheet(part->bitmap(5), Rect(x+w1, y+h1+h2, w2, h3))); // S
          part->setBitmap(6, sliceSheet(part->bitmap(6), Rect(x, y+h1+h2, w1, h3))); // SW
          part->setBitmap(7, sliceSheet(part->bitmap(7), Rect(x, y+h1, w1, h2))); // W
        }

        xmlPart = xmlPart->NextSiblingElement();
      }
    }

    // Load styles
    {
      tinyxml2::XMLElement* xmlStyle = handle
                                       .FirstChildElement("skin")
                                       .FirstChildElement("stylesheet")
                                       .FirstChildElement("style").ToElement();
      while (xmlStyle) {
        const char* style_id = xmlStyle->Attribute("id");
        const char* base_id = xmlStyle->Attribute("base");
        const css::Style* base = nullptr;

        if (base_id)
          base = m_stylesheet.getCssStyle(base_id);

        const auto style = new css::Style(style_id, base);
        m_stylesheet.addCssStyle(style);

        tinyxml2::XMLElement* xmlRule = xmlStyle->FirstChildElement();
        while (xmlRule) {
          const std::string ruleName = xmlRule->Value();

          LOG("- Rule '%s' for '%s'\n", ruleName.c_str(), style_id);

          // TODO This code design to read styles could be improved.

          const char* part_id = xmlRule->Attribute("part");
          const char* color_id = xmlRule->Attribute("color");

          // Style align
          int align = 0;
          const char* halign = xmlRule->Attribute("align");
          const char* valign = xmlRule->Attribute("valign");
          const char* wordwrap = xmlRule->Attribute("wordwrap");
          if (halign) {
            if (strcmp(halign, "left") == 0) align |= LEFT;
            else if (strcmp(halign, "right") == 0) align |= RIGHT;
            else if (strcmp(halign, "center") == 0) align |= CENTER;
          }
          if (valign) {
            if (strcmp(valign, "top") == 0) align |= TOP;
            else if (strcmp(valign, "bottom") == 0) align |= BOTTOM;
            else if (strcmp(valign, "middle") == 0) align |= MIDDLE;
          }
          if (wordwrap && strcmp(wordwrap, "true") == 0)
            align |= WORDWRAP;

          if (ruleName == "background") {
            const char* repeat_id = xmlRule->Attribute("repeat");

            if (color_id) (*style)[StyleSheet::backgroundColorRule()] = value_or_none(color_id);
            if (part_id) (*style)[StyleSheet::backgroundPartRule()] = value_or_none(part_id);
            if (repeat_id) (*style)[StyleSheet::backgroundRepeatRule()] = value_or_none(repeat_id);
          }
          else if (ruleName == "icon") {
            if (align) (*style)[StyleSheet::iconAlignRule()] = css::Value(align);
            if (part_id) (*style)[StyleSheet::iconPartRule()] = css::Value(part_id);

            const char* x = xmlRule->Attribute("x");
            const char* y = xmlRule->Attribute("y");

            if (x) (*style)[StyleSheet::iconXRule()] = css::Value(strtol(x, nullptr, 10));
            if (y) (*style)[StyleSheet::iconYRule()] = css::Value(strtol(y, nullptr, 10));
          }
          else if (ruleName == "text") {
            if (color_id) (*style)[StyleSheet::textColorRule()] = css::Value(color_id);
            if (align) (*style)[StyleSheet::textAlignRule()] = css::Value(align);

            const char* l = xmlRule->Attribute("padding-left");
            const char* t = xmlRule->Attribute("padding-top");
            const char* r = xmlRule->Attribute("padding-right");
            const char* b = xmlRule->Attribute("padding-bottom");

            if (l) (*style)[StyleSheet::paddingLeftRule()] = css::Value(strtol(l, nullptr, 10));
            if (t) (*style)[StyleSheet::paddingTopRule()] = css::Value(strtol(t, nullptr, 10));
            if (r) (*style)[StyleSheet::paddingRightRule()] = css::Value(strtol(r, nullptr, 10));
            if (b) (*style)[StyleSheet::paddingBottomRule()] = css::Value(strtol(b, nullptr, 10));
          }

          xmlRule = xmlRule->NextSiblingElement();
        }

        xmlStyle = xmlStyle->NextSiblingElement();
      }
    }

    SkinFile<SkinTheme>::updateInternals();
  }

  she::Surface* SkinTheme::sliceSheet(she::Surface* sur, const Rect& bounds) const
  {
    if (sur && (sur->width() != bounds.w ||
      sur->height() != bounds.h)) {
      sur->dispose();
      sur = nullptr;
    }

    if (!sur)
      sur = she::instance()->createRgbaSurface(bounds.w, bounds.h);

    {
      she::SurfaceLock lockSrc(m_sheet);
      she::SurfaceLock lockDst(sur);
      m_sheet->blitTo(sur, bounds.x, bounds.y, 0, 0, bounds.w, bounds.h);
    }

    sur->applyScale(guiscale());
    return sur;
  }

  std::shared_ptr<she::Font> SkinTheme::getWidgetFont(const Widget* widget) const
  {
    if (const SkinPropertyPtr skinProperty = widget->getProperty(SkinProperty::Name);
        skinProperty && skinProperty->hasMiniFont())
      return getMiniFont();

    return getDefaultFont();
  }

  Cursor* SkinTheme::getCursor(const CursorType type)
  {
    if (type == kNoCursor)
      return nullptr;

    ASSERT(type >= kFirstCursorType && type <= kLastCursorType);
    return m_cursors[type];
  }

  void SkinTheme::initWidget(Widget* widget)
  {
#define BORDER(n)                               \
  widget->setBorder(gfx::Border(n))

#define BORDER4(L,T,R,B)                                \
  widget->setBorder(gfx::Border((L), (T), (R), (B)))

    const int scale = guiscale();
    switch (widget->type()) {
      case kBoxWidget:
        BORDER(0);
        widget->setChildSpacing(4 * scale);
        break;
      case kButtonWidget:
        BORDER4(
          parts.buttonNormal()->bitmapW()->width(),
          parts.buttonNormal()->bitmapN()->height(),
          parts.buttonNormal()->bitmapE()->width(),
          parts.buttonNormal()->bitmapS()->height());
        widget->setChildSpacing(0);
        break;
      case kCheckWidget:
        BORDER(2 * scale);
        widget->setChildSpacing(4 * scale);

        static_cast<ButtonBase*>(widget)->setIconInterface
        (new ButtonIconImpl(parts.checkNormal(),
                            parts.checkSelected(),
                            parts.checkDisabled(),
                            LEFT | MIDDLE));
        break;
      case kEntryWidget:
        BORDER4(
          parts.sunkenNormal()->bitmapW()->width(),
          parts.sunkenNormal()->bitmapN()->height(),
          parts.sunkenNormal()->bitmapE()->width(),
          parts.sunkenNormal()->bitmapS()->height());
        widget->setChildSpacing(3 * scale);
        break;
      case kGridWidget:
        BORDER(0);
        widget->setChildSpacing(4 * scale);
        break;
      case kLabelWidget:
        BORDER(1 * scale);
        break;
      case kListBoxWidget:
        BORDER(0);
        widget->setChildSpacing(0);
        break;
      case kListItemWidget:
        BORDER(1 * scale);
        break;
      case kComboBoxWidget:
        {
          const auto combobox = dynamic_cast<ComboBox*>(widget);
          ASSERT(combobox != nullptr);

          Button* button = combobox->getButtonWidget();

          button->setBorder(Border(0));
          button->setChildSpacing(0);
          button->setMinSize(Size(15 * guiscale(),
                                       16 * guiscale()));

          button->setIconInterface
          (new ButtonIconImpl(parts.comboboxArrowDown(),
                              parts.comboboxArrowDownSelected(),
                              parts.comboboxArrowDownDisabled(),
                              CENTER | MIDDLE));
        }
        break;
      case kMenuWidget:
      case kMenuBarWidget:
      case kMenuBoxWidget:
        BORDER(0);
        widget->setChildSpacing(0);
        break;
      case kMenuItemWidget:
        BORDER(2 * scale);
        widget->setChildSpacing(18 * scale);
        break;
      case kSplitterWidget:
        BORDER(0);
        widget->setChildSpacing(3 * scale);
        break;
      case kRadioWidget:
        BORDER(2 * scale);
        widget->setChildSpacing(4 * scale);

        static_cast<ButtonBase*>(widget)->setIconInterface
        (new ButtonIconImpl(parts.radioNormal(),
                            parts.radioSelected(),
                            parts.radioDisabled(),
                            LEFT | MIDDLE));
        break;
      case kSeparatorWidget:
        // Frame
        if (widget->align() & HORIZONTAL &&
            widget->align() & VERTICAL) {
          BORDER(4 * scale);
        }
        // Horizontal bar
        else if (widget->align() & HORIZONTAL) {
          BORDER4(2 * scale, 4 * scale, 2 * scale, 0);
        }
        // Vertical bar
        else {
          BORDER4(4 * scale, 2 * scale, 1 * scale, 2 * scale);
        }
        break;
      case kSliderWidget:
        BORDER4(
          parts.sliderEmpty()->bitmapW()->width()-1*scale,
          parts.sliderEmpty()->bitmapN()->height(),
          parts.sliderEmpty()->bitmapE()->width()-1*scale,
          parts.sliderEmpty()->bitmapS()->height()-1*scale);
        widget->setChildSpacing(widget->textHeight());
        widget->setAlign(CENTER | MIDDLE);
        break;
      case kTextBoxWidget:
        BORDER(0);
        widget->setChildSpacing(0);
        break;
      case kViewWidget:
        BORDER4(
          parts.sunkenNormal()->bitmapW()->width()-1*scale,
          parts.sunkenNormal()->bitmapN()->height(),
          parts.sunkenNormal()->bitmapE()->width()-1*scale,
          parts.sunkenNormal()->bitmapS()->height()-1*scale);
        widget->setChildSpacing(0);
        widget->setBgColor(colors.windowFace());
        break;
      case kViewScrollbarWidget:
        BORDER(1 * scale);
        widget->setChildSpacing(0);
        break;
      case kViewViewportWidget:
        BORDER(0);
        widget->setChildSpacing(0);
        break;
      case kWindowWidget:
        if (!dynamic_cast<Window*>(widget)->isDesktop()) {
          if (widget->hasText()) {
            BORDER4(6 * scale,
                    (4+6) * scale + widget->textHeight(),
                    6 * scale,
                    6 * scale);

            if (!widget->hasFlags(INITIALIZED)) {
              Button* button = new WindowCloseButton();
              widget->addChild(button);
            }
          }
          else {
            BORDER(3 * scale);
          }
        }
        else {
          BORDER(0);
        }

        widget->setChildSpacing(4 * scale); // TODO this hard-coded 4 should be configurable in skin.xml

        // Tooltip background color
        if (dynamic_cast<TipWindow*>(widget))
          widget->setBgColor(instance()->colors.tooltipFace());
        else
          widget->setBgColor(colors.windowFace());
        break;
      default:
        break;
    }
  }

  void SkinTheme::getWindowMask(Widget* widget, Region& region)
  {
    region = widget->bounds();
  }

  void SkinTheme::setDecorativeWidgetBounds(Widget* widget)
  {
    if (widget->id() == kThemeCloseButtonId) {
      const Widget* window = widget->parent();
      Rect rect(parts.windowCloseButtonNormal()->size());

      rect.offset(window->bounds().x2() - 3*guiscale() - rect.w,
                  window->bounds().y + 3*guiscale());

      widget->setBounds(rect);
    }
  }

  int SkinTheme::getScrollbarSize()
  {
    return dimensions.scrollbarSize();
  }

  void SkinTheme::paintDesktop(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();

    g->fillRect(colors.disabled(), g->getClipBounds());
  }

  void SkinTheme::paintBox(PaintEvent& ev)
  {
    const Widget* widget = ev.getSource();
    Graphics* g = ev.graphics();

    if (!widget->isTransparent() &&
      !is_transparent(BGCOLOR)) {
      g->fillRect(BGCOLOR, g->getClipBounds());
    }
  }

  void SkinTheme::paintButton(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    auto* widget = dynamic_cast<ButtonBase*>(ev.getSource());
    IButtonIcon* iconInterface = widget->iconInterface();
    Rect box, text, icon;
    gfx::Color fg, bg;
    SkinPartPtr part_nw;

    widget->getTextIconInfo(&box, &text, &icon,
                            iconInterface ? iconInterface->iconAlign(): 0,
                            iconInterface ? iconInterface->size().w: 0,
                            iconInterface ? iconInterface->size().h: 0);

    // Tool buttons are smaller
    LookType look = NormalLook;
    if (const SkinPropertyPtr skinProperty = widget->getProperty(SkinProperty::Name))
      look = skinProperty->getLook();

    // Selected
    auto pickByLook = [&](const SkinPartPtr& mini,
                          const SkinPartPtr& left,
                          const SkinPartPtr& right,
                          const SkinPartPtr& normal) -> SkinPartPtr {
      if (look == MiniLook) return mini;
      if (look == LeftButtonLook) return left;
      if (look == RightButtonLook) return right;
      return normal;
    };

    if (widget->isSelected())
    {
      fg = colors.buttonSelectedText();
      bg = colors.buttonSelectedFace();
      part_nw = pickByLook(
        parts.toolbuttonNormal(),
        parts.dropDownButtonLeftSelected(),
        parts.dropDownButtonRightSelected(),
        parts.buttonSelected());
    }
    // With mouse
    else if (widget->isEnabled() && widget->hasMouseOver())
    {
      fg = colors.buttonHotText();
      bg = colors.buttonHotFace();
      part_nw = pickByLook(
        parts.toolbuttonHot(),
        parts.dropDownButtonLeftHot(),
        parts.dropDownButtonRightHot(),
        parts.buttonHot());
    }
    // Without mouse
    else
    {
      fg = colors.buttonNormalText();
      bg = colors.buttonNormalFace();

      if (widget->hasFocus())
        part_nw = pickByLook(
          parts.toolbuttonHot(),
          parts.dropDownButtonLeftFocused(),
          parts.dropDownButtonRightFocused(),
          parts.buttonFocused());
      else
        part_nw = pickByLook(
          parts.toolbuttonNormal(),
          parts.dropDownButtonLeftNormal(),
          parts.dropDownButtonRightNormal(),
          parts.buttonNormal());
    }

    // external background
    g->fillRect(BGCOLOR, g->getClipBounds());

    // draw borders
    if (part_nw)
      drawRect(g, widget->clientBounds(), part_nw.get(), bg);

    // text
    drawTextString(g, nullptr, fg, ColorNone, widget,
                   widget->clientChildrenBounds(), get_button_selected_offset());

    // Paint the icon
    if (iconInterface) {
      if (widget->isSelected())
        icon.offset(get_button_selected_offset(),
                    get_button_selected_offset());

      paintIcon(widget, ev.graphics(), iconInterface, icon.x, icon.y);
    }
  }

  void SkinTheme::paintCheckBox(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto widget = static_cast<ButtonBase*>(ev.getSource());
    const Rect bounds = widget->clientBounds();
    IButtonIcon* iconInterface = widget->iconInterface();
    Rect box, text, icon;
    gfx::Color bg;

    widget->getTextIconInfo(&box, &text, &icon,
                            iconInterface ? iconInterface->iconAlign(): 0,
                            iconInterface ? iconInterface->size().w: 0,
                            iconInterface ? iconInterface->size().h: 0);

    // Check box look
    LookType look = NormalLook;
    if (const SkinPropertyPtr skinProperty = widget->getProperty(SkinProperty::Name))
      look = skinProperty->getLook();

    // Background
    g->fillRect(bg = BGCOLOR, bounds);

    // Mouse
    if (widget->isEnabled()) {
      if (widget->hasMouseOver())
        g->fillRect(bg = colors.checkHotFace(), bounds);
      else if (widget->hasFocus())
        g->fillRect(bg = colors.checkFocusFace(), bounds);
    }

    // Text
    drawTextString(g, nullptr, ColorNone, ColorNone, widget, text, 0);

    // Paint the icon
    if (iconInterface)
      paintIcon(widget, g, iconInterface, icon.x, icon.y);

    // draw focus
    if (look != WithoutBordersLook && widget->hasFocus())
      drawRect(g, bounds, parts.checkFocus().get(), ColorNone);
  }

  void SkinTheme::paintGrid(PaintEvent& ev)
  {
    const Widget* widget = ev.getSource();
    Graphics* g = ev.graphics();

    if (!is_transparent(BGCOLOR))
      g->fillRect(BGCOLOR, g->getClipBounds());
  }

  void SkinTheme::paintEntry(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto widget = static_cast<Entry*>(ev.getSource());
    Rect bounds = widget->clientBounds();
    const bool password = widget->isPassword();
    int scroll, caret, state, selbeg, selend;
    const std::string& textString = widget->text();
    int c;

    widget->getEntryThemeInfo(&scroll, &caret, &state, &selbeg, &selend);

    // Outside borders
    g->fillRect(BGCOLOR, bounds);

    bool isMiniLook = false;
    if (const SkinPropertyPtr skinProperty = widget->getProperty(SkinProperty::Name))
      isMiniLook = skinProperty->getLook() == MiniLook;

    gfx::Color bg = colors.background();
    const SkinPart* part_nw = widget->hasFocus()
      ? (isMiniLook
         ? parts.sunkenMiniFocused().get()
         : parts.sunkenFocused().get())
      : (isMiniLook
         ? parts.sunkenMiniNormal().get()
         : parts.sunkenNormal().get());
    drawRect(g, bounds, part_nw, bg);

    // Draw the text
    bounds = widget->getEntryTextBounds();
    int x = bounds.x;
    int y = bounds.y;

    auto utf8_it = base::utf8_const_iterator(textString.begin());
    const int textlen = base::utf8_length(textString);
    if (scroll < textlen)
      utf8_it += scroll;

    if (const auto font = widget->font())
      g->setFont(font);

    for (c=scroll; c<textlen; ++c, ++utf8_it) {
      int ch = password ? '*' : *utf8_it;

      // Normal text
      bg = ColorNone;
      gfx::Color fg = colors.text();

      // Selected
      if (c >= selbeg && c <= selend) {
        if (widget->hasFocus())
          bg = colors.selected();
        else
          bg = colors.disabled();
        fg = colors.background();
      }

      // Disabled
      if (!widget->isEnabled()) {
        bg = ColorNone;
        fg = colors.disabled();
      }

      const int w = g->measureChar(ch).w;
      if (x + w > bounds.x2())
        return;

      const int caret_x = x;
      g->drawChar(ch, fg, bg, x, y);
      x += w;

      // Caret
      if (c == caret && state && widget->hasFocus())
        drawEntryCaret(g, widget, caret_x, y);
    }

    // Draw suffix if there is enough space
    if (!widget->getSuffix().empty()) {
      const Rect sufBounds(x, y,
                     bounds.x2()-widget->childSpacing()*guiscale()-x,
                     widget->textHeight());
      if (const IntersectClip clip(g, sufBounds); clip) {
        drawTextString(
          g, widget->getSuffix().c_str(),
          colors.entrySuffix(), ColorNone,
          widget, sufBounds, 0);
      }
    }

    // Draw the caret if it is next of the last character
    if (c == caret && state &&
      widget->hasFocus() &&
      widget->isEnabled()) {
      drawEntryCaret(g, widget, x, y);
    }
  }

  void SkinTheme::paintLabel(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto widget = static_cast<Label*>(ev.getSource());
    Style* style = styles.label();
    const gfx::Color bg = BGCOLOR;
    Rect text, rc = widget->clientBounds();

    if (const SkinStylePropertyPtr styleProp = widget->getProperty(SkinStyleProperty::Name))
      style = styleProp->getStyle();

    if (!is_transparent(bg))
      g->fillRect(bg, rc);

    rc.shrink(widget->border());

    widget->getTextIconInfo(nullptr, &text);
    style->paint(g, text, widget->text().c_str(), Style::State());
  }

  void SkinTheme::paintLinkLabel(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    Widget* widget = ev.getSource();
    Style* style = styles.link();
    const Rect bounds = widget->clientBounds();
    const gfx::Color bg = BGCOLOR;

    if (const SkinStylePropertyPtr styleProp = widget->getProperty(SkinStyleProperty::Name))
      style = styleProp->getStyle();

    Style::State state;
    if (widget->hasMouseOver()) state += Style::hover();
    if (widget->isSelected()) state += Style::clicked();

    g->fillRect(bg, bounds);
    style->paint(g, bounds, widget->text().c_str(), state);
  }

  void SkinTheme::paintListBox(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();

    g->fillRect(colors.background(), g->getClipBounds());
  }

  void SkinTheme::paintListItem(PaintEvent& ev)
  {
    const Widget* widget = ev.getSource();
    Rect bounds = widget->clientBounds();
    Graphics* g = ev.graphics();
    gfx::Color fg, bg;

    if (!widget->isEnabled()) {
      bg = colors.face();
      fg = colors.disabled();
    }
    else if (widget->isSelected()) {
      fg = colors.listitemSelectedText();
      bg = colors.listitemSelectedFace();
    }
    else {
      fg = colors.listitemNormalText();
      bg = colors.listitemNormalFace();
    }

    g->fillRect(bg, bounds);

    if (widget->hasText()) {
      bounds.shrink(widget->border());
      drawTextString(g, nullptr, fg, bg, widget, bounds, 0);
    }
  }

  void SkinTheme::paintMenu(PaintEvent& ev)
  {
    const Widget* widget = ev.getSource();
    Graphics* g = ev.graphics();

    g->fillRect(BGCOLOR, g->getClipBounds());
  }

  void SkinTheme::paintMenuItem(PaintEvent& ev)
  {
    const int scale = guiscale();
    Graphics* g = ev.graphics();
    const auto widget = static_cast<MenuItem*>(ev.getSource());
    const Rect bounds = widget->clientBounds();
    gfx::Color fg, bg;

    // TODO ASSERT?
    if (!widget->parent()->parent())
      return;

    const int bar = widget->parent()->parent()->type() == kMenuBarWidget;

    // Colors
    if (!widget->isEnabled()) {
      fg = ColorNone;
      bg = colors.menuitemNormalFace();
    }
    else {
      if (widget->isHighlighted()) {
        fg = colors.menuitemHighlightText();
        bg = colors.menuitemHighlightFace();
      }
      else if (widget->hasMouse()) {
        fg = colors.menuitemHotText();
        bg = colors.menuitemHotFace();
      }
      else {
        fg = colors.menuitemNormalText();
        bg = colors.menuitemNormalFace();
      }
    }

    // Background
    g->fillRect(bg, bounds);

    // Draw an indicator for selected items
    if (widget->isSelected()) {
      she::Surface* icon =
      widget->isEnabled() ?
        parts.checkSelected()->bitmap(0):
        parts.checkDisabled()->bitmap(0);

      const int x = 4 * scale - icon->width() / 2 + bounds.x;
      const int y = bounds.h / 2 - icon->height() / 2 + bounds.y;
      g->drawRgbaSurface(icon, x, y);
    }

    // Text
    if (bar)
      widget->setAlign(CENTER | MIDDLE);
    else
      widget->setAlign(LEFT | MIDDLE);

    Rect pos = bounds;
    if (!bar)
      pos.offset(widget->childSpacing()/2, 0);
    drawTextString(g, nullptr, fg, ColorNone, widget, pos, 0);

    // For menu-box
    if (!bar) {
      // Draw the arrown (to indicate which this menu has a sub-menu)
      if (widget->getSubmenu())
      {
        int c;
        // Enabled
        if (widget->isEnabled()) {
          for (c = 0; c < 3 * scale; c++)
            g->drawVLine(fg,
                         bounds.x2() - 3 * scale - c,
                         bounds.y + bounds.h / 2 - c, 2 * c + 1);
        }
        // Disabled
        else {
          for (c = 0; c < 3 * scale; c++)
            g->drawVLine(colors.background(),
                         bounds.x2() - 3 * scale - c + 1,
                         bounds.y + bounds.h / 2 - c + 1, 2 * c + 1);

          for (c = 0; c < 3 * scale; c++)
            g->drawVLine(colors.disabled(),
                         bounds.x2() - 3 * scale - c,
                         bounds.y + bounds.h / 2 - c, 2 * c + 1);
        }
      }
      // Draw the keyboard shortcut
      else if (const auto appMenuItem = dynamic_cast<AppMenuItem*>(widget)) {
        if (appMenuItem->key() && !appMenuItem->key()->accels().empty()) {
          const int old_align = appMenuItem->align();

          pos = bounds;
          pos.w -= widget->childSpacing() / 4;

          const std::string buf = appMenuItem->key()->accels().front().toString();

          widget->setAlign(RIGHT | MIDDLE);
          drawTextString(g, buf.c_str(), fg, ColorNone, widget, pos, 0);
          widget->setAlign(old_align);
        }
      }
    }
  }

  void SkinTheme::paintSplitter(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    g->fillRect(colors.splitterNormalFace(), g->getClipBounds());
  }

  void SkinTheme::paintRadioButton(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto widget = static_cast<ButtonBase*>(ev.getSource());
    const Rect bounds = widget->clientBounds();
    IButtonIcon* iconInterface = widget->iconInterface();
    gfx::Color bg = BGCOLOR;

    Rect box, text, icon;
    widget->getTextIconInfo(&box, &text, &icon,
                            iconInterface ? iconInterface->iconAlign(): 0,
                            iconInterface ? iconInterface->size().w: 0,
                            iconInterface ? iconInterface->size().h: 0);

    // Background
    g->fillRect(bg, g->getClipBounds());

    // Mouse
    if (widget->isEnabled()) {
      if (widget->hasMouseOver())
        g->fillRect(bg = colors.radioHotFace(), bounds);
      else if (widget->hasFocus())
        g->fillRect(bg = colors.radioFocusFace(), bounds);
    }

    // Text
    drawTextString(g, nullptr, ColorNone, ColorNone, widget, text, 0);

    // Icon
    if (iconInterface)
      paintIcon(widget, g, iconInterface, icon.x, icon.y);

    // Focus
    if (widget->hasFocus())
      drawRect(g, bounds, parts.radioFocus().get(), ColorNone);
  }

  void SkinTheme::paintSeparator(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const Widget* widget = ev.getSource();
    const Rect bounds = widget->clientBounds();

    // background
    g->fillRect(BGCOLOR, bounds);

    if (widget->align() & HORIZONTAL) {
      int h = parts.separatorHorz()->bitmap(0)->height();
      drawHline(g, gfx::Rect(bounds.x, bounds.y + bounds.h / 2 - h / 2,
                             bounds.w, h),
                parts.separatorHorz().get());
    }

    if (widget->align() & VERTICAL) {
      int w = parts.separatorVert()->bitmap(0)->width();
      drawVline(g, gfx::Rect(bounds.x + bounds.w / 2 - w / 2, bounds.y,
                             w, bounds.h),
                parts.separatorVert().get());
    }

    // text
    if (widget->hasText()) {
      const int h = widget->textHeight();
      const Rect r(
        bounds.x + widget->border().left()/2 + h/2,
        bounds.y + bounds.h/2 - h/2,
        widget->textWidth(), h);

      drawTextString(g, nullptr,
                     colors.separatorLabel(), BGCOLOR,
                     widget, r, 0);
    }
  }

  void SkinTheme::paintSlider(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto widget = static_cast<Slider*>(ev.getSource());
    Rect bounds = widget->clientBounds();
    int min, max, value;

    // Outside borders
    if (const gfx::Color bgcolor = widget->bgColor(); !is_transparent(bgcolor))
      g->fillRect(bgcolor, bounds);

    widget->getSliderThemeInfo(&min, &max, &value);

    Rect rc(Rect(bounds).shrink(widget->border()));
    int x;
    if (min != max)
      x = rc.x + rc.w * (value-min) / (max-min);
    else
      x = rc.x;

    rc = widget->clientBounds();

    // The mini-look is used for sliders with tiny borders.
    bool isMiniLook = false;

    // The BG painter is used for sliders without a number-indicator and
    // customized background (e.g. RGB sliders)
    ISliderBgPainter* bgPainter = nullptr;

    if (const SkinPropertyPtr skinProperty = widget->getProperty(SkinProperty::Name))
      isMiniLook = skinProperty->getLook() == MiniLook;

    if (const SkinSliderPropertyPtr skinSliderProperty = widget->getProperty(SkinSliderProperty::Name))
      bgPainter = skinSliderProperty->getBgPainter();

    // Draw customized background
    if (bgPainter) {
      const SkinPartPtr nw = parts.miniSliderEmpty();
      she::Surface* thumb =
      widget->hasFocus() ? parts.miniSliderThumbFocused()->bitmap(0):
        parts.miniSliderThumb()->bitmap(0);

      // Draw background
      g->fillRect(BGCOLOR, rc);

      // Draw thumb
      g->drawRgbaSurface(thumb, x-thumb->width()/2, rc.y);

      // Draw borders
      rc.shrink(Border(
        3 * guiscale(),
        thumb->height(),
        3 * guiscale(),
        1 * guiscale()));

      drawRect(g, rc, nw.get(), ColorNone);

      // Draw background (using the customized ISliderBgPainter implementation)
      rc.shrink(Border(1, 1, 1, 2) * guiscale());
      if (!rc.isEmpty())
        bgPainter->paint(widget, g, rc);
    }
    else {
      // Draw borders
      SkinPartPtr full_part;
      SkinPartPtr empty_part;

      if (isMiniLook) {
        full_part = widget->hasMouseOver() ? parts.miniSliderFullFocused():
                      parts.miniSliderFull();
        empty_part = widget->hasMouseOver() ? parts.miniSliderEmptyFocused():
                       parts.miniSliderEmpty();
      }
      else {
        full_part = widget->hasFocus() ? parts.sliderFullFocused():
                      parts.sliderFull();
        empty_part = widget->hasFocus() ? parts.sliderEmptyFocused():
                       parts.sliderEmpty();
      }

      if (value == min)
        drawRect(g, rc, empty_part.get(), colors.sliderEmptyFace());
      else if (value == max)
        drawRect(g, rc, full_part.get(), colors.sliderFullFace());
      else
        drawRect2(g, rc, x,
                  full_part.get(), empty_part.get(),
                  colors.sliderFullFace(),
                  colors.sliderEmptyFace());

      // Draw text
      const std::string old_text = widget->text();
      widget->setTextQuiet(widget->convertValueToText(value));

      {
        if (const IntersectClip clip(g, Rect(rc.x, rc.y, x - rc.x, rc.h)); clip) {
          drawTextString(g, nullptr,
                         colors.sliderFullText(), ColorNone,
                         widget, rc, 0);
        }
      }

      {
        if (const IntersectClip clip(g, Rect(x + 1, rc.y, rc.w - (x - rc.x + 1), rc.h)); clip) {
          drawTextString(g, nullptr,
                         colors.sliderEmptyText(),
                         ColorNone, widget, rc, 0);
        }
      }

      widget->setTextQuiet(old_text.c_str());
    }
  }

  void SkinTheme::paintComboBoxEntry(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto widget = static_cast<Entry*>(ev.getSource());
    const Rect bounds = widget->clientBounds();
    const bool password = widget->isPassword();
    int scroll, caret, state, selbeg, selend;
    const std::string& textString = widget->text();
    int c;

    widget->getEntryThemeInfo(&scroll, &caret, &state, &selbeg, &selend);

    // Outside borders
    g->fillRect(BGCOLOR, bounds);

    gfx::Color bg = colors.background();

    drawRect(g, bounds,
             widget->hasFocus() ?
               parts.sunken2Focused().get():
               parts.sunken2Normal().get(), bg);

    // Draw the text
    int x = bounds.x + widget->border().left();
    int y = bounds.y + bounds.h / 2 - widget->textHeight() / 2;

    auto utf8_it = base::utf8_const_iterator(textString.begin());
    const int textlen = base::utf8_length(textString);

    if (scroll < textlen)
      utf8_it += scroll;

    for (c = scroll; c < textlen; ++c, ++utf8_it) {
      int ch = password ? '*' : *utf8_it;

      // Normal text
      bg = ColorNone;
      gfx::Color fg = colors.text();

      // Selected
      if (c >= selbeg && c <= selend) {
        if (widget->hasFocus())
          bg = colors.selected();
        else
          bg = colors.disabled();
        fg = colors.background();
      }

      // Disabled
      if (!widget->isEnabled()) {
        bg = ColorNone;
        fg = colors.disabled();
      }

      const int w = g->measureChar(ch).w;
      if (x + w > bounds.x2() - 3)
        return;

      const int caret_x = x;
      g->drawChar(ch, fg, bg, x, y);
      x += w;

      // Caret
      if (c == caret && state && widget->hasFocus())
        drawEntryCaret(g, widget, caret_x, y);
    }

    // Draw the caret if it is next of the last character
    if (c == caret && state &&
      widget->hasFocus() &&
      widget->isEnabled()) {
      drawEntryCaret(g, widget, x, y);
    }
  }

  void SkinTheme::paintComboBoxButton(PaintEvent& ev)
  {
    const auto widget = static_cast<Button*>(ev.getSource());
    Graphics* g = ev.graphics();
    IButtonIcon* iconInterface = widget->iconInterface();
    SkinPartPtr part_nw;
    gfx::Color bg;

    if (widget->isSelected()) {
      bg = colors.buttonSelectedFace();
      part_nw = parts.toolbuttonPushed();
    }
    // With mouse
    else if (widget->isEnabled() && widget->hasMouseOver()) {
      bg = colors.buttonHotFace();
      part_nw = parts.toolbuttonHot();
    }
    // Without mouse
    else {
      bg = colors.buttonNormalFace();
      part_nw = parts.toolbuttonLast();
    }

    const Rect rc = widget->clientBounds();

    // external background
    g->fillRect(BGCOLOR, rc);

    // draw borders
    drawRect(g, rc, part_nw.get(), bg);

    // Paint the icon
    if (iconInterface) {
      // Icon
      const int x = rc.x + rc.w/2 - iconInterface->size().w/2;
      const int y = rc.y + rc.h/2 - iconInterface->size().h/2;

      paintIcon(widget, ev.graphics(), iconInterface, x, y);
    }
  }

  void SkinTheme::paintTextBox(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    Widget* widget = ev.getSource();

    drawTextBox(g, widget, nullptr, nullptr,
                colors.textboxFace(),
                colors.textboxText());
  }

  void SkinTheme::paintView(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto widget = static_cast<View*>(ev.getSource());
    const Rect bounds = widget->clientBounds();
    const gfx::Color bg = BGCOLOR;
    Style* style = styles.view();

    if (const SkinStylePropertyPtr styleProp = widget->getProperty(SkinStyleProperty::Name))
      style = styleProp->getStyle();

    Style::State state;
    if (widget->hasMouseOver()) state += Style::hover();

    if (!is_transparent(bg))
      g->fillRect(bg, bounds);

    style->paint(g, bounds, nullptr, state);
  }

  void SkinTheme::paintViewScrollbar(PaintEvent& ev)
  {
    const auto widget = static_cast<ScrollBar*>(ev.getSource());
    Graphics* g = ev.graphics();
    int pos, len;

    bool isMiniLook = false;
    if (const SkinPropertyPtr skinProperty = widget->getProperty(SkinProperty::Name))
      isMiniLook = skinProperty->getLook() == MiniLook;

    Style* bgStyle;
    Style* thumbStyle;

    if (widget->isTransparent()) {
      bgStyle = styles.transparentScrollbar();
      thumbStyle = styles.transparentScrollbarThumb();
    }
    else if (isMiniLook) {
      bgStyle = styles.miniScrollbar();
      thumbStyle = styles.miniScrollbarThumb();
    }
    else {
      bgStyle = styles.scrollbar();
      thumbStyle = styles.scrollbarThumb();
    }

    widget->getScrollBarThemeInfo(&pos, &len);

    Style::State state;
    if (widget->hasMouse()) state += Style::hover();

    Rect rc = widget->clientBounds();
    bgStyle->paint(g, rc, nullptr, state);

    // Horizontal bar
    if (widget->align() & HORIZONTAL) {
      rc.x += pos;
      rc.w = len;
    }
    // Vertical bar
    else {
      rc.y += pos;
      rc.h = len;
    }

    thumbStyle->paint(g, rc, nullptr, state);
  }

  void SkinTheme::paintViewViewport(PaintEvent& ev)
  {
    const auto widget = static_cast<Viewport*>(ev.getSource());
    Graphics* g = ev.graphics();

    if (const gfx::Color bg = BGCOLOR; !is_transparent(bg))
      g->fillRect(bg, widget->clientBounds());
  }

  void SkinTheme::paintWindow(PaintEvent& ev)
  {
    Graphics* g = ev.graphics();
    const auto window = static_cast<Window*>(ev.getSource());
    Rect pos = window->clientBounds();
    const Rect cpos = window->clientChildrenBounds();

    if (!window->isDesktop()) {
      // window frame
      if (window->hasText()) {
        styles.window()->paint(g, pos, nullptr, Style::State());
        styles.windowTitle()->paint(g,
                                    Rect(cpos.x, pos.y+3*guiscale(), cpos.w, // TODO this hard-coded 3 should be configurable in skin.xml
                                              window->textHeight()),
                                    window->text().c_str(), Style::State());
      }
      // menubox
      else {
        styles.menubox()->paint(g, pos, nullptr, Style::State());
      }
    }
    // desktop
    else {
      styles.desktop()->paint(g, pos, nullptr, Style::State());
    }
  }

  void SkinTheme::paintPopupWindow(PaintEvent& ev)
  {
    const Widget* widget = ev.getSource();
    const auto window = static_cast<Window*>(ev.getSource());
    Graphics* g = ev.graphics();
    Rect pos = window->clientBounds();

    if (!is_transparent(BGCOLOR))
      styles.menubox()->paint(g, pos, nullptr, Style::State());

    pos.shrink(window->border());

    g->drawAlignedUIString(window->text(),
                           colors.text(),
                           window->bgColor(), pos,
                           window->align());
  }

  void SkinTheme::paintWindowButton(PaintEvent& ev)
  {
    const auto widget = static_cast<ButtonBase*>(ev.getSource());
    Graphics* g = ev.graphics();
    const Rect rc = widget->clientBounds();
    SkinPartPtr part;

    if (widget->isSelected())
      part = parts.windowCloseButtonSelected();
    else if (widget->hasMouseOver())
      part = parts.windowCloseButtonHot();
    else
      part = parts.windowCloseButtonNormal();

    g->drawRgbaSurface(part->bitmap(0), rc.x, rc.y);
  }

  void SkinTheme::paintTooltip(PaintEvent& ev)
  {
    const auto widget = static_cast<TipWindow*>(ev.getSource());
    Graphics* g = ev.graphics();
    const Rect absRc = widget->bounds();
    Rect rc = widget->clientBounds();
    const gfx::Color fg = colors.tooltipText();
    const gfx::Color bg = colors.tooltipFace();
    const SkinPartPtr tooltipPart = parts.tooltip();

    she::Surface* nw = tooltipPart->bitmapNW();
    she::Surface* n  = tooltipPart->bitmapN();
    she::Surface* ne = tooltipPart->bitmapNE();
    she::Surface* e  = tooltipPart->bitmapE();
    she::Surface* se = tooltipPart->bitmapSE();
    she::Surface* s  = tooltipPart->bitmapS();
    she::Surface* sw = tooltipPart->bitmapSW();
    she::Surface* w  = tooltipPart->bitmapW();

    switch (widget->arrowAlign()) {
      case TOP | LEFT:     nw = parts.tooltipArrow()->bitmapNW(); break;
      case TOP | RIGHT:    ne = parts.tooltipArrow()->bitmapNE(); break;
      case BOTTOM | LEFT:  sw = parts.tooltipArrow()->bitmapSW(); break;
      case BOTTOM | RIGHT: se = parts.tooltipArrow()->bitmapSE(); break;
      default: break;
    }

    drawRect(g, rc, nw, n, ne, e, se, s, sw, w);

    // Draw arrow in sides
    she::Surface* arrow = nullptr;
    Rect target(widget->target());
    target = target.createIntersection(Rect(0, 0, display_w(), display_h()));
    target.offset(-absRc.origin());

    switch (widget->arrowAlign()) {
      case TOP:
        arrow = parts.tooltipArrow()->bitmapN();
        g->drawRgbaSurface(arrow,
                           target.x + target.w / 2 - arrow->width() / 2,
                           rc.y);
        break;
      case BOTTOM:
        arrow = parts.tooltipArrow()->bitmapS();
        g->drawRgbaSurface(arrow,
                           target.x + target.w / 2 - arrow->width() / 2,
                           rc.y + rc.h-arrow->height());
        break;
      case LEFT:
        arrow = parts.tooltipArrow()->bitmapW();
        g->drawRgbaSurface(arrow,
                           rc.x,
                           target.y + target.h / 2 - arrow->height() / 2);
        break;
      case RIGHT:
        arrow = parts.tooltipArrow()->bitmapE();
        g->drawRgbaSurface(arrow,
                           rc.x + rc.w - arrow->width(),
                           target.y + target.h / 2 - arrow->height() / 2);
        break;
      default: break;
    }

    // Fill background
    g->fillRect(
      bg, Rect(rc).shrink(
        Border(
          w->width(),
          n->height(),
          e->width(),
          s->height())));

    rc.shrink(widget->border());

    g->drawAlignedUIString(widget->text(), fg, bg, rc, widget->align());
  }

  gfx::Color SkinTheme::getWidgetBgColor(const Widget* widget)
  {
    const gfx::Color c = widget->bgColor();
    const bool decorative = widget->isDecorative();

    if (!is_transparent(c) || widget->type() == kWindowWidget)
      return c;
    if (decorative)
      return colors.selected();
    return colors.face();
  }

  void SkinTheme::drawTextString(Graphics* g, const char *t, gfx::Color fg_color, const gfx::Color bg_color,
                                 const Widget* widget, const Rect& rc,
                                 const int selected_offset)
  {
    if (t || widget->hasText()) {
      Rect textrc;

      g->setFont(widget->font());

      if (!t)
        t = widget->text().c_str();

      textrc.setSize(g->measureUIString(t));

      // Horizontally text alignment

      if (widget->align() & RIGHT)
        textrc.x = rc.x + rc.w - textrc.w - 1;
      else if (widget->align() & CENTER)
        textrc.x = rc.center().x - textrc.w / 2;
      else
        textrc.x = rc.x;

      // Vertically text alignment

      if (widget->align() & BOTTOM)
        textrc.y = rc.y + rc.h - textrc.h - 1;
      else if (widget->align() & MIDDLE)
        textrc.y = rc.center().y - textrc.h / 2;
      else
        textrc.y = rc.y;

      if (widget->isSelected()) {
        textrc.x += selected_offset;
        textrc.y += selected_offset;
      }

      // Background
      if (!is_transparent(bg_color)) {
        if (!widget->isEnabled())
          g->fillRect(bg_color, Rect(textrc).inflate(guiscale(), guiscale()));
        else
          g->fillRect(bg_color, textrc);
      }

      // Text
      const Rect textWrap = textrc.createIntersection(
        // TODO add Widget::getPadding() property
        widget->clientBounds()).inflate(0, 1*guiscale());

      if (const IntersectClip clip(g, textWrap); clip) {
        if (!widget->isEnabled()) {
          // Draw white part
          g->drawUIString(t,
                          colors.background(),
                          ColorNone,
                          textrc.origin() + Point(guiscale(), guiscale()));
        }

        g->drawUIString(t,
                        !widget->isEnabled() ?
                          colors.disabled():
                          geta(fg_color) > 0 ? fg_color :
                          colors.text(),
                        bg_color, textrc.origin());
      }
    }
  }

  void SkinTheme::drawEntryCaret(Graphics* g, const Entry* widget, const int x, const int y)
  {
    const gfx::Color color = colors.text();
    const int h = widget->textHeight();

    for (int u = x; u < x + 2 * guiscale(); ++u)
      g->drawVLine(color, u, y - 1, h + 2);
  }

  she::Surface* SkinTheme::getToolIcon(const char* toolId) const
  {
    if (const auto it = m_toolicon.find(toolId); it != m_toolicon.end())
      return it->second;

    return nullptr;
  }

  void SkinTheme::drawRect(Graphics* g, const Rect& rc,
                           she::Surface* nw, she::Surface* n, she::Surface* ne,
                           she::Surface* e, she::Surface* se, she::Surface* s,
                           she::Surface* sw, she::Surface* w)
  {
    int x;

    // Top
    g->drawRgbaSurface(nw, rc.x, rc.y);
    {
      const IntersectClip clip(g, Rect(rc.x + nw->width(), rc.y,
                                 rc.w-nw->width()-ne->width(), rc.h));
      if (clip) {
        for (x = rc.x + nw->width();
             x < rc.x + rc.w-ne->width();
             x += n->width()) {
          g->drawRgbaSurface(n, x, rc.y);
        }
      }
    }

    g->drawRgbaSurface(ne, rc.x + rc.w-ne->width(), rc.y);

    // Bottom
    g->drawRgbaSurface(sw, rc.x, rc.y + rc.h-sw->height());
    {
      const IntersectClip clip(g, Rect(rc.x + sw->width(), rc.y,
                               rc.w-sw->width() - se->width(), rc.h));
      if (clip) {
        for (x = rc.x + sw->width();
             x < rc.x + rc.w-se->width();
             x += s->width()) {
          g->drawRgbaSurface(s, x, rc.y + rc.h - s->height());
        }
      }
    }

    g->drawRgbaSurface(se, rc.x + rc.w-se->width(), rc.y + rc.h-se->height());
    {
      const IntersectClip clip(g, Rect(rc.x, rc.y + nw->height(),
                                 rc.w, rc.h - nw->height()-sw->height()));
      if (clip) {
        int y;
        // Left
        for (y = rc.y + nw->height();
             y < rc.y + rc.h - sw->height();
             y += w->height()) {
          g->drawRgbaSurface(w, rc.x, y);
        }

        // Right
        for (y = rc.y + ne->height();
             y < rc.y + rc.h - se->height();
             y += e->height()) {
          g->drawRgbaSurface(e, rc.x + rc.w - e->width(), y);
        }
      }
    }
  }

  void SkinTheme::drawRect(Graphics* g, const Rect& rc, const SkinPart* skinPart, const gfx::Color bg)
  {
    drawRect(g, rc,
             skinPart->bitmap(0),
             skinPart->bitmap(1),
             skinPart->bitmap(2),
             skinPart->bitmap(3),
             skinPart->bitmap(4),
             skinPart->bitmap(5),
             skinPart->bitmap(6),
             skinPart->bitmap(7));

    // Center
    if (!is_transparent(bg)) {
      Rect inside = rc;
      inside.shrink(Border(
        skinPart->bitmap(7)->width(),
        skinPart->bitmap(1)->height(),
        skinPart->bitmap(3)->width(),
        skinPart->bitmap(5)->height()));

      if (const IntersectClip clip(g, inside); clip)
        g->fillRect(bg, inside);
    }
  }

  void SkinTheme::drawRect2(Graphics* g, const Rect& rc, const int x_mid,
                            const SkinPart* nw1, const SkinPart* nw2,
                            const gfx::Color bg1, const gfx::Color bg2)
  {
    Rect rc2(rc.x, rc.y, x_mid - rc.x + 1, rc.h);
    {
      if (const IntersectClip clip(g, rc2); clip)
        drawRect(g, rc, nw1, bg1);
    }

    rc2.x += rc2.w;
    rc2.w = rc.w - rc2.w;

    if (const IntersectClip clip(g, rc2); clip)
      drawRect(g, rc, nw2, bg2);
  }

  void SkinTheme::drawHline(Graphics* g, const Rect& rc, const SkinPart* skinPart)
  {
    int x;

    for (x = rc.x;
         x < rc.x2() - skinPart->size().w;
         x += skinPart->size().w) {
      g->drawRgbaSurface(skinPart->bitmap(0), x, rc.y);
    }

    if (x < rc.x2()) {
      const Rect rc2(x, rc.y, rc.w-(x-rc.x), skinPart->size().h);
      if (const IntersectClip clip(g, rc2); clip)
        g->drawRgbaSurface(skinPart->bitmap(0), x, rc.y);
    }
  }

  void SkinTheme::drawVline(Graphics* g, const Rect& rc, const SkinPart* skinPart)
  {
    int y;

    for (y = rc.y;
         y < rc.y2() - skinPart->size().h;
         y += skinPart->size().h) {
      g->drawRgbaSurface(skinPart->bitmap(0), rc.x, y);
    }

    if (y < rc.y2()) {
      const Rect rc2(rc.x, y, skinPart->size().w, rc.h - (y - rc.y));
      if (const IntersectClip clip(g, rc2); clip)
        g->drawRgbaSurface(skinPart->bitmap(0), rc.x, y);
    }
  }

  void SkinTheme::paintProgressBar(Graphics* g, const Rect& rc, const double progress)
  {
    g->drawRect(colors.text(), rc);

    Rect r = rc;
    r.shrink(1);

    int u = static_cast<int>(progress * r.w);
    u = MID(0, u, r.w);

    if (u > 0)
      g->fillRect(colors.selected(), Rect(r.x, r.y, u, r.h));

    if (1+u < r.w)
      g->fillRect(colors.background(), Rect(r.x + u, r.y, r.w - u, r.h));
  }

  void SkinTheme::paintIcon(const Widget* widget, Graphics* g, IButtonIcon* iconInterface, const int x, const int y)
  {
    she::Surface* icon_bmp = nullptr;

    // enabled
    if (widget->isEnabled()) {
      if (widget->isSelected())   // selected
        icon_bmp = iconInterface->selectedIcon();
      else
        icon_bmp = iconInterface->normalIcon();
    }
    // disabled
    else {
      icon_bmp = iconInterface->disabledIcon();
    }

    if (icon_bmp)
      g->drawRgbaSurface(icon_bmp, x, y);
  }

  std::shared_ptr<she::Font> SkinTheme::loadFont(const std::vector<std::pair<std::string, size_t>>& fonts) const
  {
    std::shared_ptr<she::Font> fallback;
    std::vector<std::pair<std::string, std::string>> candidates;

    for (const auto& [themeFont, size] : fonts) {
      if (base::get_file_extension(themeFont) != "png") {
        if (const auto f = she::instance()->loadTrueTypeFont(themeFont.c_str(), size * guiscale())) {
          f->setAntialias(true);
          return std::shared_ptr<she::Font>(f);
        }
        auto themeLower = base::string_to_lower(base::get_file_title(themeFont));
        for (auto& dir : base::get_font_paths()) {
          const auto item = FileSystemModule::instance()->getFileItemFromPath(dir);
          if (!item)
            continue;
          for (const auto child : item->children()) {
            if (child->isFolder())
              continue;
            if (auto shortName = base::string_to_lower(child->displayName()); shortName.find(themeLower) != std::string::npos)
              candidates.emplace_back(shortName, child->fileName());
          }
        }
      } else {
        try {
          const auto f = she::instance()->loadSpriteSheetFont(themeFont.c_str(), guiscale());
          if (f->isScalable())
            f->setSize(size);
          return std::shared_ptr<she::Font>(f);
        } catch(const std::exception&) {} // do nothing
      }
    }

    std::ranges::sort(candidates, [](auto& a, auto& b){
      return a.first.size() > b.first.size();
    });
    while (!candidates.empty()) {
      auto& themeFont = candidates.back().second;
      // Use the size from the first font pair (if available) for fallback fonts
      const auto size = fonts.empty() ? 0 : fonts.front().second;
      if (const auto f = she::instance()->loadTrueTypeFont(themeFont.c_str(), static_cast<int>(size) * guiscale())) {
        return std::shared_ptr<she::Font>(f);
      }
      candidates.pop_back();
    }

    return fallback;
  }

}
