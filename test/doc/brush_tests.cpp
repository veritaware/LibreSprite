// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tests.h"

#include "doc/brush.h"
#include "doc/image.h"

#include <cmath>

using namespace doc;

namespace {

// Counts set pixels in a brush's (always square) bitmap image.
int countSetPixels(Brush& brush)
{
  Image* img = brush.image();
  int count = 0;
  for (int y = 0; y < img->height(); ++y)
    for (int x = 0; x < img->width(); ++x)
      if (img->getPixel(x, y) != 0)
        ++count;
  return count;
}

} // namespace

TEST(Brush, DefaultIsAOnePixelCircle)
{
  Brush brush;
  EXPECT_EQ(kCircleBrushType, brush.type());
  EXPECT_EQ(1, brush.size());
  EXPECT_EQ(0, brush.angle());
  EXPECT_EQ(1, brush.image()->width());
  EXPECT_EQ(1, brush.image()->height());
  EXPECT_NE(0, brush.image()->getPixel(0, 0));
}

TEST(Brush, CircleIgnoresRotationEntirely)
{
  Brush flat(kCircleBrushType, 9, 0);
  Brush rotated(kCircleBrushType, 9, 137);

  ASSERT_EQ(flat.bounds(), rotated.bounds());
  ASSERT_EQ(flat.image()->width(), rotated.image()->width());
  ASSERT_EQ(flat.image()->height(), rotated.image()->height());

  for (int y = 0; y < flat.image()->height(); ++y)
    for (int x = 0; x < flat.image()->width(); ++x)
      EXPECT_EQ(flat.image()->getPixel(x, y), rotated.image()->getPixel(x, y))
        << "at (" << x << "," << y << ")";
}

TEST(Brush, SquareAtZeroAngleKeepsTheOriginalSizeAndIsFullyFilled)
{
  Brush brush(kSquareBrushType, 6, 0);

  EXPECT_EQ(6, brush.image()->width());
  EXPECT_EQ(6, brush.image()->height());
  EXPECT_EQ(gfx::Rect(-3, -3, 6, 6), brush.bounds());
  EXPECT_EQ(6 * 6, countSetPixels(brush));
}

TEST(Brush, RotatedSquareGrowsItsCanvasBySqrtTwoTimesSizePlusTwo)
{
  const int size = 8;
  const int expectedCanvas = static_cast<int>(std::sqrt(2.0 * size * size)) + 2;

  Brush brush(kSquareBrushType, size, 45);

  EXPECT_EQ(expectedCanvas, brush.image()->width());
  EXPECT_EQ(expectedCanvas, brush.image()->height());
  EXPECT_EQ(gfx::Rect(-expectedCanvas / 2, -expectedCanvas / 2, expectedCanvas, expectedCanvas),
            brush.bounds());
  EXPECT_GT(countSetPixels(brush), 0);
  // The rotated square must fit inside its enlarged canvas without filling
  // it completely (that would mean no rotation actually happened).
  EXPECT_LT(countSetPixels(brush), expectedCanvas * expectedCanvas);
}

TEST(Brush, RotatedSquareSizeTwoOrLessNeverEnlargesTheCanvas)
{
  // size <= 2 bypasses the rotation branch entirely regardless of angle.
  Brush size1(kSquareBrushType, 1, 45);
  Brush size2(kSquareBrushType, 2, 45);

  EXPECT_EQ(1, size1.image()->width());
  EXPECT_EQ(2, size2.image()->width());
  EXPECT_GT(countSetPixels(size1), 0);
  EXPECT_EQ(4, countSetPixels(size2)); // fully filled 2x2
}

