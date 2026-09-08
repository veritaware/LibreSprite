// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/context.h"
#include "app/crash/read_document.h"
#include "app/crash/write_document.h"
#include "app/document.h"
#include "base/fs.h"
#include "base/path.h"
#include "doc/doc.h"

using namespace app;

namespace {

// crash::write_document() populates the directory with several files
// (cel-*, celdata-*, doc-*, img-*, lay-*, pal-*, spr-*) - base::remove_directory()
// only removes an empty directory, so a fixture dir left over from a
// previous write needs its contents cleared out first.
void removeDirRecursive(const std::string& dir)
{
  if (!base::is_directory(dir))
    return;
  for (const auto& entry : base::list_files(dir))
    base::delete_file(base::join_path(dir, entry));
  base::remove_directory(dir);
}

} // namespace

TEST(ReadDocumentInfo, NonSquareCanvasWidthAndHeightAreNotSwapped)
{
  // Regression test for 88b816476: read_document_info() used to write the
  // sprite's width into both m_loadInfo->width and m_loadInfo->height
  // (`m_loadInfo->height = w;` instead of `= h;`), so any crash-recovery
  // info read back for a non-square canvas silently reported a square one.
  const int w = 20, h = 10;
  ASSERT_NE(w, h) << "test setup: width and height must differ to catch a swap/duplication bug";

  app::Context ctx;
  doc::Document* doc = ctx.documents().add(w, h, doc::ColorMode::RGB);
  doc->setFilename("read_document_info_test.ase");

  const std::string dir = "read_document_info_test_dir";
  removeDirRecursive(dir);
  base::make_all_directories(dir);

  crash::write_document(dir, static_cast<app::Document*>(doc));

  crash::DocumentInfo info;
  ASSERT_TRUE(crash::read_document_info(dir, info));
  EXPECT_EQ(w, info.width);
  EXPECT_EQ(h, info.height);

  doc->close();
  delete doc;
  removeDirRecursive(dir);
}

TEST(ReadDocumentInfo, SquareCanvasStillRoundTripsCorrectly)
{
  // Companion to the non-square test above: a square canvas can't reveal
  // a width/height swap on its own (which is exactly why the regression
  // needs a non-square fixture), but it's worth pinning too.
  const int size = 15;

  app::Context ctx;
  doc::Document* doc = ctx.documents().add(size, size, doc::ColorMode::INDEXED, 4);
  doc->setFilename("read_document_info_square_test.ase");

  const std::string dir = "read_document_info_square_test_dir";
  removeDirRecursive(dir);
  base::make_all_directories(dir);

  crash::write_document(dir, static_cast<app::Document*>(doc));

  crash::DocumentInfo info;
  ASSERT_TRUE(crash::read_document_info(dir, info));
  EXPECT_EQ(size, info.width);
  EXPECT_EQ(size, info.height);

  doc->close();
  delete doc;
  removeDirRecursive(dir);
}

TEST(ReadDocumentInfo, ReturnsFalseForADirectoryWithNoBackup)
{
  const std::string dir = "read_document_info_empty_test_dir";
  removeDirRecursive(dir);
  base::make_all_directories(dir);

  crash::DocumentInfo info;
  EXPECT_FALSE(crash::read_document_info(dir, info));

  removeDirRecursive(dir);
}
