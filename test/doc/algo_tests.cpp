// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tests.h"

#include "doc/algo.h"

#include <vector>

using namespace doc;

namespace {

std::vector<std::pair<int, int>> collectLine(int x1, int y1, int x2, int y2)
{
  std::vector<std::pair<int, int>> points;
  algo_line(x1, y1, x2, y2, [&](int x, int y) { points.emplace_back(x, y); });
  return points;
}

struct FloatPoint {
  int x, y;
  float f;
};

std::vector<FloatPoint> collectLineFloat(int x1, int y1, int x2, int y2)
{
  std::vector<FloatPoint> points;
  algo_line_float(x1, y1, x2, y2, [&](int x, int y, float f) {
    points.push_back({x, y, f});
  });
  return points;
}

} // namespace

TEST(AlgoLineFloat, MatchesAlgoLinesPixelSetInEveryOctant)
{
  // One representative line per octant, both endpoints reachable from
  // either direction, plus a couple of pure horizontal/vertical/diagonal
  // cases.
  const std::vector<std::tuple<int, int, int, int>> lines = {
    {0, 0, 6, 2},   // shallow, +x +y
    {0, 0, 2, 6},   // steep,   +x +y
    {0, 0, 6, -2},  // shallow, +x -y
    {0, 0, 2, -6},  // steep,   +x -y
    {0, 0, -6, 2},  // shallow, -x +y
    {0, 0, -2, 6},  // steep,   -x +y
    {0, 0, -6, -2}, // shallow, -x -y
    {0, 0, -2, -6}, // steep,   -x -y
    {0, 0, 5, 0},   // horizontal
    {0, 0, 0, 5},   // vertical
    {0, 0, 4, 4},   // pure diagonal
    {6, 2, 0, 0},   // reversed shallow
  };

  for (auto& [x1, y1, x2, y2] : lines) {
    auto intPoints = collectLine(x1, y1, x2, y2);
    auto floatPoints = collectLineFloat(x1, y1, x2, y2);

    ASSERT_EQ(intPoints.size(), floatPoints.size())
      << "line (" << x1 << "," << y1 << ")-(" << x2 << "," << y2 << ")";

    for (std::size_t i = 0; i < intPoints.size(); ++i) {
      EXPECT_EQ(intPoints[i].first, floatPoints[i].x)
        << "point " << i << " of line (" << x1 << "," << y1 << ")-(" << x2 << "," << y2 << ")";
      EXPECT_EQ(intPoints[i].second, floatPoints[i].y)
        << "point " << i << " of line (" << x1 << "," << y1 << ")-(" << x2 << "," << y2 << ")";
    }
  }
}

TEST(AlgoLineFloat, FRunsMonotonicallyFromNearZeroToJustOverOne)
{
  auto points = collectLineFloat(0, 0, 8, 3);
  ASSERT_GE(points.size(), 2u);

  for (std::size_t i = 1; i < points.size(); ++i)
    EXPECT_GT(points[i].f, points[i - 1].f) << "f must strictly increase at step " << i;

  // First step is one unit of the parametric step (≈ 1/n); last step
  // overshoots by that same one unit past the far endpoint (the loop counts
  // n+1 points across n steps of size 1/n).
  EXPECT_NEAR(points.front().f, points[1].f - points[0].f, 1e-4f);
  EXPECT_GT(points.back().f, 1.0f);
  EXPECT_LT(points.back().f, 1.0f + 2.0f * (points[1].f - points[0].f));
}

TEST(AlgoLineFloat, FIncreasesRegardlessOfWhichOctantOrDirection)
{
  const std::vector<std::tuple<int, int, int, int>> lines = {
    {0, 0, 5, 0}, {5, 0, 0, 0},
    {0, 0, 0, 5}, {0, 5, 0, 0},
    {0, 0, -5, -5}, {-5, -5, 0, 0},
    {0, 0, -1, -4}, {0, 0, 2, -5},
  };
  for (auto& [x1, y1, x2, y2] : lines) {
    auto points = collectLineFloat(x1, y1, x2, y2);
    for (std::size_t i = 1; i < points.size(); ++i) {
      EXPECT_GT(points[i].f, points[i - 1].f)
        << "line (" << x1 << "," << y1 << ")-(" << x2 << "," << y2 << ") step " << i;
    }
  }
}

TEST(AlgoLineFloat, ZeroLengthLineCallsBackExactlyOnce)
{
  auto points = collectLineFloat(3, 3, 3, 3);
  ASSERT_EQ(1u, points.size());
  EXPECT_EQ(3, points[0].x);
  EXPECT_EQ(3, points[0].y);
  EXPECT_FLOAT_EQ(0.0f, points[0].f);
}

TEST(AlgoLineFloat, TemplateOverloadForwardsToTheCallback)
{
  int calls = 0;
  algo_line_float(0, 0, 3, 0, [&](int x, int y, float f) {
    (void)x; (void)y; (void)f;
    ++calls;
  });
  EXPECT_EQ(4, calls); // x = 0,1,2,3
}
