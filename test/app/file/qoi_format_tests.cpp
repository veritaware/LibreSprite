// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/context.h"
#include "app/document.h"
#include "app/file/file.h"
#include "doc/doc.h"

#include <cstdlib>

using namespace app;

namespace {

// Saves `doc` (already positioned at `filename`) as .qoi, reloads it and
// returns the reloaded document, or nullptr if either step failed. `doc`
// is closed and deleted either way.
app::Document* roundTrip(app::Context& ctx, doc::Document* doc, const char* filename)
{
  doc->setFilename(filename);
  int saveResult = save_document(&ctx, doc);
  doc->close();
  delete doc;
  if (saveResult != 0)
    return nullptr;
  return load_document(&ctx, filename);
}

} // namespace

TEST(QoiFormat, RgbRoundTripPreservesEveryPixelIncludingAlpha)
{
  app::Context ctx;
  const int w = 6, h = 5;

  doc::Document* doc = ctx.documents().add(w, h, doc::ColorMode::RGB);
  Layer* layer = doc->sprite()->folder()->getFirstLayer();
  ASSERT_TRUE(layer != NULL);
  Image* image = layer->cel(frame_t(0))->image();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      put_pixel(image, x, y, doc::rgba(
        (x * 37) & 0xff, (y * 53) & 0xff, ((x + y) * 11) & 0xff,
        (x + y) % 3 == 0 ? 0 : (x + y) % 3 == 1 ? 128 : 255));
    }
  }

  app::Document* loaded = roundTrip(ctx, doc, "qoi_rgb_test.qoi");
  ASSERT_NE(nullptr, loaded);
  ASSERT_EQ(w, loaded->sprite()->width());
  ASSERT_EQ(h, loaded->sprite()->height());

  Layer* loadedLayer = loaded->sprite()->folder()->getFirstLayer();
  ASSERT_TRUE(loadedLayer != NULL);
  Image* loadedImage = loadedLayer->cel(frame_t(0))->image();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      color_t expected = doc::rgba(
        (x * 37) & 0xff, (y * 53) & 0xff, ((x + y) * 11) & 0xff,
        (x + y) % 3 == 0 ? 0 : (x + y) % 3 == 1 ? 128 : 255);
      EXPECT_EQ(expected, loadedImage->getPixel(x, y)) << "at (" << x << "," << y << ")";
    }
  }

  loaded->close();
  delete loaded;
}

TEST(QoiFormat, OnePixelImageRoundTrips)
{
  app::Context ctx;

  doc::Document* doc = ctx.documents().add(1, 1, doc::ColorMode::RGB);
  Layer* layer = doc->sprite()->folder()->getFirstLayer();
  Image* image = layer->cel(frame_t(0))->image();
  put_pixel(image, 0, 0, doc::rgba(200, 100, 50, 180));

  app::Document* loaded = roundTrip(ctx, doc, "qoi_1x1_test.qoi");
  ASSERT_NE(nullptr, loaded);
  ASSERT_EQ(1, loaded->sprite()->width());
  ASSERT_EQ(1, loaded->sprite()->height());

  Layer* loadedLayer = loaded->sprite()->folder()->getFirstLayer();
  Image* loadedImage = loadedLayer->cel(frame_t(0))->image();
  EXPECT_EQ(doc::rgba(200, 100, 50, 180), loadedImage->getPixel(0, 0));

  loaded->close();
  delete loaded;
}

TEST(QoiFormat, LargerImageWithVariedContentRoundTrips)
{
  app::Context ctx;
  const int w = 96, h = 64;

  doc::Document* doc = ctx.documents().add(w, h, doc::ColorMode::RGB);
  Layer* layer = doc->sprite()->folder()->getFirstLayer();
  Image* image = layer->cel(frame_t(0))->image();
  std::srand(w * h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      put_pixel(image, x, y, doc::rgba(
        std::rand() % 256, std::rand() % 256, std::rand() % 256, std::rand() % 256));
    }
  }

  // Re-read the pixels we just wrote (rather than re-seeding rand()), so
  // this doesn't depend on QOI preserving values through some transform.
  std::vector<color_t> expected(w * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      expected[y * w + x] = image->getPixel(x, y);

  app::Document* loaded = roundTrip(ctx, doc, "qoi_large_test.qoi");
  ASSERT_NE(nullptr, loaded);
  ASSERT_EQ(w, loaded->sprite()->width());
  ASSERT_EQ(h, loaded->sprite()->height());

  Layer* loadedLayer = loaded->sprite()->folder()->getFirstLayer();
  Image* loadedImage = loadedLayer->cel(frame_t(0))->image();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      ASSERT_EQ(expected[y * w + x], loadedImage->getPixel(x, y)) << "at (" << x << "," << y << ")";

  loaded->close();
  delete loaded;
}

TEST(QoiFormat, SavingAnIndexedSpriteFailsGracefully)
{
  // QoiFormat's onGetFlags() doesn't include FILE_SUPPORT_INDEXED, so
  // createSaveDocumentOperation() must reject this up front rather than
  // handing indexed pixel data to QoiFormat::onSave() (which assumes 4
  // interleaved RGBA bytes per pixel).
  app::Context ctx;
  doc::Document* doc = ctx.documents().add(4, 4, doc::ColorMode::INDEXED, 4);
  doc->setFilename("qoi_indexed_test.qoi");

  EXPECT_EQ(-1, save_document(&ctx, doc));

  doc->close();
  delete doc;
}

TEST(QoiFormat, SavingAGrayscaleSpriteFailsGracefully)
{
  app::Context ctx;
  doc::Document* doc = ctx.documents().add(4, 4, doc::ColorMode::GRAYSCALE);
  doc->setFilename("qoi_grayscale_test.qoi");

  EXPECT_EQ(-1, save_document(&ctx, doc));

  doc->close();
  delete doc;
}
