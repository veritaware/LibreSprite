// Aseprite  | Copyright (C) 2001-2015 David Capello
// Besprited | Copyright (C) 2026      Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/app.h"
#include "app/context.h"
#include "app/document.h"
#include "app/file/file.h"
#include "app/file/file_formats_manager.h"
#include "base/fs.h"
#include "doc/doc.h"
#include "she/system.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace app;

TEST(File, SeveralSizes)
{
  // Register all possible image formats.
  std::vector<char> fn(256);
  app::Context ctx;

  for (int w=10; w<=10+503*2; w+=503) {
    for (int h=10; h<=10+503*2; h+=503) {
      //std::sprintf(&fn[0], "test_%dx%d.ase", w, h);
      std::snprintf(&fn[0], fn.size(), "test.ase");

      {
        doc::Document* doc = ctx.documents().add(w, h, doc::ColorMode::INDEXED, 256);
        doc->setFilename(&fn[0]);

        // Random pixels
        Layer* layer = doc->sprite()->folder()->getFirstLayer();
        ASSERT_TRUE(layer != NULL);
        Image* image = layer->cel(frame_t(0))->image();
        std::srand(w*h);
        int c = std::rand()%256;
        for (int y=0; y<h; y++) {
          for (int x=0; x<w; x++) {
            put_pixel_fast<IndexedTraits>(image, x, y, c);
            if ((std::rand()&4) == 0)
              c = std::rand()%256;
          }
        }

        save_document(&ctx, doc);
        doc->close();
        delete doc;
      }

      {
        app::Document* doc = load_document(&ctx, &fn[0]);
        ASSERT_EQ(w, doc->sprite()->width());
        ASSERT_EQ(h, doc->sprite()->height());

        // Same random pixels (see the seed)
        Layer* layer = doc->sprite()->folder()->getFirstLayer();
        ASSERT_TRUE(layer != NULL);
        Image* image = layer->cel(frame_t(0))->image();
        std::srand(w*h);
        int c = std::rand()%256;
        for (int y=0; y<h; y++) {
          for (int x=0; x<w; x++) {
            ASSERT_EQ(c, get_pixel_fast<IndexedTraits>(image, x, y));
            if ((std::rand()&4) == 0)
              c = std::rand()%256;
          }
        }

        doc->close();
        delete doc;
      }
    }
  }
}

TEST(FileFormatsManager, SupportListsAseThenPngFirst)
{
  auto formats = FileFormatsManager::instance()->support(FILE_SUPPORT_LOAD);
  ASSERT_GE(formats.size(), 2u);

  // AseFormat overrides listPriority() to -100 and PngFormat to 0; every
  // other registered format is left at the default of 1, so these two must
  // sort to the front regardless of how many other formats are registered.
  EXPECT_STREQ("ase", formats[0]->name());
  EXPECT_STREQ("png", formats[1]->name());

  for (std::size_t i = 2; i < formats.size(); ++i) {
    EXPECT_STRNE("ase", formats[i]->name());
    EXPECT_STRNE("png", formats[i]->name());
  }
}

TEST(FileFormatsManager, GetFileFormatByExtensionIsCaseInsensitive)
{
  auto* manager = FileFormatsManager::instance();

  EXPECT_STREQ("ase", manager->getFileFormatByExtension("ase")->name());
  EXPECT_STREQ("ase", manager->getFileFormatByExtension("ASE")->name());
  EXPECT_STREQ("ase", manager->getFileFormatByExtension("AsE")->name());
  EXPECT_STREQ("png", manager->getFileFormatByExtension("PNG")->name());
}

TEST(FileFormatsManager, GetFileFormatByExtensionMatchesAnyTokenInAMultiExtensionList)
{
  auto* manager = FileFormatsManager::instance();

  // AseFormat: onGetExtensions() == "ase,aseprite"
  EXPECT_STREQ("ase", manager->getFileFormatByExtension("aseprite")->name());
  EXPECT_STREQ("ase", manager->getFileFormatByExtension("ASEPRITE")->name());

  // JpegFormat: onGetExtensions() == "jpeg,jpg"
  EXPECT_STREQ("jpeg", manager->getFileFormatByExtension("jpeg")->name());
  EXPECT_STREQ("jpeg", manager->getFileFormatByExtension("jpg")->name());
  EXPECT_STREQ("jpeg", manager->getFileFormatByExtension("JPG")->name());
}

TEST(FileFormatsManager, GetFileFormatByExtensionReturnsNullForAnUnknownExtension)
{
  EXPECT_EQ(nullptr, FileFormatsManager::instance()->getFileFormatByExtension("not-a-real-format"));
}

TEST(File, LoadFallsBackToSheFormatWhenTheExtensionMatchesNothing)
{
  // she::instance() must be live for SheFormat::onLoad() (it calls
  // she::instance()->loadRgbaSurface()) - none of the other tests in this
  // file need it, so it's not constructed by default.
  std::unique_ptr<she::System> sys(she::create_system());

  app::Context ctx;
  const int w = 5, h = 4;

  doc::Document* doc = ctx.documents().add(w, h, doc::ColorMode::RGB);
  doc->setFilename("she_fallback_test.png");

  Layer* layer = doc->sprite()->folder()->getFirstLayer();
  ASSERT_TRUE(layer != NULL);
  Image* image = layer->cel(frame_t(0))->image();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      put_pixel(image, x, y, doc::rgba(x * 10, y * 20, 128, 255));

  ASSERT_EQ(0, save_document(&ctx, doc));
  doc->close();
  delete doc;

  // A real PNG file, but under an extension no registered FileFormat
  // claims - getFileFormatByExtension() would return null for it, so
  // loading must fall through to SheFormat's loadPriority()-based fallback
  // (she_fallback_test.png -> she_fallback_test.notaformat).
  ASSERT_TRUE(base::is_file("she_fallback_test.png"));
  if (base::is_file("she_fallback_test.notaformat"))
    base::delete_file("she_fallback_test.notaformat");
  std::rename("she_fallback_test.png", "she_fallback_test.notaformat");

  ASSERT_EQ(nullptr, FileFormatsManager::instance()->getFileFormatByExtension("notaformat"));

  app::Document* loaded = load_document(&ctx, "she_fallback_test.notaformat");
  ASSERT_NE(nullptr, loaded);
  ASSERT_EQ(w, loaded->sprite()->width());
  ASSERT_EQ(h, loaded->sprite()->height());

  Layer* loadedLayer = loaded->sprite()->folder()->getFirstLayer();
  ASSERT_TRUE(loadedLayer != NULL);
  Image* loadedImage = loadedLayer->cel(frame_t(0))->image();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      EXPECT_EQ(doc::rgba(x * 10, y * 20, 128, 255), loadedImage->getPixel(x, y))
        << "at (" << x << "," << y << ")";

  loaded->close();
  delete loaded;
}
