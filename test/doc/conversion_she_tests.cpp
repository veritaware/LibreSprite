// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tests.h"

#include "doc/conversion_she.h"
#include "doc/image.h"
#include "doc/palette.h"
#include "gfx/color.h"
#include "she/sdl2/sdl2_surface.h"

#include <memory>

using namespace doc;

namespace {

// SDL2Surface can be built directly, without a live she::System: its
// (width, height, destroy) constructor just calls SDL_CreateRGBSurface with
// masks that match doc::rgba's own byte layout (r/g/b/a at shift 0/8/16/24),
// so the "fast path" in convert_image_to_surface (which requires the
// surface's shifts to equal doc::rgba_*_shift) is exercised - same as what
// she's own SDL2 backend hands out in the real app.
std::unique_ptr<she::SDL2Surface> makeSurface(int w, int h)
{
  return std::make_unique<she::SDL2Surface>(w, h, she::SDL2Surface::DestroyHandle);
}

gfx::Color pixelAt(she::Surface* surface, int x, int y)
{
  she::SurfaceLock lock(surface);
  return surface->getPixel(x, y);
}

} // namespace

TEST(ConvertImageToSurface, RgbImageCopiesExactly)
{
  std::unique_ptr<Image> image(Image::create(IMAGE_RGB, 2, 2));
  image->putPixel(0, 0, doc::rgba(10, 20, 30, 255));
  image->putPixel(1, 0, doc::rgba(40, 50, 60, 128));
  image->putPixel(0, 1, doc::rgba(70, 80, 90, 0));
  image->putPixel(1, 1, doc::rgba(255, 0, 0, 255));

  auto surface = makeSurface(2, 2);
  convert_image_to_surface(image.get(), nullptr, surface.get(), 0, 0, 0, 0, 2, 2);

  EXPECT_EQ(gfx::rgba(10, 20, 30, 255), pixelAt(surface.get(), 0, 0));
  EXPECT_EQ(gfx::rgba(40, 50, 60, 128), pixelAt(surface.get(), 1, 0));
  EXPECT_EQ(gfx::rgba(70, 80, 90, 0), pixelAt(surface.get(), 0, 1));
  EXPECT_EQ(gfx::rgba(255, 0, 0, 255), pixelAt(surface.get(), 1, 1));
}

TEST(ConvertImageToSurface, GrayscaleImageExpandsToRgba)
{
  std::unique_ptr<Image> image(Image::create(IMAGE_GRAYSCALE, 2, 1));
  image->putPixel(0, 0, doc::graya(128, 255));
  image->putPixel(1, 0, doc::graya(0, 64));

  auto surface = makeSurface(2, 1);
  convert_image_to_surface(image.get(), nullptr, surface.get(), 0, 0, 0, 0, 2, 1);

  EXPECT_EQ(gfx::rgba(128, 128, 128, 255), pixelAt(surface.get(), 0, 0));
  EXPECT_EQ(gfx::rgba(0, 0, 0, 64), pixelAt(surface.get(), 1, 0));
}

TEST(ConvertImageToSurface, IndexedImageLooksUpThePalette)
{
  auto palette = Palette::create(4);
  palette->setEntry(0, doc::rgba(0, 0, 0, 0));      // the "transparent" index
  palette->setEntry(1, doc::rgba(255, 0, 0, 255));
  palette->setEntry(2, doc::rgba(0, 255, 0, 255));
  palette->setEntry(3, doc::rgba(0, 0, 255, 200));

  std::unique_ptr<Image> image(Image::create(IMAGE_INDEXED, 4, 1));
  image->putPixel(0, 0, 0);
  image->putPixel(1, 0, 1);
  image->putPixel(2, 0, 2);
  image->putPixel(3, 0, 3);

  auto surface = makeSurface(4, 1);
  convert_image_to_surface(image.get(), palette.get(), surface.get(), 0, 0, 0, 0, 4, 1);

  // Index 0 is only "transparent" in the sense that its palette entry says
  // so (alpha 0) - convert_image_to_surface does a plain palette lookup,
  // with no separate special-casing of any particular index.
  EXPECT_EQ(gfx::rgba(0, 0, 0, 0), pixelAt(surface.get(), 0, 0));
  EXPECT_EQ(gfx::rgba(255, 0, 0, 255), pixelAt(surface.get(), 1, 0));
  EXPECT_EQ(gfx::rgba(0, 255, 0, 255), pixelAt(surface.get(), 2, 0));
  EXPECT_EQ(gfx::rgba(0, 0, 255, 200), pixelAt(surface.get(), 3, 0));
}

TEST(ConvertImageToSurface, BitmapImageLooksUpThePaletteByZeroOrOne)
{
  auto palette = Palette::create(2);
  palette->setEntry(0, doc::rgba(0, 0, 0, 0));
  palette->setEntry(1, doc::rgba(255, 255, 255, 255));

  std::unique_ptr<Image> image(Image::create(IMAGE_BITMAP, 2, 1));
  image->putPixel(0, 0, 0);
  image->putPixel(1, 0, 1);

  auto surface = makeSurface(2, 1);
  convert_image_to_surface(image.get(), palette.get(), surface.get(), 0, 0, 0, 0, 2, 1);

  EXPECT_EQ(gfx::rgba(0, 0, 0, 0), pixelAt(surface.get(), 0, 0));
  EXPECT_EQ(gfx::rgba(255, 255, 255, 255), pixelAt(surface.get(), 1, 0));
}

