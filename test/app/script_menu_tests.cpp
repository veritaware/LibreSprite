// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#define TEST_GUI
#include "tests.h"

#include "app/commands/command.h"
#include "app/file_system.h"
#include "app/ui/app_menuitem.h"
#include "base/fs.h"
#include "base/path.h"
#include "ui/menu.h"

#include <fstream>

// scanFolder() has external linkage (defined directly in `namespace app`,
// not file-local) but isn't declared in script_menu.h - it's only reachable
// through ScriptMenu::rebuildScriptsList(), which additionally needs a real
// CommandsModule::instance() and touches the user's actual scripts config
// directory via ResourceFinder. Declaring its existing signature here
// reaches the directory-scanning logic directly and headlessly, without
// either of those.
namespace app {
  void scanFolder(const std::string& scriptsDir, Command* cmd_run_script, ui::Menu* parent);
}

using namespace app;

namespace {

// FileSystemModule::instance() backs FileSystemModule::getFileItemFromPath()
// (used by scanFolder()) - a standalone singleton like CommandsModule/App in
// the other test files in this issue, so likewise constructed at most once
// and deliberately leaked for the whole binary.
void ensureFileSystemModule()
{
  static app::FileSystemModule* fs = new app::FileSystemModule();
  (void)fs;
}

struct ScriptMenuFixture : public ::testing::Test {
  std::string dir;
  Command runScriptCmd{"RunScript", "Run Script", CmdRecordableFlag};
  ui::Menu menu;

  static void SetUpTestSuite() { ensureFileSystemModule(); }

  void SetUp() override {
    dir = "script_menu_test_scripts";
    if (base::is_directory(dir))
      removeDirRecursive(dir);
    base::make_all_directories(dir);
  }

  void TearDown() override {
    removeDirRecursive(dir);
  }

  void removeDirRecursive(const std::string& d)
  {
    if (!base::is_directory(d))
      return;
    for (const auto& entry : base::list_files(d)) {
      std::string full = base::join_path(d, entry);
      if (base::is_directory(full))
        removeDirRecursive(full);
      else
        base::delete_file(full);
    }
    base::remove_directory(d);
  }

  void writeFile(const std::string& relPath, const std::string& content = "// a script\n")
  {
    std::ofstream out(base::join_path(dir, relPath), std::ios::binary);
    out << content;
  }

  // Refreshes FileSystemModule's cached view of the directory tree so
  // freshly-written fixture files are actually picked up - it caches
  // FileItem children until told otherwise (see FileSystemModule::refresh()).
  void refreshFs()
  {
    FileSystemModule::instance()->refresh();
  }

  AppMenuItem* findChild(const std::string& text)
  {
    for (auto* w : menu.children()) {
      if (auto* item = dynamic_cast<AppMenuItem*>(w)) {
        if (item->text() == text)
          return item;
      }
    }
    return nullptr;
  }
};

} // namespace

TEST_F(ScriptMenuFixture, ScriptFilesBecomeMenuItemsRunningTheGivenCommand)
{
  writeFile("one.js");
  writeFile("two.js");
  refreshFs();

  scanFolder(dir, &runScriptCmd, &menu);

  AppMenuItem* one = findChild("one.js");
  ASSERT_NE(nullptr, one);
  EXPECT_EQ(&runScriptCmd, one->getCommand());
  EXPECT_EQ(base::join_path(dir, "one.js"), one->getParams().get("filename"));
  EXPECT_EQ(nullptr, one->getSubmenu());

  EXPECT_NE(nullptr, findChild("two.js"));
}

TEST_F(ScriptMenuFixture, NonScriptFilesAreIgnored)
{
  writeFile("one.js");
  writeFile("notes.txt");
  writeFile("readme.md");
  refreshFs();

  scanFolder(dir, &runScriptCmd, &menu);

  EXPECT_NE(nullptr, findChild("one.js"));
  EXPECT_EQ(nullptr, findChild("notes.txt"));
  EXPECT_EQ(nullptr, findChild("readme.md"));
  EXPECT_EQ(1, menu.children().size());
}

TEST_F(ScriptMenuFixture, SubfoldersBecomeSubmenusContainingTheirOwnScripts)
{
  writeFile("top.js");
  base::make_all_directories(base::join_path(dir, "sub"));
  writeFile("sub/nested.js");
  refreshFs();

  scanFolder(dir, &runScriptCmd, &menu);

  AppMenuItem* sub = findChild("sub");
  ASSERT_NE(nullptr, sub);
  EXPECT_EQ(nullptr, sub->getCommand()) << "a folder item doesn't run a script itself";

  ui::Menu* submenu = sub->getSubmenu();
  ASSERT_NE(nullptr, submenu);

  AppMenuItem* nested = nullptr;
  for (auto* w : submenu->children()) {
    if (auto* item = dynamic_cast<AppMenuItem*>(w); item && item->text() == "nested.js")
      nested = item;
  }
  ASSERT_NE(nullptr, nested);
  EXPECT_EQ(&runScriptCmd, nested->getCommand());
  EXPECT_EQ(base::join_path(base::join_path(dir, "sub"), "nested.js"),
            nested->getParams().get("filename"));
}

TEST_F(ScriptMenuFixture, AnEmptyDirectoryProducesNoMenuItems)
{
  refreshFs();

  scanFolder(dir, &runScriptCmd, &menu);

  EXPECT_EQ(0, menu.children().size());
}
