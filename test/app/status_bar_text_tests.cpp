// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/ui/status_bar_text.h"

using namespace app;
using Kind = StatusBarTextToken::Kind;

namespace {

::testing::AssertionResult isText(const StatusBarTextToken& t, const std::string& value)
{
  if (t.kind != Kind::Text)
    return ::testing::AssertionFailure() << "expected a Text token, got an Icon token (\"" << t.value << "\")";
  if (t.value != value)
    return ::testing::AssertionFailure() << "expected text \"" << value << "\", got \"" << t.value << "\"";
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult isIcon(const StatusBarTextToken& t, const std::string& value)
{
  if (t.kind != Kind::Icon)
    return ::testing::AssertionFailure() << "expected an Icon token, got a Text token (\"" << t.value << "\")";
  if (t.value != value)
    return ::testing::AssertionFailure() << "expected icon \"" << value << "\", got \"" << t.value << "\"";
  return ::testing::AssertionSuccess();
}

} // namespace

// Regression coverage for commit cba84ff20 ("Fix freeze and out-of-bounds
// memory read with : in layer names"): a lone ':' with no matching close
// used to read `*(j+1)` one byte past the string's terminator once `j`
// itself stopped on the terminator (an infinite loop/freeze scanning
// forward, and an OOB read past it). These are exactly its repro cases.

TEST(TokenizeStatusBarText, ATrailingColonWithNothingAfterItIsLiteralText)
{
  auto tokens = tokenizeStatusBarText("a:");
  ASSERT_EQ(1u, tokens.size());
  EXPECT_TRUE(isText(tokens[0], "a:"));
}

TEST(TokenizeStatusBarText, ALeadingColonWithNoClosingColonIsLiteralText)
{
  auto tokens = tokenizeStatusBarText(":b");
  ASSERT_EQ(1u, tokens.size());
  EXPECT_TRUE(isText(tokens[0], ":b"));
}

TEST(TokenizeStatusBarText, ColonsSeparatedBySpacesWithNoNameBetweenThemAreLiteralText)
{
  auto tokens = tokenizeStatusBarText("a : b");
  ASSERT_EQ(1u, tokens.size());
  EXPECT_TRUE(isText(tokens[0], "a : b"));
}

TEST(TokenizeStatusBarText, AColonMarkerClosedByEndOfStringIsRecognized)
{
  auto tokens = tokenizeStatusBarText("layer :name:");
  // "layer " then ":name:" - the opening ':' is preceded by a space, and
  // "name" is closed by a ':' right at the end of the string - a proper
  // icon marker, contrasting with the truly-unterminated cases above.
  ASSERT_EQ(2u, tokens.size());
  EXPECT_TRUE(isText(tokens[0], "layer"));
  EXPECT_TRUE(isIcon(tokens[1], "name"));
}

TEST(TokenizeStatusBarText, PlainTextWithNoColonsIsOneTextToken)
{
  auto tokens = tokenizeStatusBarText("hello world");
  ASSERT_EQ(1u, tokens.size());
  EXPECT_TRUE(isText(tokens[0], "hello world"));
}

TEST(TokenizeStatusBarText, EmptyStringProducesNoTokens)
{
  EXPECT_TRUE(tokenizeStatusBarText("").empty());
}

TEST(TokenizeStatusBarText, AnIconMarkerAtTheStartOfTheStringIsRecognized)
{
  auto tokens = tokenizeStatusBarText(":icon: rest");
  ASSERT_EQ(2u, tokens.size());
  EXPECT_TRUE(isIcon(tokens[0], "icon"));
  EXPECT_TRUE(isText(tokens[1], "rest"));
}

TEST(TokenizeStatusBarText, AnIconMarkerInTheMiddleSplitsTheSurroundingText)
{
  auto tokens = tokenizeStatusBarText("before :icon: after");
  ASSERT_EQ(3u, tokens.size());
  EXPECT_TRUE(isText(tokens[0], "before"));
  EXPECT_TRUE(isIcon(tokens[1], "icon"));
  EXPECT_TRUE(isText(tokens[2], "after"));
}

TEST(TokenizeStatusBarText, MultipleIconMarkersAreAllRecognized)
{
  auto tokens = tokenizeStatusBarText(":one: mid :two:");
  ASSERT_EQ(3u, tokens.size());
  EXPECT_TRUE(isIcon(tokens[0], "one"));
  EXPECT_TRUE(isText(tokens[1], "mid"));
  EXPECT_TRUE(isIcon(tokens[2], "two"));
}

TEST(TokenizeStatusBarText, AColonNotAtAWordBoundaryIsLiteralText)
{
  // The opening ':' must be at the very start or right after a space -
  // "a:b:" has its ':' preceded by 'a', not a space, so it's never treated
  // as an icon marker at all.
  auto tokens = tokenizeStatusBarText("a:b:");
  ASSERT_EQ(1u, tokens.size());
  EXPECT_TRUE(isText(tokens[0], "a:b:"));
}
