// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/app.h"
#include "app/color.h"
#include "app/commands/cmd_fill.h"
#include "app/commands/cmd_stroke.h"
#include "app/context.h"
#include "app/document.h"
#include "app/document_undo.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/mask.h"
#include "doc/sprite.h"
#include "doc/test_context.h"
#include "render/render.h"

#include <memory>

using namespace app;
using namespace doc;

namespace {

// fill_mask()/stroke_mask() go through ExpandCelCanvas (whose static
// create_buffers() connects a slot to app::App::instance()->Exit) and
// Transaction::commit() -> DocumentUndo::add() (which reads
// App::instance()->preferences()) - both need a real, non-null
// App::instance() with its CoreModules (ConfigModule + Preferences)
// constructed. initializeCoreModulesForTesting() gives them that without
// pulling in everything else App::initialize() does (tools, commands,
// UIContext, palette loading, an optional ui::UISystem...).
//
// Deliberately leaked, constructed at most once for the whole test binary:
// App::~App() deletes the (separate, file-local) KeyboardShortcuts and
// GuiXml singletons without resetting their own instance pointers to null,
// which is fine for the one App a real process ever creates, but means a
// second App living and dying in the same process later double-frees them
// through those stale pointers.
void ensureApp()
{
  static app::App* app = new app::App();
  app->initializeCoreModulesForTesting();
}

// Same fixture pattern as test/app/document_api_tests.cpp: a TestContextT
// keeps the just-added document active (see doc/test_context.h), which is
// what fill_mask()/stroke_mask()'s ContextWriter need to find an active
// document/sprite/layer without a real UI selecting one.
struct FillStrokeFixture : public ::testing::Test {
  doc::TestContextT<app::Context> ctx;
  app::Document* doc = nullptr;

  FillStrokeFixture()
  {
    ensureApp();
  }

  void makeSprite(int w, int h)
  {
    doc = static_cast<app::Document*>(ctx.documents().add(w, h));
  }

  void selectRect(int x, int y, int w, int h)
  {
    Mask mask;
    mask.replace(gfx::Rect(x, y, w, h));
    doc->setMask(&mask);
  }

  // ExpandCelCanvas::commit() (used by both fill_mask() and stroke_mask())
  // crops the cel's image down to just its trimmed content bounds - after a
  // fill/stroke on a small selection, the cel image can be much smaller
  // than (and offset within) the sprite canvas, so pixel coordinates from
  // the test's point of view don't line up with raw cel-image coordinates
  // any more. Render the whole canvas into a full-size Image instead, so
  // every assertion can just use sprite/canvas coordinates directly - this
  // also sidesteps ExpandCelCanvas::commit() swapping in a brand-new Image
  // object for the cel each time (any Image* fetched before a fill/stroke
  // call is stale afterwards).
  std::unique_ptr<Image> renderCanvas()
  {
    Sprite* sprite = doc->sprite();
    std::unique_ptr<Image> canvas(
      Image::create(sprite->pixelFormat(), sprite->width(), sprite->height()));
    clear_image(canvas.get(), 0);
    render::Render render;
    render.renderSprite(canvas.get(), sprite, frame_t(0));
    return canvas;
  }
};

} // namespace

TEST_F(FillStrokeFixture, FillOnlyPaintsPixelsInsideTheMask)
{
  makeSprite(8, 8);
  selectRect(2, 2, 3, 3); // x in [2,4], y in [2,4]

  fill_mask(&ctx, app::Color::fromRgb(200, 100, 50), 255, "Fill");

  std::unique_ptr<Image> image = renderCanvas();
  EXPECT_EQ(doc::rgba(200, 100, 50, 255), image->getPixel(3, 3)) << "inside the mask";
  EXPECT_EQ(doc::rgba(200, 100, 50, 255), image->getPixel(2, 2)) << "mask corner";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(0, 0)) << "outside the mask, untouched";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(5, 5)) << "just past the mask, untouched";
}

