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

// A widget nothing has ever evaluated successfully has no value to fall
// back on, so unparseable text reads as 0 rather than as some invented
// number.
TEST(WidgetTextMath, UnparseableTextWithNoPriorValueReadsAsZero)
{
  RecordingWidget w;
  w.setText("not a number");
  EXPECT_EQ(0, w.textInt());
  EXPECT_EQ(1, w.errorCount);
  EXPECT_FALSE(w.lastError.empty());

  EXPECT_DOUBLE_EQ(0.0, w.textDouble());
}

// The half-typed states an expression passes through while it is being
// edited must not move the value a live preview is reading.
TEST(WidgetTextMath, HalfTypedExpressionKeepsTheLastValueThatParsed)
{
  RecordingWidget w;

  w.setText("300");
  EXPECT_EQ(300, w.textInt());

  w.setText("-"); // start of a negative number
  EXPECT_EQ(300, w.textInt());
  EXPECT_EQ(1, w.errorCount);

  w.setText("-1"); // ...finished
  EXPECT_EQ(-1, w.textInt());
  EXPECT_EQ(1, w.errorCount);

  w.setText("-1*"); // trailing operator
  EXPECT_EQ(-1, w.textInt());
}

TEST(WidgetTextMath, LastEvalTextTracksTheTextThatParsed)
{
  RecordingWidget w;
  EXPECT_FALSE(w.hasLastEvalText());

  w.setText("16*2");
  EXPECT_EQ(32, w.textInt());
  EXPECT_TRUE(w.hasLastEvalText());
  EXPECT_EQ("16*2", w.lastEvalText());

  // A failed evaluation leaves the known-good text alone.
  w.setText("16*");
  EXPECT_EQ(32, w.textInt());
  EXPECT_EQ("16*2", w.lastEvalText());
}

// hasLastEvalText() is what tells an Entry it is a numeric field at all, so
// it must stay false for text nothing reads as a number.
TEST(WidgetTextMath, HasLastEvalTextStaysFalseUntilTextIsReadAsANumber)
{
  RecordingWidget w;
  w.setText("123");
  EXPECT_FALSE(w.hasLastEvalText());
  EXPECT_EQ(123, w.textInt());
  EXPECT_TRUE(w.hasLastEvalText());
}

// evalmath happily divides by zero, so a parseable expression can still
// produce inf/NaN - neither is a usable value to hand a caller.
TEST(WidgetTextMath, NonFiniteResultIsTreatedAsAFailedEvaluation)
{
  RecordingWidget w;
  w.setText("10");
  EXPECT_EQ(10, w.textInt());

  w.setText("1/0");
  EXPECT_EQ(10, w.textInt());
  EXPECT_EQ(1, w.errorCount);
  EXPECT_EQ("10", w.lastEvalText());
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