TEST(Brush, RotatedSquareAt90And180DegreesIsSymmetric)
{
  // A square rotated by a multiple of 90 degrees looks the same as the
  // unrotated square, just rasterized on the (still enlarged, since the
  // code enlarges for any nonzero angle) canvas.
  Brush at90(kSquareBrushType, 8, 90);
  Brush at180(kSquareBrushType, 8, 180);
  Brush at270(kSquareBrushType, 8, 270);

  ASSERT_EQ(at90.image()->width(), at180.image()->width());
  ASSERT_EQ(at90.image()->width(), at270.image()->width());

  int size = at90.image()->width();
  int count90 = countSetPixels(at90);
  int count180 = countSetPixels(at180);
  int count270 = countSetPixels(at270);

  // All three should cover the same number of pixels (same shape, just
  // rotated by a right angle each time).
  EXPECT_EQ(count90, count180);
  EXPECT_EQ(count90, count270);

  // The 180-degree rotation must be point-symmetric around the image
  // center: pixel (x,y) set iff pixel (size-1-x, size-1-y) is set.
  Image* img = at180.image();
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      EXPECT_EQ(img->getPixel(x, y) != 0, img->getPixel(size - 1 - x, size - 1 - y) != 0)
        << "at (" << x << "," << y << ")";
    }
  }
}

TEST(Brush, LineBrushDrawsThroughTheCenterAtTheGivenAngle)
{
  Brush horizontal(kLineBrushType, 8, 0);
  Brush vertical(kLineBrushType, 8, 90);

  ASSERT_EQ(horizontal.image()->width(), vertical.image()->width());
  int size = horizontal.image()->width();
  int center = size / 2;

  // A 0-degree line brush is a horizontal stroke through the vertical
  // center; a 90-degree one is a vertical stroke through the horizontal
  // center. Each must have at least one set pixel on its central row/column
  // and the two brushes must not draw identical bitmaps.
  bool anyOnCenterRow = false;
  for (int x = 0; x < size; ++x)
    anyOnCenterRow |= (horizontal.image()->getPixel(x, center) != 0);
  EXPECT_TRUE(anyOnCenterRow);

  bool anyOnCenterColumn = false;
  for (int y = 0; y < size; ++y)
    anyOnCenterColumn |= (vertical.image()->getPixel(center, y) != 0);
  EXPECT_TRUE(anyOnCenterColumn);

  bool identical = true;
  for (int y = 0; y < size && identical; ++y)
    for (int x = 0; x < size; ++x)
      if ((horizontal.image()->getPixel(x, y) != 0) != (vertical.image()->getPixel(x, y) != 0)) {
        identical = false;
        break;
      }
  EXPECT_FALSE(identical);
}

TEST(Brush, SetAngleTriggersRegeneration)
{
  Brush brush(kSquareBrushType, 8, 0);
  int genBefore = brush.gen();
  int widthBefore = brush.image()->width();

  brush.setAngle(45);

  EXPECT_NE(genBefore, brush.gen());
  EXPECT_NE(widthBefore, brush.image()->width()); // canvas grows for the rotated square
}

TEST(Brush, SetSizeTriggersRegeneration)
{
  Brush brush(kCircleBrushType, 4, 0);
  int genBefore = brush.gen();

  brush.setSize(9);

  EXPECT_NE(genBefore, brush.gen());
  EXPECT_EQ(9, brush.image()->width());
}

TEST(Brush, CopyConstructorPreservesTypeSizeAngleAndBitmap)
{
  Brush original(kSquareBrushType, 8, 45);
  Brush copy(original);

  EXPECT_EQ(original.type(), copy.type());
  EXPECT_EQ(original.size(), copy.size());
  EXPECT_EQ(original.angle(), copy.angle());
  EXPECT_EQ(original.bounds(), copy.bounds());

  Image* a = original.image();
  Image* b = copy.image();
  ASSERT_EQ(a->width(), b->width());
  ASSERT_EQ(a->height(), b->height());
  for (int y = 0; y < a->height(); ++y)
    for (int x = 0; x < a->width(); ++x)
      EXPECT_EQ(a->getPixel(x, y), b->getPixel(x, y));
}
