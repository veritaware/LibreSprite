// Aseprite  | Copyright (C) 2001-2015 David Capello
// Besprited | Copyright (C)      2026 Veritaware
//
// This file is released under the terms of the GNU General Public License
// version 2 as published by the Free Software Foundation.

#pragma once

// Shared entry point for tests that need more setup than GTest::gtest_main
// provides. Historically included as "tests/test.h".
//
// Define TEST_GUI before including this header to pull in the she/ui stack
// and spin up a minimal ui::UISystem / ui::Manager around the test run.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtest/gtest.h>

#ifdef TEST_GUI
  #include "she/she.h"
  #include "ui/ui.h"
#endif

#ifdef LINKED_WITH_SHE
  // she's platform layer provides the real main() and calls app_main().
  #undef main
  #ifdef _WIN32
    int main(int argc, char* argv[]) {
      extern int app_main(int argc, char* argv[]);
      return app_main(argc, argv);
    }
  #endif
  #define main app_main
#endif

int main(int argc, char* argv[])
{
  int exitcode;
  ::testing::InitGoogleTest(&argc, argv);

  #ifdef TEST_GUI
    {
      // Do not create a she::System, as we don't need it for testing purposes.
      ui::UISystem uiSystem;
      ui::Manager uiManager;
  #endif

      exitcode = RUN_ALL_TESTS();

  #ifdef TEST_GUI
    }
  #endif

  return exitcode;
}