TEST_F(FillStrokeFixture, FillOpacityBlendsIntoTheExistingTransparentPixel)
{
  makeSprite(4, 4);
  selectRect(0, 0, 4, 4);

  // The cel starts fully transparent (alpha 0): rgba_blender_normal() over a
  // zero-alpha backdrop keeps the source RGB untouched and sets the result
  // alpha to opacity outright (no partial mixing needed), so this is exact.
  fill_mask(&ctx, app::Color::fromRgb(10, 20, 30), 128, "Fill");

  EXPECT_EQ(doc::rgba(10, 20, 30, 128), renderCanvas()->getPixel(1, 1));
}

TEST_F(FillStrokeFixture, UndoAfterFillRestoresTheOriginalCel)
{
  makeSprite(4, 4);
  selectRect(0, 0, 4, 4);

  fill_mask(&ctx, app::Color::fromRgb(255, 0, 0), 255, "Fill");
  ASSERT_EQ(doc::rgba(255, 0, 0, 255), renderCanvas()->getPixel(1, 1));
  ASSERT_TRUE(doc->undoHistory()->canUndo());

  doc->undoHistory()->undo();

  EXPECT_EQ(doc::rgba(0, 0, 0, 0), renderCanvas()->getPixel(1, 1));
}

TEST_F(FillStrokeFixture, StrokeInsidePaintsOnlyTheBorderOfTheSelectionLeavingTheInteriorAlone)
{
  makeSprite(12, 12);
  selectRect(2, 2, 5, 5); // x,y in [2,6]

  stroke_mask(&ctx, app::Color::fromRgb(0, 255, 0), 255, 1,
              app::gen::StrokePosition::INSIDE, "Stroke");

  std::unique_ptr<Image> image = renderCanvas();
  EXPECT_EQ(doc::rgba(0, 255, 0, 255), image->getPixel(2, 2)) << "selection corner (border)";
  EXPECT_EQ(doc::rgba(0, 255, 0, 255), image->getPixel(2, 4)) << "selection edge (border)";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(4, 4)) << "selection center (true interior)";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(1, 4)) << "outside the selection, untouched";
}

TEST_F(FillStrokeFixture, StrokeOutsidePaintsOnlyAWidthWideBandOutsideTheSelection)
{
  makeSprite(12, 12);
  selectRect(2, 2, 5, 5); // x,y in [2,6]

  stroke_mask(&ctx, app::Color::fromRgb(0, 0, 255), 255, 1,
              app::gen::StrokePosition::OUTSIDE, "Stroke");

  std::unique_ptr<Image> image = renderCanvas();
  EXPECT_EQ(doc::rgba(0, 0, 255, 255), image->getPixel(1, 4)) << "1px outside the left edge";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(0, 4)) << "2px outside: past the stroke width";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(4, 4)) << "inside the selection, untouched";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(2, 2)) << "on the selection edge, untouched";
}

TEST_F(FillStrokeFixture, StrokeWidthControlsHowFarTheOutsideBandReaches)
{
  makeSprite(12, 12);
  selectRect(2, 2, 5, 5); // x,y in [2,6]

  stroke_mask(&ctx, app::Color::fromRgb(0, 0, 255), 255, 2,
              app::gen::StrokePosition::OUTSIDE, "Stroke");

  std::unique_ptr<Image> image = renderCanvas();
  EXPECT_EQ(doc::rgba(0, 0, 255, 255), image->getPixel(1, 4)) << "1px outside";
  EXPECT_EQ(doc::rgba(0, 0, 255, 255), image->getPixel(0, 4)) << "2px outside: now within width=2";
  EXPECT_EQ(doc::rgba(0, 0, 0, 0), image->getPixel(4, 4)) << "inside the selection, still untouched";
}

TEST_F(FillStrokeFixture, UndoAfterStrokeRestoresTheOriginalCel)
{
  makeSprite(12, 12);
  selectRect(2, 2, 5, 5);

  stroke_mask(&ctx, app::Color::fromRgb(0, 255, 0), 255, 1,
              app::gen::StrokePosition::INSIDE, "Stroke");
  ASSERT_EQ(doc::rgba(0, 255, 0, 255), renderCanvas()->getPixel(2, 2));
  ASSERT_TRUE(doc->undoHistory()->canUndo());

  doc->undoHistory()->undo();

  EXPECT_EQ(doc::rgba(0, 0, 0, 0), renderCanvas()->getPixel(2, 2));
}
