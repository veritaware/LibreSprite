// Aseprite Base Library
// Aseprite  | Copyright (C) 2001-2015 David Capello
// Besprited | Copyright (C) 2026      Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "base/fs.h"
#include "base/path.h"

using namespace base;

TEST(FileSystem, MakeDirectory)
{
  EXPECT_FALSE(is_directory("a"));

  make_directory("a");
  EXPECT_TRUE(is_directory("a"));

  remove_directory("a");
  EXPECT_FALSE(is_directory("a"));
}

TEST(FileSystem, MakeAllDirectories)
{
  EXPECT_FALSE(is_directory("a"));
  EXPECT_FALSE(is_directory("a/b"));
  EXPECT_FALSE(is_directory("a/b/c"));

  make_all_directories("a/b/c");

  EXPECT_TRUE(is_directory("a"));
  EXPECT_TRUE(is_directory("a/b"));
  EXPECT_TRUE(is_directory("a/b/c"));

  remove_directory("a/b/c");
  EXPECT_FALSE(is_directory("a/b/c"));

  remove_directory("a/b");
  EXPECT_FALSE(is_directory("a/b"));

  remove_directory("a");
  EXPECT_FALSE(is_directory("a"));
}

TEST(FileSystem, ListFilesOnAnEmptyDirectory)
{
  make_directory("list_a");
  auto files = list_files("list_a");
  EXPECT_TRUE(files.empty());
  remove_directory("list_a");
}

TEST(FileSystem, ListFilesExcludesDotAndDotDot)
{
  make_all_directories("list_b/child");
  auto files = list_files("list_b");

  ASSERT_EQ(1u, files.size());
  EXPECT_EQ("child", files[0]);
  for (auto& name : files) {
    EXPECT_NE(".", name);
    EXPECT_NE("..", name);
  }

  remove_directory("list_b/child");
  remove_directory("list_b");
}

TEST(FileSystem, ListFilesOnAMissingDirectoryReturnsEmpty)
{
  EXPECT_FALSE(is_directory("does_not_exist_dir"));
  auto files = list_files("does_not_exist_dir");
  EXPECT_TRUE(files.empty());
}

TEST(FileSystem, GetCanonicalPathResolvesTheCurrentDirectory)
{
  make_directory("canon_dir");

  auto canonical = get_canonical_path("canon_dir");

  // Must be turned into an absolute path.
  EXPECT_TRUE(is_path_separator(canonical.front()) ||
              (canonical.size() > 1 && canonical[1] == ':'));
  EXPECT_TRUE(is_directory(canonical));

  remove_directory("canon_dir");
}

TEST(FileSystem, GetCanonicalPathOnAMissingPathReturnsItUnchanged)
{
  std::string missing = "this_path_does_not_exist_at_all";
  EXPECT_EQ(missing, get_canonical_path(missing));
}

TEST(FileSystem, GetFontPathsIsNonEmpty)
{
  auto paths = get_font_paths();
  EXPECT_FALSE(paths.empty());
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
