// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/pref/preferences.h" // for app::gen::SymmetryMode; pulls in pref.xml.h
#include "app/tools/stroke.h"
#include "app/tools/symmetry.h"
#include "app/tools/tool_loop.h"
#include "doc/brush.h"
#include "gfx/region.h"
#include "render/zoom.h"

#include <algorithm>
#include <set>

using namespace app::tools;
using namespace doc;

namespace {

// Symmetry::generateStrokes() only ever calls loop->getBrush() (to check
// the brush bounds' width/height parity), so every other member of this
// otherwise huge interface is an unused stub.
class FakeToolLoop : public ToolLoop {
public:
  explicit FakeToolLoop(Brush brush) : m_brush(std::move(brush)) {}

  void dispose() override {}
  Tool* getTool() override { return nullptr; }
  Brush* getBrush() override { return &m_brush; }
  app::Document* getDocument() override { return nullptr; }
  Sprite* sprite() override { return nullptr; }
  Layer* getLayer() override { return nullptr; }
  doc::frame_t getFrame() override { return 0; }
  const Image* getSrcImage() override { return nullptr; }
  const Image* getFloodFillSrcImage() override { return nullptr; }
  Image* getDstImage() override { return nullptr; }
  void validateSrcImage(const gfx::Region&) override {}
  void validateDstImage(const gfx::Region&) override {}
  void invalidateDstImage() override {}
  void invalidateDstImage(const gfx::Region&) override {}
  void copyValidDstToSrcImage(const gfx::Region&) override {}
  RgbMap* getRgbMap() override { return nullptr; }
  bool useMask() override { return false; }
  Mask* getMask() override { return nullptr; }
  void setMask(Mask*) override {}
  gfx::Point getMaskOrigin() override { return {}; }
  const render::Zoom& zoom() override { return m_zoom; }
  Button getMouseButton() override { return Left; }
  doc::color_t getFgColor() override { return 0; }
  doc::color_t getBgColor() override { return 0; }
  doc::color_t getPrimaryColor() override { return 0; }
  void setPrimaryColor(doc::color_t) override {}
  doc::color_t getSecondaryColor() override { return 0; }
  void setSecondaryColor(doc::color_t) override {}
  int getOpacity() override { return 255; }
  int getTolerance() override { return 0; }
  bool getContiguous() override { return true; }
  ToolLoopModifiers getModifiers() override { return {}; }
  filters::TiledMode getTiledMode() override { return {}; }
  bool getGridVisible() override { return false; }
  bool getSnapToGrid() override { return false; }
  bool getStopAtGrid() override { return false; }
  gfx::Rect getGridBounds() override { return {}; }
  bool getFilled() override { return false; }
  bool getPreviewFilled() override { return false; }
  int getSprayWidth() override { return 0; }
  int getSpraySpeed() override { return 0; }
  gfx::Point getCelOrigin() override { return {}; }
  void setSpeed(const gfx::Point&) override {}
  gfx::Point getSpeed() override { return {}; }
  Ink* getInk() override { return nullptr; }
  Controller* getController() override { return nullptr; }
  PointShape* getPointShape() override { return nullptr; }
  Intertwine* getIntertwine() override { return nullptr; }
  TracePolicy getTracePolicy() override { return {}; }
  Symmetry* getSymmetry() override { return nullptr; }
  const doc::Remap* getShadingRemap() override { return nullptr; }
  void cancel() override {}
  bool isCanceled() override { return false; }
  gfx::Region& getDirtyArea() override { return m_dirtyArea; }
  void updateDirtyArea() override {}
  void updateStatusBar(const char*) override {}

private:
  Brush m_brush;
  render::Zoom m_zoom{1, 1};
  gfx::Region m_dirtyArea;
};

Stroke onePoint(int x, int y, float pressure) {
  Stroke s;
  s.addPoint({x, y, pressure});
  return s;
}

} // namespace

