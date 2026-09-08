// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/ui/skin/skin_theme_fonts.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace app::skin;

namespace {

// A resolver that "finds" every candidate name, mapping it to itself -
// isolates the tests from the real skins/fonts directories on disk while
// still letting fallback ordering (which candidate is tried first) show
// through in the output.
std::optional<std::string> resolveAll(const std::string& name)
{
  return name;
}

// A resolver that only "finds" names in the given allow-list - used to
// exercise the "preferred candidate isn't available, fall back to the
// next one" path.
std::function<std::optional<std::string>(const std::string&)>
resolveOnly(std::initializer_list<std::string> allowed)
{
  std::vector<std::string> list(allowed);
  return [list](const std::string& name) -> std::optional<std::string> {
    if (std::find(list.begin(), list.end(), name) != list.end())
      return name;
    return std::nullopt;
  };
}

std::unique_ptr<tinyxml2::XMLDocument> parse(const std::string& xml)
{
  auto doc = std::make_unique<tinyxml2::XMLDocument>();
  doc->Parse(xml.c_str());
  return doc;
}

} // namespace

TEST(SkinThemeFonts, MainAndMiniFamiliesAreKeyedByTheFamilyId)
{
  auto doc = parse(R"(
    <skin>
      <fonts>
        <family id="main_font"><font name="main.ttf" size="8"/></family>
        <family id="mini_font"><font name="mini.ttf" size="6"/></family>
      </fonts>
    </skin>
  )");

  std::vector<std::pair<std::string, size_t>> mainFonts, miniFonts;
  parseFontFamiliesFromSkinXml(*doc, "en", resolveAll, mainFonts, miniFonts);

  ASSERT_EQ(1u, mainFonts.size());
  EXPECT_EQ("main.ttf", mainFonts[0].first);
  EXPECT_EQ(8u, mainFonts[0].second);

  ASSERT_EQ(1u, miniFonts.size());
  EXPECT_EQ("mini.ttf", miniFonts[0].first);
  EXPECT_EQ(6u, miniFonts[0].second);
}

TEST(SkinThemeFonts, AFamilyIdOtherThanMainOrMiniIsIgnored)
{
  auto doc = parse(R"(
    <skin>
      <fonts>
        <family id="something_else"><font name="x.ttf" size="8"/></family>
      </fonts>
    </skin>
  )");

  std::vector<std::pair<std::string, size_t>> mainFonts, miniFonts;
  parseFontFamiliesFromSkinXml(*doc, "en", resolveAll, mainFonts, miniFonts);

  EXPECT_TRUE(mainFonts.empty());
  EXPECT_TRUE(miniFonts.empty());
}

TEST(SkinThemeFonts, ALangMatchingEntryIsPreferredOverTheLanguagelessDefault)
{
  auto doc = parse(R"(
    <skin>
      <fonts>
        <family id="main_font">
          <font name="default.ttf" size="8"/>
          <font name="japanese.ttf" size="8" lang="ja,jp"/>
        </family>
      </fonts>
    </skin>
  )");

  std::vector<std::pair<std::string, size_t>> mainFonts, miniFonts;
  parseFontFamiliesFromSkinXml(*doc, "ja", resolveAll, mainFonts, miniFonts);

  ASSERT_EQ(2u, mainFonts.size());
  EXPECT_EQ("japanese.ttf", mainFonts[0].first) << "the lang match goes first";
  EXPECT_EQ("default.ttf", mainFonts[1].first) << "then the language-less default, as a fallback";
}

TEST(SkinThemeFonts, WithNoLangMatchTheLanguagelessDefaultIsChosen)
{
  auto doc = parse(R"(
    <skin>
      <fonts>
        <family id="main_font">
          <font name="japanese.ttf" size="8" lang="ja,jp"/>
          <font name="default.ttf" size="8"/>
        </family>
      </fonts>
    </skin>
  )");

  std::vector<std::pair<std::string, size_t>> mainFonts, miniFonts;
  parseFontFamiliesFromSkinXml(*doc, "en", resolveAll, mainFonts, miniFonts);

  ASSERT_EQ(2u, mainFonts.size());
  EXPECT_EQ("default.ttf", mainFonts[0].first);
  EXPECT_EQ("japanese.ttf", mainFonts[1].first) << "still offered as a fallback";
}

TEST(SkinThemeFonts, WhenThePreferredCandidateDoesNotResolveTheNextOneIsUsed)
{
  auto doc = parse(R"(
    <skin>
      <fonts>
        <family id="main_font">
          <font name="missing.ttf" size="8" lang="ja"/>
          <font name="fallback.ttf" size="8"/>
        </family>
      </fonts>
    </skin>
  )");

  std::vector<std::pair<std::string, size_t>> mainFonts, miniFonts;
  parseFontFamiliesFromSkinXml(*doc, "ja", resolveOnly({"fallback.ttf"}), mainFonts, miniFonts);

  ASSERT_EQ(1u, mainFonts.size());
  EXPECT_EQ("fallback.ttf", mainFonts[0].first)
    << "the lang-matched candidate didn't resolve to a file, so it's dropped "
       "and the next candidate in fallback order is used instead";
}

TEST(SkinThemeFonts, AMissingFontsElementProducesEmptyLists)
{
  auto doc = parse("<skin></skin>");

  std::vector<std::pair<std::string, size_t>> mainFonts, miniFonts;
  parseFontFamiliesFromSkinXml(*doc, "en", resolveAll, mainFonts, miniFonts);

  EXPECT_TRUE(mainFonts.empty());
  EXPECT_TRUE(miniFonts.empty());
}
