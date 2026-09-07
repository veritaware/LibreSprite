// Aseprite Base Library
// Aseprite  | Copyright (C) 2001-2013 David Capello
// Besprited | Copyright (C) 2026      Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "base/split_string.h"
#include "base/string.h"

TEST(SplitString, Empty)
{
  std::vector<std::string> result;
  base::split_string("", result, ",");
  ASSERT_EQ(1, result.size());
  EXPECT_EQ("", result[0]);
}

TEST(SplitString, NoSeparator)
{
  std::vector<std::string> result;
  base::split_string("Hello,World", result, "");
  ASSERT_EQ(1, result.size());
  EXPECT_EQ("Hello,World", result[0]);
}

TEST(SplitString, OneSeparator)
{
  std::vector<std::string> result;
  base::split_string("Hello,World", result, ",");
  ASSERT_EQ(2, result.size());
  EXPECT_EQ("Hello", result[0]);
  EXPECT_EQ("World", result[1]);
}

TEST(SplitString, MultipleSeparators)
{
  std::vector<std::string> result;
  base::split_string("Hello,World", result, ",r");
  ASSERT_EQ(3, result.size());
  EXPECT_EQ("Hello", result[0]);
  EXPECT_EQ("Wo", result[1]);
  EXPECT_EQ("ld", result[2]);
}

// base::split() (base/string.h) is a separate, newer helper: single-char
// delimiter, returns the tokens directly instead of an out-parameter.

TEST(Split, EmptyStringYieldsOneEmptyToken)
{
  auto result = base::split("", ',');
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("", result[0]);
}

TEST(Split, NoDelimiterPresentYieldsTheWholeString)
{
  auto result = base::split("HelloWorld", ',');
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("HelloWorld", result[0]);
}

TEST(Split, SplitsOnEveryOccurrence)
{
  auto result = base::split("a,b,c", ',');
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("a", result[0]);
  EXPECT_EQ("b", result[1]);
  EXPECT_EQ("c", result[2]);
}

TEST(Split, LeadingDelimiterKeepsAnEmptyFirstToken)
{
  auto result = base::split(",a", ',');
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("", result[0]);
  EXPECT_EQ("a", result[1]);
}

TEST(Split, TrailingDelimiterKeepsAnEmptyLastToken)
{
  auto result = base::split("a,", ',');
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("a", result[0]);
  EXPECT_EQ("", result[1]);
}

TEST(Split, ConsecutiveDelimitersKeepEmptyTokensBetweenThem)
{
  auto result = base::split("a,,b", ',');
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("a", result[0]);
  EXPECT_EQ("", result[1]);
  EXPECT_EQ("b", result[2]);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
