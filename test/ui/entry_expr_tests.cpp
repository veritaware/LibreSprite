// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#define TEST_GUI
#include "tests.h"

#include "ui/entry.h"
#include "ui/message.h"

#include <string>

using namespace ui;

namespace {

// Entry puts a half-typed expression right on kFocusLeaveMessage - send
// one directly instead of standing up a Manager to move focus for real.
void loseFocus(Entry& entry)
{
  Message msg(kFocusLeaveMessage);
  entry.sendMessage(&msg);
}

// Stands in for a window that reads an entry as a number to refresh a live
// preview (Canvas Size, Sprite Size, Import Sprite Sheet...).
class PreviewReader {
public:
  explicit PreviewReader(Entry& entry) : m_entry(entry)
  {
    m_entry.Change.connect([this]{ read(); });
  }

  void read() { value = m_entry.textInt(); ++reads; }

  int value = 0;
  int reads = 0;

private:
  Entry& m_entry;
};

} // namespace

TEST(EntryExpr, HalfTypedExpressionDoesNotMoveThePreviewValue)
{
  Entry entry(32, "300");
  PreviewReader preview(entry);
  preview.read();
  EXPECT_EQ(300, preview.value);

  // Typing the "-" of a negative number leaves the entry holding something
  // that doesn't evaluate; the preview must sit still rather than jump to
  // an invented number.
  entry.setText("-");
  preview.read();
  EXPECT_EQ(300, preview.value);

  entry.setText("-25");
  preview.read();
  EXPECT_EQ(-25, preview.value);
}

// The case from the Canvas Size dialog. That window fills its border
// fields with 0 when it is built but doesn't read them until something
// changes, so the value to go back to has to come from the text itself,
// not from a read.
TEST(EntryExpr, FocusLeaveRestoresAValueNothingHadReadYet)
{
  Entry entry(32, "0"); // as the window fills it
  PreviewReader preview(entry);

  entry.setText("-");
  preview.read(); // first read of all: the field is only now known numeric
  EXPECT_EQ(0, preview.value);

  loseFocus(entry);
  EXPECT_EQ("0", entry.text());
}

TEST(EntryExpr, FocusLeaveRestoresTheLastTextThatEvaluated)
{
  Entry entry(32, "0");
  PreviewReader preview(entry);
  preview.read();

  entry.setText("-25");
  preview.read();

  entry.setText("-25*"); // trailing operator
  preview.read();

  loseFocus(entry);
  EXPECT_EQ("-25", entry.text()); // not "0": the newer valid text wins
}

TEST(EntryExpr, FocusLeaveKeepsTextThatStillEvaluates)
{
  Entry entry(32, "0");
  PreviewReader preview(entry);
  preview.read();

  entry.setText("16*2");
  preview.read();
  EXPECT_EQ(32, preview.value);

  loseFocus(entry);
  EXPECT_EQ("16*2", entry.text()); // the expression itself is preserved
}

TEST(EntryExpr, FocusLeaveNotifiesListenersAfterRestoringTheText)
{
  Entry entry(32, "10");
  PreviewReader preview(entry);
  preview.read();

  entry.setText("10+");
  const int readsBefore = preview.reads;

  loseFocus(entry);
  EXPECT_EQ("10", entry.text());
  EXPECT_GT(preview.reads, readsBefore);
  EXPECT_EQ(10, preview.value);
}

// Repeated focus changes must not wear the restore value down - this is
// what made the behaviour look random when it was snapshotted on
// focus-enter instead of tracked on the text.
TEST(EntryExpr, RepeatedFocusChangesKeepRestoringTheSameValue)
{
  Entry entry(32, "0");
  PreviewReader preview(entry);
  preview.read();

  for (int i = 0; i < 5; ++i) {
    entry.setText("-");
    preview.read();
    loseFocus(entry);
    EXPECT_EQ("0", entry.text()) << "iteration " << i;
  }
}

// A focus-leave with nothing wrong in the field must not disturb it.
TEST(EntryExpr, FocusLeaveOnValidTextIsANoOp)
{
  Entry entry(32, "5");
  PreviewReader preview(entry);
  preview.read();

  const int readsBefore = preview.reads;
  loseFocus(entry);
  EXPECT_EQ("5", entry.text());
  EXPECT_EQ(readsBefore, preview.reads); // no spurious Change
}

// The restore only applies to entries something reads as a number. A plain
// text entry must keep whatever the user typed, even if it happens to look
// like a broken expression.
TEST(EntryExpr, PlainTextEntryIsLeftAloneOnFocusLeave)
{
  Entry entry(32, "");

  entry.setText("Layer-");
  loseFocus(entry);
  EXPECT_EQ("Layer-", entry.text());

  entry.setText("-");
  loseFocus(entry);
  EXPECT_EQ("-", entry.text());
}

// Even a numeric-looking text entry stays untouched as long as nothing
// evaluates it - a layer named "123" is still just a name.
TEST(EntryExpr, NumericLookingTextEntryIsLeftAloneOnFocusLeave)
{
  Entry entry(32, "123");

  entry.setText("abc");
  loseFocus(entry);
  EXPECT_EQ("abc", entry.text());
}

// An entry that has never held anything valid has nothing to restore, and
// must not blank itself out trying.
TEST(EntryExpr, EntryWithNoValidTextIsLeftAloneOnFocusLeave)
{
  Entry entry(32, "abc");
  PreviewReader preview(entry);
  preview.read();
  EXPECT_EQ(0, preview.value);

  entry.setText("def");
  preview.read();

  loseFocus(entry);
  EXPECT_EQ("def", entry.text());
}
