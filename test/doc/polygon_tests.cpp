// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tests.h"

#include "doc/algorithm/polygon.h"

#include <vector>

using namespace doc;

namespace {

struct Span { int y, x1, x2; };

bool operator==(const Span& a, const Span& b)
{
  return a.y == b.y && a.x1 == b.x1 && a.x2 == b.x2;
}

std::vector<Span> fillPolygon(const std::vector<int>& xy)
{
  std::vector<Span> spans;
  algorithm::polygon(
    static_cast<int>(xy.size() / 2), xy.data(), 2, &spans,
    [](int x1, int y, int x2, void* data) {
      static_cast<std::vector<Span>*>(data)->push_back({y, x1, x2});
    });
  return spans;
}

} // namespace

TEST(Polygon, RightTriangleScanlineSpans)
{
  // (0,0) - (4,0) - (0,4): a right triangle whose hypotenuse shrinks the
  // span by one pixel per row.
  auto spans = fillPolygon({0, 0, 4, 0, 0, 4});

  ASSERT_EQ(5u, spans.size());
  EXPECT_EQ((Span{0, 0, 4}), spans[0]);
  EXPECT_EQ((Span{1, 0, 3}), spans[1]);
  EXPECT_EQ((Span{2, 0, 2}), spans[2]);
  EXPECT_EQ((Span{3, 0, 1}), spans[3]);
  EXPECT_EQ((Span{4, 0, 0}), spans[4]);
}

TEST(Polygon, AxisAlignedSquareFillsEveryRowFully)
{
  auto spans = fillPolygon({0, 0, 4, 0, 4, 4, 0, 4});

  ASSERT_EQ(5u, spans.size());
  for (auto& s : spans)
    EXPECT_EQ((Span{s.y, 0, 4}), s);
}

TEST(Polygon, HorizontalEdgesAreSkippedButStillFillCorrectly)
{
  // A single-row rectangle: top and bottom edges are horizontal (skipped
  // outright), only the two vertical edges contribute intersections.
  auto spans = fillPolygon({0, 0, 4, 0, 4, 1, 0, 1});

  ASSERT_EQ(2u, spans.size());
  EXPECT_EQ((Span{0, 0, 4}), spans[0]);
  EXPECT_EQ((Span{1, 0, 4}), spans[1]);
}

TEST(Polygon, ConcaveUShapeProducesTwoSpansPerNotchedRow)
{
  // A "U": two legs (x in [0,1] and [3,4]) for y in [0,3], joined by a base
  // for y in [4,5]. The notch must yield two separate spans, not one.
  auto spans = fillPolygon({
    0, 0, 1, 0, 1, 4, 3, 4, 3, 0, 4, 0, 4, 5, 0, 5
  });

  auto spansForRow = [&](int y) {
    std::vector<Span> row;
    for (auto& s : spans)
      if (s.y == y)
        row.push_back(s);
    return row;
  };

  for (int y = 0; y <= 3; ++y) {
    auto row = spansForRow(y);
    ASSERT_EQ(2u, row.size()) << "row " << y;
    EXPECT_EQ((Span{y, 0, 1}), row[0]);
    EXPECT_EQ((Span{y, 3, 4}), row[1]);
  }

  for (int y = 4; y <= 5; ++y) {
    auto row = spansForRow(y);
    ASSERT_EQ(1u, row.size()) << "row " << y;
    EXPECT_EQ((Span{y, 0, 4}), row[0]);
  }
}

TEST(Polygon, EmptyPolygonProducesNoSpans)
{
  auto spans = fillPolygon({});
  EXPECT_TRUE(spans.empty());
}

TEST(Polygon, ContainerOverloadMatchesThePointerOverload)
{
  struct Point { int x, y; };
  std::vector<Point> triangle = {{0, 0}, {4, 0}, {0, 4}};

  std::vector<Span> spans;
  algorithm::polygon(triangle, [&](int x1, int y, int x2) {
    spans.push_back({y, x1, x2});
  });

  auto expected = fillPolygon({0, 0, 4, 0, 0, 4});
  ASSERT_EQ(expected.size(), spans.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(expected[i], spans[i]);
}