TEST(HorizontalSymmetry, MirrorsAroundTheAxisWithEvenBrushSize)
{
  Brush brush(doc::kCircleBrushType, 4, 0); // even size -> bounds.w % 2 == 0
  FakeToolLoop loop{brush};

  HorizontalSymmetry axis(10);
  Strokes out;
  axis.generateStrokes(onePoint(12, 5, 0.5f), out, &loop);

  ASSERT_EQ(2u, out.size());
  EXPECT_EQ(12, out[0][0].x);
  EXPECT_EQ(5, out[0][0].y);
  // Mirrors 12 around x=10 -> 8 (no +1 adjustment for an even brush).
  EXPECT_EQ(8, out[1][0].x);
  EXPECT_EQ(5, out[1][0].y);
  EXPECT_FLOAT_EQ(0.5f, out[1][0].pressure);
}

TEST(HorizontalSymmetry, MirrorsAroundTheAxisWithOddBrushSizeAdjustsByOne)
{
  Brush brush(doc::kCircleBrushType, 5, 0); // odd size -> bounds.w % 2 == 1
  FakeToolLoop loop{brush};

  HorizontalSymmetry axis(10);
  Strokes out;
  axis.generateStrokes(onePoint(12, 5, 1.0f), out, &loop);

  ASSERT_EQ(2u, out.size());
  // With the +1 adjustment: 10 - (12 - 10 + 1) = 7.
  EXPECT_EQ(7, out[1][0].x);
  EXPECT_EQ(5, out[1][0].y);
}

TEST(VerticalSymmetry, MirrorsAroundTheAxisWithEvenAndOddBrushSize)
{
  Brush even(doc::kCircleBrushType, 4, 0);
  Brush odd(doc::kCircleBrushType, 5, 0);

  FakeToolLoop evenLoop{even};
  FakeToolLoop oddLoop{odd};

  VerticalSymmetry axis(10);

  Strokes outEven;
  axis.generateStrokes(onePoint(3, 12, 0.f), outEven, &evenLoop);
  ASSERT_EQ(2u, outEven.size());
  EXPECT_EQ(8, outEven[1][0].y);

  Strokes outOdd;
  axis.generateStrokes(onePoint(3, 12, 0.f), outOdd, &oddLoop);
  ASSERT_EQ(2u, outOdd.size());
  EXPECT_EQ(7, outOdd[1][0].y);
}

TEST(Diagonal45Symmetry, ReflectsAcrossTheSlopeMinusOneDiagonal)
{
  Brush brush;
  FakeToolLoop loop{brush};
  Diagonal45Symmetry axis(10, 10);

  Strokes out;
  // A point directly "east" of the center reflects to directly "north".
  axis.generateStrokes(onePoint(14, 10, 0.f), out, &loop);

  ASSERT_EQ(2u, out.size());
  EXPECT_EQ(10, out[1][0].x);
  EXPECT_EQ(6, out[1][0].y);
}

TEST(Diagonal135Symmetry, ReflectsAcrossTheSlopePlusOneDiagonal)
{
  Brush brush;
  FakeToolLoop loop{brush};
  Diagonal135Symmetry axis(10, 10);

  Strokes out;
  axis.generateStrokes(onePoint(14, 10, 0.f), out, &loop);

  ASSERT_EQ(2u, out.size());
  EXPECT_EQ(10, out[1][0].x);
  EXPECT_EQ(14, out[1][0].y);
}

TEST(Rotational180Symmetry, RotatesAboutTheCenterWithBrushParityAdjustment)
{
  Brush odd(doc::kCircleBrushType, 5, 0);
  FakeToolLoop loop{odd};
  Rotational180Symmetry axis(10, 10);

  Strokes out;
  axis.generateStrokes(onePoint(14, 12, 0.f), out, &loop);

  ASSERT_EQ(2u, out.size());
  // 10 - (14-10+1) = 5 ; 10 - (12-10+1) = 7
  EXPECT_EQ(5, out[1][0].x);
  EXPECT_EQ(7, out[1][0].y);
}

