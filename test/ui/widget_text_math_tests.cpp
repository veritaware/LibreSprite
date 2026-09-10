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
// overridable onEvalError() hook - the default implementation does nothing,
// so tests override it to observe that the failure was detected at all.
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

// A widget that has something better to fall back on than zero, the way
// ui::Entry does with the last text it held that evaluated.
class FallbackWidget : public RecordingWidget {
public:
  double fallback = 0.0;

protected:
  double onEvalFallback() const override { return fallback; }
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

TEST(WidgetTextMath, TextIntClampsAResultOutsideIntRangeWithoutOverflow)
{
  RecordingWidget w;
  w.setText("99999999999"); // ~1e11, past INT_MAX
  EXPECT_EQ(INT_MAX, w.textInt());
  EXPECT_EQ(0, w.errorCount);

  w.setText("-99999999999");
  EXPECT_EQ(INT_MIN, w.textInt());
}

// Clamping has to happen in double space; converting an out-of-range
// double straight to an integer type is undefined behaviour.
TEST(WidgetTextMath, HugeResultClampsWithoutUndefinedConversion)
{
  RecordingWidget w;
  w.setText("9999999999*9999999999"); // ~1e20, past LONG_MAX
  EXPECT_EQ(INT_MAX, w.textInt());
  EXPECT_EQ(0, w.errorCount);

  w.setText("-9999999999*9999999999");
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

// Text that doesn't evaluate never invents a number: a plain widget has
// nothing to fall back on, so it reads as zero.
TEST(WidgetTextMath, UnparseableTextReadsAsTheFallbackValue)
{
  RecordingWidget w;
  w.setText("not a number");
  EXPECT_EQ(0, w.textInt());
  EXPECT_EQ(1, w.errorCount);
  EXPECT_FALSE(w.lastError.empty());

  EXPECT_DOUBLE_EQ(0.0, w.textDouble());
}

TEST(WidgetTextMath, UnparseableTextUsesTheWidgetsOwnFallback)
{
  FallbackWidget w;
  w.fallback = 300.0;

  w.setText("-"); // start of a negative number
  EXPECT_EQ(300, w.textInt());
  EXPECT_DOUBLE_EQ(300.0, w.textDouble());
  EXPECT_EQ(2, w.errorCount);

  w.setText("-25"); // ...finished
  EXPECT_EQ(-25, w.textInt());
  EXPECT_EQ(2, w.errorCount);
}

// A result that parses but isn't a usable number falls back too. evalmath
// rejects most division by zero itself, so this guards the rest.
TEST(WidgetTextMath, NonFiniteResultIsTreatedAsAFailedEvaluation)
{
  FallbackWidget w;
  w.fallback = 10.0;

  w.setText("1/0");
  EXPECT_EQ(10, w.textInt());
  EXPECT_EQ(1, w.errorCount);

  w.setText("0/0");
  EXPECT_EQ(10, w.textInt());
}

// isTextReadAsNumber() is what tells an Entry it is a numeric field at
// all, so it must stay false for text nothing reads as a number.
TEST(WidgetTextMath, IsTextReadAsNumberStaysFalseUntilTextIsReadAsANumber)
{
  RecordingWidget w;
  w.setText("123");
  EXPECT_FALSE(w.isTextReadAsNumber());
  EXPECT_EQ(123, w.textInt());
  EXPECT_TRUE(w.isTextReadAsNumber());
}

// It is set even when the read fails - the read is what makes the widget
// numeric, not whether that particular text happened to parse.
TEST(WidgetTextMath, IsTextReadAsNumberIsSetEvenWhenTheTextDoesNotParse)
{
  RecordingWidget w;
  w.setText("-");
  EXPECT_EQ(0, w.textInt());
  EXPECT_TRUE(w.isTextReadAsNumber());
}
