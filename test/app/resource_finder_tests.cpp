// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/resource_finder.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

using namespace app;

namespace {

// Saves an environment variable's value on construction and restores it
// (or removes it, if it was unset) on destruction.
class ScopedEnvVar {
public:
  ScopedEnvVar(const char* name, const char* value) : m_name(name) {
    const char* prev = std::getenv(name);
    if (prev)
      m_prev = prev;
    if (value)
      setenv(name, value, 1);
    else
      unsetenv(name);
  }

  ~ScopedEnvVar() {
    if (m_prev)
      setenv(m_name.c_str(), m_prev->c_str(), 1);
    else
      unsetenv(m_name.c_str());
  }

private:
  std::string m_name;
  std::optional<std::string> m_prev;
};

std::vector<std::string> collectPaths(ResourceFinder& rf) {
  std::vector<std::string> paths;
  while (rf.next())
    paths.push_back(rf.filename());
  return paths;
}

} // namespace

#if !defined(_WIN32) && !defined(__APPLE__)

TEST(ResourceFinder, IncludeUserDirUsesXdgConfigHomeWhenSet)
{
  ScopedEnvVar xdg("XDG_CONFIG_HOME", "/tmp/besprited-test-xdg");

  ResourceFinder rf(false);
  rf.includeUserDir("foo.ini");

  auto paths = collectPaths(rf);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("/tmp/besprited-test-xdg/besprited/foo.ini", paths[0]);
}

TEST(ResourceFinder, IncludeUserDirFallsBackToHomeConfigWhenXdgUnset)
{
  ScopedEnvVar xdg("XDG_CONFIG_HOME", nullptr);
  ScopedEnvVar home("HOME", "/tmp/besprited-test-home");

  ResourceFinder rf(false);
  rf.includeUserDir("foo.ini");

  auto paths = collectPaths(rf);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("/tmp/besprited-test-home/.config/besprited/foo.ini", paths[0]);
}

TEST(ResourceFinder, IncludeUserDirTreatsAnEmptyXdgConfigHomeAsUnset)
{
  ScopedEnvVar xdg("XDG_CONFIG_HOME", "");
  ScopedEnvVar home("HOME", "/tmp/besprited-test-home");

  ResourceFinder rf(false);
  rf.includeUserDir("foo.ini");

  auto paths = collectPaths(rf);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("/tmp/besprited-test-home/.config/besprited/foo.ini", paths[0]);
}

TEST(ResourceFinder, IncludeDataDirCandidateOrderWithXdgSet)
{
  ScopedEnvVar xdg("XDG_CONFIG_HOME", "/tmp/besprited-test-xdg");

  ResourceFinder rf(false);
  rf.includeDataDir("skin.png");

  auto paths = collectPaths(rf);
  ASSERT_EQ(3u, paths.size());
  // 1) $XDG_CONFIG_HOME/besprited/data/filename
  EXPECT_EQ("/tmp/besprited-test-xdg/besprited/data/skin.png", paths[0]);
  // 2) $BINDIR/data/filename
  EXPECT_TRUE(paths[1].ends_with("data/skin.png")) << paths[1];
  // 3) $BINDIR/../share/besprited/data/filename
  EXPECT_TRUE(paths[2].ends_with("../share/besprited/data/skin.png")) << paths[2];
}

TEST(ResourceFinder, IncludeDataDirCandidateOrderWithoutXdg)
{
  ScopedEnvVar xdg("XDG_CONFIG_HOME", nullptr);
  ScopedEnvVar home("HOME", "/tmp/besprited-test-home");

  ResourceFinder rf(false);
  rf.includeDataDir("skin.png");

  auto paths = collectPaths(rf);
  ASSERT_EQ(3u, paths.size());
  // 1) $HOME/.config/besprited/data/filename
  EXPECT_EQ("/tmp/besprited-test-home/.config/besprited/data/skin.png", paths[0]);
  EXPECT_TRUE(paths[1].ends_with("data/skin.png")) << paths[1];
  EXPECT_TRUE(paths[2].ends_with("../share/besprited/data/skin.png")) << paths[2];
}

#endif // !_WIN32 && !__APPLE__

TEST(ResourceFinder, NextIteratesInAdditionOrderAndStopsAtTheEnd)
{
  ResourceFinder rf(false);
  rf.addPath("a");
  rf.addPath("b");
  rf.addPath("c");

  ASSERT_TRUE(rf.next());
  EXPECT_EQ("a", rf.filename());
  ASSERT_TRUE(rf.next());
  EXPECT_EQ("b", rf.filename());
  ASSERT_TRUE(rf.next());
  EXPECT_EQ("c", rf.filename());
  EXPECT_FALSE(rf.next());
}

TEST(ResourceFinder, DefaultFilenameFallsBackToTheFirstAddedPath)
{
  ResourceFinder rf(false);
  rf.addPath("only-candidate");

  EXPECT_EQ("only-candidate", rf.defaultFilename());
}
