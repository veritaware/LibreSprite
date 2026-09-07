// Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "base/range.h"

#include <vector>

TEST(Range, IteratesTheUnderlyingContainer)
{
  std::vector<int> values{1, 2, 3, 4};
  base::range<std::vector<int>::iterator> r{{values.begin(), values.end()}};

  std::vector<int> collected;
  for (int v : r)
    collected.push_back(v);

  EXPECT_EQ(values, collected);
}

TEST(Range, EmptyWhenBeginEqualsEnd)
{
  std::vector<int> values{1, 2, 3};
  base::range<std::vector<int>::iterator> r{{values.begin(), values.begin()}};

  EXPECT_TRUE(r.empty());
  EXPECT_EQ(r.begin(), r.end());
}

TEST(Range, NotEmptyWhenThereAreElements)
{
  std::vector<int> values{1};
  base::range<std::vector<int>::iterator> r{{values.begin(), values.end()}};

  EXPECT_FALSE(r.empty());
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
