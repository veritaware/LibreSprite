// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/commands/cmd_remove_layer.h"

using namespace app;

TEST(RemoveLayerGuards, WouldRemoveAllLayersOnlyWhenTheRangeCoversEveryLayer)
{
  EXPECT_TRUE(wouldRemoveAllLayers(3, 3));
  EXPECT_FALSE(wouldRemoveAllLayers(2, 3));
  EXPECT_FALSE(wouldRemoveAllLayers(0, 3));
}

TEST(RemoveLayerGuards, WouldRemoveTheLastLayerOnlyWhenExactlyOneLayerIsLeft)
{
  EXPECT_TRUE(wouldRemoveTheLastLayer(1));
  EXPECT_FALSE(wouldRemoveTheLastLayer(2));
  EXPECT_FALSE(wouldRemoveTheLastLayer(0)) << "0 layers isn't the 'last layer' case";
}

TEST(RemoveLayerGuards, AnyLayerHiddenIsFalseWhenEveryLayerIsVisible)
{
  EXPECT_FALSE(anyLayerHidden({true, true, true}));
}

TEST(RemoveLayerGuards, AnyLayerHiddenIsTrueWhenAtLeastOneLayerIsHidden)
{
  EXPECT_TRUE(anyLayerHidden({true, false, true}));
  EXPECT_TRUE(anyLayerHidden({false}));
}

TEST(RemoveLayerGuards, AnyLayerHiddenIsFalseForAnEmptyList)
{
  EXPECT_FALSE(anyLayerHidden({}));
}