TEST(Rotational90Symmetry, ProducesFourStrokesEachRotated90DegreesFromThePrevious)
{
  Brush brush;
  FakeToolLoop loop{brush};
  Rotational90Symmetry axis(10, 10);

  Strokes out;
  axis.generateStrokes(onePoint(14, 10, 0.75f), out, &loop);

  ASSERT_EQ(4u, out.size());
  EXPECT_EQ(14, out[0][0].x);
  EXPECT_EQ(10, out[0][0].y);
  // (dx,dy)=(4,0) -> (-0,4) -> point (10,14)
  EXPECT_EQ(10, out[1][0].x);
  EXPECT_EQ(14, out[1][0].y);
  // -> (-4,0) -> point (6,10)
  EXPECT_EQ(6, out[2][0].x);
  EXPECT_EQ(10, out[2][0].y);
  // -> (0,-4) -> point (10,6)
  EXPECT_EQ(10, out[3][0].x);
  EXPECT_EQ(6, out[3][0].y);

  for (auto& stroke : out)
    EXPECT_FLOAT_EQ(0.75f, stroke[0].pressure);

  // A 4th application returns to the starting point (full turn).
  int dx = out[3][0].x - 10, dy = out[3][0].y - 10;
  EXPECT_EQ(14 - 10, -dy);
  EXPECT_EQ(10 - 10, dx);
}

TEST(CompositeSymmetry, SingleAxisMatchesTheDedicatedSymmetryClass)
{
  Brush brush(doc::kCircleBrushType, 5, 0);
  FakeToolLoop loop{brush};

  CompositeSymmetry composite((int)app::gen::SymmetryMode::HORIZONTAL, 10, 10);
  Strokes compositeOut;
  composite.generateStrokes(onePoint(14, 3, 0.f), compositeOut, &loop);

  HorizontalSymmetry dedicated(10);
  Strokes dedicatedOut;
  dedicated.generateStrokes(onePoint(14, 3, 0.f), dedicatedOut, &loop);

  ASSERT_EQ(dedicatedOut.size(), compositeOut.size());
  for (std::size_t i = 0; i < dedicatedOut.size(); ++i) {
    EXPECT_EQ(dedicatedOut[i][0].x, compositeOut[i][0].x);
    EXPECT_EQ(dedicatedOut[i][0].y, compositeOut[i][0].y);
  }
}

TEST(CompositeSymmetry, CombiningTwoAxesYieldsFourStrokesWithNoDuplicates)
{
  Brush brush(doc::kCircleBrushType, 5, 0);
  FakeToolLoop loop{brush};

  int flags = (int)app::gen::SymmetryMode::HORIZONTAL | (int)app::gen::SymmetryMode::VERTICAL;
  CompositeSymmetry composite(flags, 10, 10);

  Strokes out;
  composite.generateStrokes(onePoint(14, 3, 0.f), out, &loop);

  ASSERT_EQ(4u, out.size());

  std::set<std::pair<int, int>> uniquePoints;
  for (auto& stroke : out)
    uniquePoints.insert({stroke[0].x, stroke[0].y});
  EXPECT_EQ(4u, uniquePoints.size());
}

TEST(CompositeSymmetry, CombiningAllFourAxesYieldsSixteenStrokesWithNoDuplicates)
{
  Brush brush(doc::kCircleBrushType, 5, 0);
  FakeToolLoop loop{brush};

  int flags = (int)app::gen::SymmetryMode::HORIZONTAL |
              (int)app::gen::SymmetryMode::VERTICAL |
              (int)app::gen::SymmetryMode::DIAGONAL_45 |
              (int)app::gen::SymmetryMode::DIAGONAL_135;
  CompositeSymmetry composite(flags, 10, 10);

  // An asymmetric starting point (not on any axis) so all 16 combinations
  // land on distinct pixels.
  Strokes out;
  composite.generateStrokes(onePoint(17, 4, 0.25f), out, &loop);

  ASSERT_EQ(16u, out.size());

  std::set<std::pair<int, int>> uniquePoints;
  for (auto& stroke : out) {
    uniquePoints.insert({stroke[0].x, stroke[0].y});
    EXPECT_FLOAT_EQ(0.25f, stroke[0].pressure);
  }
  EXPECT_EQ(16u, uniquePoints.size());
}

TEST(CompositeSymmetry, NoFlagsLeavesOnlyTheOriginalStroke)
{
  Brush brush;
  FakeToolLoop loop{brush};
  CompositeSymmetry composite(0, 10, 10);

  Strokes out;
  composite.generateStrokes(onePoint(4, 4, 0.f), out, &loop);

  ASSERT_EQ(1u, out.size());
  EXPECT_EQ(4, out[0][0].x);
  EXPECT_EQ(4, out[0][0].y);
}
