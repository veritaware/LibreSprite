// Aseprite  | Copyright (C) 2001-2015 David Capello
// Besprited | Copyright (C) 2026      Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/ini_file.h"
#include "base/fs.h"

using namespace app;

TEST(IniFile, Basic)
{
  if (base::is_file("_test.ini"))
    base::delete_file("_test.ini");

  set_config_file("_test.ini");

  EXPECT_FALSE(get_config_bool("A", "a", false));
  EXPECT_TRUE(get_config_bool("A", "b", true));
  EXPECT_FALSE(get_config_bool("B", "a", 0));
  EXPECT_TRUE(get_config_bool("B", "b", 1));

  set_config_bool("A", "a", true);
  set_config_bool("A", "b", false);
  set_config_int("B", "a", 2);
  set_config_int("B", "b", 3);

  EXPECT_TRUE(get_config_bool("A", "a", false));
  EXPECT_FALSE(get_config_bool("A", "b", true));
  EXPECT_EQ(2, get_config_int("B", "a", 0));
  EXPECT_EQ(3, get_config_int("B", "b", 1));
}

TEST(IniFile, HasConfigValue)
{
  // Regression test: has_config_value() used to return true for an *absent*
  // key (its body checked `== nullptr` instead of `!= nullptr`), silently
  // inverting the meaning its own name promises and the meaning every call
  // site (main_window.cpp's DPI-default guards) already assumed.
  if (base::is_file("_has_config_value.ini"))
    base::delete_file("_has_config_value.ini");

  // set_config_file() re-loads into whatever CfgFile is already on top of
  // the stack without clearing it first (real callers, e.g.
  // Preferences::serializeDocPref(), always pair it with a preceding
  // push_config_state() for exactly this reason - it hands out a brand new
  // CfgFile to load into); push here too, so this test doesn't inherit
  // leftover keys from another test's config file.
  push_config_state();
  set_config_file("_has_config_value.ini");

  EXPECT_FALSE(has_config_value("A", "a"));

  set_config_bool("A", "a", true);
  EXPECT_TRUE(has_config_value("A", "a"));

  // A saved empty string is still a *present* value.
  set_config_string("A", "b", "");
  EXPECT_TRUE(has_config_value("A", "b"));

  // A different key/section than what was set must still read as absent.
  EXPECT_FALSE(has_config_value("A", "c"));
  EXPECT_FALSE(has_config_value("B", "a"));

  pop_config_state();
}

TEST(IniFile, PushPop)
{
  if (base::is_file("_a.ini")) base::delete_file("_a.ini");
  if (base::is_file("_b.ini")) base::delete_file("_b.ini");

  set_config_file("_a.ini");
  set_config_int("A", "a", 32);

  push_config_state();
  set_config_file("_b.ini");
  set_config_int("A", "a", 64);
  EXPECT_EQ(64, get_config_int("A", "a", 0));
  pop_config_state();

  EXPECT_EQ(32, get_config_int("A", "a", 0));
}
