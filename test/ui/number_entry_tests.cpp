// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#define TEST_GUI
#include "tests.h"

#include "ui/message.h"
#include "ui/number_entry.h"

#include <climits>

using namespace ui;

namespace {

// NumberEntry only validates/clamps on kFocusLeaveMessage - simulate losing
// focus without needing a live Manager to actually move focus around.
void loseFocus(NumberEntry& entry)
{
  Message msg(kFocusLeaveMessage);
  entry.sendMessage(&msg);
}

} // namespace

TEST(NumberEntry, SetValueClampsToMinMax)
{
  NumberEntry entry(0, 10);

  entry.setValue(5);
  EXPECT_EQ(5, entry.getValue());

  entry.setValue(-3);
  EXPECT_EQ(0, entry.getValue());

  entry.setValue(20);
  EXPECT_EQ(10, entry.getValue());
}

TEST(NumberEntry, FocusLeaveEvaluatesAMathExpression)
{
  NumberEntry entry(0, 100);

  entry.setText("2*8");
  loseFocus(entry);
  EXPECT_EQ(16, entry.getValue());
}

TEST(NumberEntry, FocusLeaveRoundsANonIntegerResult)
{
  NumberEntry entry(0, 100);

  entry.setText("7/2");
  loseFocus(entry);
  EXPECT_EQ(4, entry.getValue()); // 3.5 rounds to 4 (std::lround)
}

TEST(NumberEntry, FocusLeaveRevertsToTheLastValidValueOnUnparseableText)
{
  NumberEntry entry(0, 100);

  entry.setValue(42);
  entry.setText("abc");
  loseFocus(entry);

  EXPECT_EQ(42, entry.getValue());
  EXPECT_EQ("42", entry.text());
}

TEST(NumberEntry, FocusLeaveClampsAnEvaluatedResultToMinMax)
{
  NumberEntry entry(0, 10);

  entry.setText("999");
  loseFocus(entry);
  EXPECT_EQ(10, entry.getValue());

  entry.setText("-999");
  loseFocus(entry);
  EXPECT_EQ(0, entry.getValue());
}

TEST(NumberEntry, FocusLeaveSaturatesRatherThanOverflowsOnAResultOutsideIntRange)
{
  // Distinct from the min/max clamp above: onProcessMessage() first clamps
  // the evaluated (long) result into [INT_MIN, INT_MAX] before narrowing it
  // to int, specifically to avoid signed-overflow UB on the narrowing cast.
  // Give the entry an [INT_MIN, INT_MAX] range so that clamp is the only
  // one in play.
  NumberEntry entry(INT_MIN, INT_MAX);

  entry.setText("99999999999"); // ~1e11, well past INT_MAX
  loseFocus(entry);
  EXPECT_EQ(INT_MAX, entry.getValue());

  entry.setText("-99999999999");
  loseFocus(entry);
  EXPECT_EQ(INT_MIN, entry.getValue());
}

TEST(NumberEntry, SetMinAndSetMaxChangeTheClampRangeForFutureValues)
{
  NumberEntry entry(0, 10);

  entry.setMin(5);
  entry.setMax(8);
  EXPECT_EQ(5, entry.min());
  EXPECT_EQ(8, entry.max());

  entry.setValue(0);
  EXPECT_EQ(5, entry.getValue());

  entry.setValue(100);
  EXPECT_EQ(8, entry.getValue());
}
