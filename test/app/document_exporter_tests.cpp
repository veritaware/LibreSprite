// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/context.h"
#include "app/document.h"
#include "app/document_exporter.h"
#include "base/fs.h"
#include "doc/doc.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace app;
using namespace doc;

namespace {

// Reads the whole file back into a string - exportSheet() only writes to a
// std::cout/UIContext-driven stream when setDataFilename() is left empty, so
// tests give it a real path instead and read it back afterwards.
std::string readWholeFile(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Every "frame" entry in the exporter's JSON is written as
//   "<filename>": {\n    "frame": { "x": N, "y": N, "w": N, "h": N },\n
// so a laid-out sample's exact bounds can be checked with one substring
// lookup, no JSON parser needed.
::testing::AssertionResult hasFrameBounds(const std::string& json, const std::string& filename,
                                           int x, int y, int w, int h)
{
  std::ostringstream expected;
  expected << "\"" << filename << "\": {\n"
           << "    \"frame\": { \"x\": " << x << ", \"y\": " << y
           << ", \"w\": " << w << ", \"h\": " << h << " },\n";
  if (json.find(expected.str()) != std::string::npos)
    return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure()
    << "expected to find " << expected.str() << "in:\n" << json;
}

} // namespace

TEST(DocumentExporter, PerTagLayoutProducesOneRowPerTagWithCorrectInTextureBounds)
{
  app::Context ctx;
  doc::Document* doc = ctx.documents().add(4, 4, doc::ColorMode::RGB);
  doc->setFilename("spr.png");
  Sprite* sprite = doc->sprite();
  sprite->setTotalFrames(frame_t(4));

  FrameTag* tagA = new FrameTag(frame_t(0), frame_t(1));
  tagA->setName("TagA");
  sprite->frameTags().add(tagA);

  FrameTag* tagB = new FrameTag(frame_t(2), frame_t(3));
  tagB->setName("TagB");
  sprite->frameTags().add(tagB);

  const std::string dataFile = "document_exporter_per_tag_test.json";
  if (base::is_file(dataFile))
    base::delete_file(dataFile);

  DocumentExporter exporter;
  exporter.setDataFilename(dataFile);
  exporter.setDataFormat(DocumentExporter::JsonHashDataFormat);
  exporter.setSpriteSheetType(SpriteSheetType::Columns);
  exporter.setPerTag(true);
  exporter.setListFrameTags(true);
  exporter.setListLayers(true);

  // Mirrors cmd_export_sprite_sheet.cpp's per-tag setup: a first "dummy"
  // addDocument() covering the whole selected range so captureSamples() can
  // detect the frame sequence, followed by one addDocument() per frame tag
  // that actually overlaps it.
  auto* appDoc = static_cast<app::Document*>(doc);
  exporter.addDocument(appDoc, nullptr, nullptr, false);
  exporter.addDocument(appDoc, nullptr, tagA, false);
  exporter.addDocument(appDoc, nullptr, tagB, false);

  std::unique_ptr<app::Document> texture(exporter.exportSheet());
  ASSERT_NE(nullptr, texture);

  std::string json = readWholeFile(dataFile);
  ASSERT_FALSE(json.empty());

  // Each tag's two frames are laid out side by side (SpriteSheetType::Columns
  // advances a new tag to a new row, and advances frames within a tag along
  // that row) - TagA's row starts at y=0, TagB's at y=4 (the sprite height).
  EXPECT_TRUE(hasFrameBounds(json, "spr #TagA 0.png", 0, 0, 4, 4));
  EXPECT_TRUE(hasFrameBounds(json, "spr #TagA 1.png", 4, 0, 4, 4));
  EXPECT_TRUE(hasFrameBounds(json, "spr #TagB 0.png", 0, 4, 4, 4));
  EXPECT_TRUE(hasFrameBounds(json, "spr #TagB 1.png", 4, 4, 4, 4));

  // The "dummy" first addDocument()'s 4 samples exist only so
  // captureSamples() can detect bframe/eframe - PerTagLayoutSamples flags
  // all of them as duplicated and never positions them, so they keep their
  // SampleBounds-constructor default (0,0,<sprite size>) instead of a real
  // slot in the sheet.
  for (int frame = 0; frame < 4; ++frame) {
    std::ostringstream name;
    name << "spr " << frame << ".png";
    EXPECT_TRUE(hasFrameBounds(json, name.str(), 0, 0, 4, 4))
      << "duplicated dummy sample for frame " << frame
      << " should be left at its unpositioned default bounds";
  }

  EXPECT_NE(std::string::npos, json.find("\"frameTags\": ["));
  EXPECT_NE(std::string::npos, json.find("\"name\": \"TagA\""));
  EXPECT_NE(std::string::npos, json.find("\"name\": \"TagB\""));
  EXPECT_NE(std::string::npos, json.find("\"layers\": ["));

  if (base::is_file(dataFile))
    base::delete_file(dataFile);
}
