// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#define TEST_GUI
#include "tests.h"

#include "ui/widget.h"

#include <climits>
#include <string>

using namespace ui;

namespace {

// Widget::textInt()/textDouble() report a parse failure through the
// overridable onEvalError() hook - the default implementation blocks on a
// ui::Alert, which needs a live, message-pumping ui::Manager, so tests
// override it instead to record the failure headlessly.
class RecordingWidget : public Widget {
public:
  mutable int errorCount = 0;
  mutable std::string lastError;

protected:
  void onEvalError(const std::string& message) const override
  {
    ++errorCount;
    lastError = message;
  }
};

} // namespace

TEST(WidgetTextMath, TextIntParsesAPlainNumber)
{
  RecordingWidget w;
  w.setText("42");
  EXPECT_EQ(42, w.textInt());
  EXPECT_EQ(0, w.errorCount);
}

TEST(WidgetTextMath, TextIntEvaluatesAnExpression)
{
  RecordingWidget w;
  w.setText("3*4+5");
  EXPECT_EQ(17, w.textInt());
  EXPECT_EQ(0, w.errorCount);
}

TEST(WidgetTextMath, TextIntOnUnparseableTextReportsTheErrorAndReturnsOne)
{
  RecordingWidget w;
  w.setText("not a number");
  EXPECT_EQ(1, w.textInt());
  EXPECT_EQ(1, w.errorCount);
  EXPECT_FALSE(w.lastError.empty());
}

TEST(WidgetTextMath, TextIntClampsAResultOutsideIntRangeWithoutOverflow)
{
  RecordingWidget w;
  w.setText("99999999999"); // ~1e11, past INT_MAX
  EXPECT_EQ(INT_MAX, w.textInt());
  EXPECT_EQ(0, w.errorCount);

  w.setText("-99999999999");
  EXPECT_EQ(INT_MIN, w.textInt());
}

TEST(WidgetTextMath, TextDoubleParsesAPlainNumber)
{
  RecordingWidget w;
  w.setText("3.5");
  EXPECT_DOUBLE_EQ(3.5, w.textDouble());
  EXPECT_EQ(0, w.errorCount);
}

TEST(WidgetTextMath, TextDoubleRoundsToThreeDecimals)
{
  RecordingWidget w;
  w.setText("1/3");
  EXPECT_DOUBLE_EQ(0.333, w.textDouble());
}

TEST(WidgetTextMath, TextDoubleOnUnparseableTextReportsTheErrorAndReturnsOne)
{
  RecordingWidget w;
  w.setText("???");
  EXPECT_DOUBLE_EQ(1.0, w.textDouble());
  EXPECT_EQ(1, w.errorCount);
}

TEST(WidgetTextMath, ScopedEvalErrorSilenceSwallowsTheErrorButKeepsTheFallback)
{
  RecordingWidget w;
  w.setText("-"); // a half-typed negative number

  {
    Widget::ScopedEvalErrorSilence silence;
    EXPECT_EQ(1, w.textInt());
    EXPECT_DOUBLE_EQ(1.0, w.textDouble());
    EXPECT_EQ(0, w.errorCount);
  }

  // Once the guard is gone the error surfaces again (e.g. on commit).
  EXPECT_EQ(1, w.textInt());
  EXPECT_EQ(1, w.errorCount);
}

TEST(WidgetTextMath, ScopedEvalErrorSilenceNestsAndRestores)
{
  RecordingWidget w;
  w.setText("not a number");

  {
    Widget::ScopedEvalErrorSilence outer;
    {
      Widget::ScopedEvalErrorSilence inner;
      EXPECT_TRUE(Widget::isEvalErrorSilenced());
      w.textInt();
    }
    EXPECT_TRUE(Widget::isEvalErrorSilenced());
    w.textInt();
  }
  EXPECT_FALSE(Widget::isEvalErrorSilenced());
  EXPECT_EQ(0, w.errorCount);
}