TEST(ConvertImageToSurface, PartialSourceRectOnlyCopiesThatRegion)
{
  // A 4x4 source image, but only its [1,1]-[3,3) sub-rect (2x2) is copied,
  // landing at (5,5) on a larger surface. Everything else on the surface
  // must be left at its cleared value.
  std::unique_ptr<Image> image(Image::create(IMAGE_RGB, 4, 4));
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x)
      image->putPixel(x, y, doc::rgba(x * 10, y * 10, 0, 255));

  auto surface = makeSurface(10, 10);
  surface->clear();

  convert_image_to_surface(image.get(), nullptr, surface.get(),
                            /*src*/ 1, 1, /*dst*/ 5, 5, /*w,h*/ 2, 2);

  // The copied 2x2 block: source (1,1)-(2,2) -> dest (5,5)-(6,6).
  EXPECT_EQ(gfx::rgba(10, 10, 0, 255), pixelAt(surface.get(), 5, 5));
  EXPECT_EQ(gfx::rgba(20, 10, 0, 255), pixelAt(surface.get(), 6, 5));
  EXPECT_EQ(gfx::rgba(10, 20, 0, 255), pixelAt(surface.get(), 5, 6));
  EXPECT_EQ(gfx::rgba(20, 20, 0, 255), pixelAt(surface.get(), 6, 6));

  // Untouched surface pixels remain cleared (all zero).
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 0, 0));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 9, 9));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 4, 4));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 7, 7));
}

TEST(ConvertImageToSurface, ClipsToTheImageBoundsWhenTheRectOverhangs)
{
  // Requesting a 4x4 region from a 2x2 image: only the overlapping 2x2
  // corner should actually be copied, at the requested destination offset.
  std::unique_ptr<Image> image(Image::create(IMAGE_RGB, 2, 2));
  image->putPixel(0, 0, doc::rgba(1, 2, 3, 255));
  image->putPixel(1, 0, doc::rgba(4, 5, 6, 255));
  image->putPixel(0, 1, doc::rgba(7, 8, 9, 255));
  image->putPixel(1, 1, doc::rgba(10, 11, 12, 255));

  auto surface = makeSurface(4, 4);
  surface->clear();

  convert_image_to_surface(image.get(), nullptr, surface.get(), 0, 0, 0, 0, 4, 4);

  EXPECT_EQ(gfx::rgba(1, 2, 3, 255), pixelAt(surface.get(), 0, 0));
  EXPECT_EQ(gfx::rgba(4, 5, 6, 255), pixelAt(surface.get(), 1, 0));
  EXPECT_EQ(gfx::rgba(7, 8, 9, 255), pixelAt(surface.get(), 0, 1));
  EXPECT_EQ(gfx::rgba(10, 11, 12, 255), pixelAt(surface.get(), 1, 1));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 2, 2));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 3, 3));
}

TEST(ConvertImageToSurface, ClipsToTheSurfaceBoundsWhenTheDestinationOverhangs)
{
  std::unique_ptr<Image> image(Image::create(IMAGE_RGB, 3, 3));
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x)
      image->putPixel(x, y, doc::rgba(x, y, 9, 255));

  auto surface = makeSurface(2, 2);
  surface->clear();

  // Destination offset (1,1) with a 3x3 source on a 2x2 surface: only the
  // single overlapping pixel at (1,1) can land anywhere.
  convert_image_to_surface(image.get(), nullptr, surface.get(), 0, 0, 1, 1, 3, 3);

  EXPECT_EQ(gfx::rgba(0, 0, 9, 255), pixelAt(surface.get(), 1, 1));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 0, 0));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 0, 1));
  EXPECT_EQ(gfx::Color(0), pixelAt(surface.get(), 1, 0));
}

TEST(ConvertImageToSurface, MatchesANaivePerPixelReferenceAcrossAWholeRgbImage)
{
  const int w = 6, h = 5;
  std::unique_ptr<Image> image(Image::create(IMAGE_RGB, w, h));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      image->putPixel(x, y, doc::rgba(
        (x * 37 + y * 11) & 0xff,
        (x * 53 + y * 7) & 0xff,
        (x * 13 + y * 29) & 0xff,
        (x + y) % 2 == 0 ? 255 : 60));
    }
  }

  auto surface = makeSurface(w, h);
  convert_image_to_surface(image.get(), nullptr, surface.get(), 0, 0, 0, 0, w, h);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      color_t src = image->getPixel(x, y);
      gfx::Color expected = gfx::rgba(
        doc::rgba_getr(src), doc::rgba_getg(src),
        doc::rgba_getb(src), doc::rgba_geta(src));
      EXPECT_EQ(expected, pixelAt(surface.get(), x, y)) << "at (" << x << "," << y << ")";
    }
  }
}

TEST(ConvertImageToSurface, MatchesANaivePerPixelReferenceForIndexedImages)
{
  auto palette = Palette::create(6);
  for (int i = 0; i < 6; ++i)
    palette->setEntry(i, doc::rgba(i * 40, 255 - i * 40, i * 10, i == 0 ? 0 : 255));

  const int w = 6, h = 1;
  std::unique_ptr<Image> image(Image::create(IMAGE_INDEXED, w, h));
  for (int x = 0; x < w; ++x)
    image->putPixel(x, 0, x);

  auto surface = makeSurface(w, h);
  convert_image_to_surface(image.get(), palette.get(), surface.get(), 0, 0, 0, 0, w, h);

  for (int x = 0; x < w; ++x) {
    color_t expectedColor = palette->getEntry(image->getPixel(x, 0));
    gfx::Color expected = gfx::rgba(
      doc::rgba_getr(expectedColor), doc::rgba_getg(expectedColor),
      doc::rgba_getb(expectedColor), doc::rgba_geta(expectedColor));
    EXPECT_EQ(expected, pixelAt(surface.get(), x, 0)) << "at index " << x;
  }
}
