// Blending modes unit tests
// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtest/gtest.h>

#include "doc/blend_funcs.h"
#include "doc/blend_mode.h"
#include "doc/color.h"

using namespace doc;

namespace {

const BlendMode kSpecialRgbaModes[] = {
  BlendMode::MULTIPLY,
  BlendMode::SCREEN,
  BlendMode::OVERLAY,
  BlendMode::DARKEN,
  BlendMode::LIGHTEN,
  BlendMode::COLOR_DODGE,
  BlendMode::COLOR_BURN,
  BlendMode::HARD_LIGHT,
  BlendMode::SOFT_LIGHT,
  BlendMode::DIFFERENCE,
  BlendMode::EXCLUSION,
  BlendMode::HSL_HUE,
  BlendMode::HSL_SATURATION,
  BlendMode::HSL_COLOR,
  BlendMode::HSL_LUMINOSITY,
};

const BlendMode kSpecialGrayaModes[] = {
  BlendMode::MULTIPLY,
  BlendMode::SCREEN,
  BlendMode::OVERLAY,
  BlendMode::DARKEN,
  BlendMode::LIGHTEN,
  BlendMode::COLOR_DODGE,
  BlendMode::COLOR_BURN,
  BlendMode::HARD_LIGHT,
  BlendMode::SOFT_LIGHT,
  BlendMode::DIFFERENCE,
  BlendMode::EXCLUSION,
};

} // anonymous namespace

TEST(BlendFuncs, RgbaSpecialModesFallBackToNormalOverTransparentBackdrop)
{
  const color_t backdrop = rgba(0, 0, 0, 0);
  const color_t src = rgba(200, 100, 50, 255);

  for (BlendMode mode : kSpecialRgbaModes) {
    BlendFunc blender = get_rgba_blender(mode);
    color_t expected = rgba_blender_normal(backdrop, src, 255);
    color_t actual = blender(backdrop, src, 255);
    EXPECT_EQ(expected, actual) << "blend mode " << (int)mode;
    // Over a transparent backdrop, the result must just be the source color
    // (the blend mode must not alter the RGB channels).
    EXPECT_EQ(rgba_getr(src), rgba_getr(actual)) << "blend mode " << (int)mode;
    EXPECT_EQ(rgba_getg(src), rgba_getg(actual)) << "blend mode " << (int)mode;
    EXPECT_EQ(rgba_getb(src), rgba_getb(actual)) << "blend mode " << (int)mode;
  }
}

TEST(BlendFuncs, RgbaSpecialModesStillBlendOverOpaqueBackdrop)
{
  const color_t backdrop = rgba(10, 20, 30, 255);
  const color_t src = rgba(200, 100, 50, 255);

  // Sanity check: Multiply must still behave like Multiply when the
  // backdrop is opaque (i.e. this isn't just always falling back to Normal).
  BlendFunc multiply = get_rgba_blender(BlendMode::MULTIPLY);
  color_t normalResult = rgba_blender_normal(backdrop, src, 255);
  color_t multiplyResult = multiply(backdrop, src, 255);
  EXPECT_NE(normalResult, multiplyResult);
}

TEST(BlendFuncs, GrayaSpecialModesFallBackToNormalOverTransparentBackdrop)
{
  const color_t backdrop = graya(0, 0);
  const color_t src = graya(180, 255);

  for (BlendMode mode : kSpecialGrayaModes) {
    BlendFunc blender = get_graya_blender(mode);
    color_t expected = graya_blender_normal(backdrop, src, 255);
    color_t actual = blender(backdrop, src, 255);
    EXPECT_EQ(expected, actual) << "blend mode " << (int)mode;
    EXPECT_EQ(graya_getv(src), graya_getv(actual)) << "blend mode " << (int)mode;
  }
}

TEST(BlendFuncs, GrayaSpecialModesStillBlendOverOpaqueBackdrop)
{
  const color_t backdrop = graya(60, 255);
  const color_t src = graya(180, 255);

  BlendFunc multiply = get_graya_blender(BlendMode::MULTIPLY);
  color_t normalResult = graya_blender_normal(backdrop, src, 255);
  color_t multiplyResult = multiply(backdrop, src, 255);
  EXPECT_NE(normalResult, multiplyResult);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
