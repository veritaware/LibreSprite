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

// Entry validates an expression on kFocusLeaveMessage - send one directly
// instead of standing up a Manager to move focus around for real.
void loseFocus(Entry& entry)
{
  Message msg(kFocusLeaveMessage);
  entry.sendMessage(&msg);
}

// Focus-enter can't be sent the same way: handling it also starts a blink
// timer, talks to she and re-selects the text through the theme's font,
// none of which exists in a headless test. The part that matters here is
// the snapshot Entry takes of the value the user is about to edit.
class TestEntry : public Entry {
public:
  using Entry::Entry;
  void gainFocus() { rememberTextForRestore(); }
};

// Stands in for a dialog that reads an entry as a number on every edit to
// refresh a live preview (Canvas Size, Sprite Size, Import Sprite Sheet...).
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

TEST(EntryExpr, FocusLeaveRestoresTheLastTextThatEvaluated)
{
  Entry entry(32, "0");
  PreviewReader preview(entry);
  preview.read(); // the dialog reads it as a number => it's a numeric field

  entry.setText("-");
  preview.read();

  loseFocus(entry);
  EXPECT_EQ("0", entry.text());
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

  entry.setText("10+"); // trailing operator
  int readsBeforeFocusLeave = preview.reads;

  loseFocus(entry);
  EXPECT_EQ("10", entry.text());
  EXPECT_GT(preview.reads, readsBeforeFocusLeave);
  EXPECT_EQ(10, preview.value);
}

// The revert only applies to entries something reads as a number. A plain
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

// The case from the Canvas Size dialog: the field opens on a valid value
// that the dialog hasn't read yet, and the very first keystroke makes it
// unparseable - so there is no previously evaluated text to go back to.
// What the field held when editing started stands in for it.
TEST(EntryExpr, FocusLeaveFallsBackToTheTextEditingStartedFrom)
{
  TestEntry entry(32, "0");
  PreviewReader preview(entry);

  entry.gainFocus();

  entry.setText("-"); // first keystroke, before anything evaluated
  preview.read();
  EXPECT_EQ(0, preview.value); // no invented number reaches the preview

  loseFocus(entry);
  EXPECT_EQ("0", entry.text());
}

// A later good value wins over the one editing started from.
TEST(EntryExpr, FocusLeavePrefersTheLastTextThatEvaluated)
{
  TestEntry entry(32, "5");
  PreviewReader preview(entry);

  entry.gainFocus();

  entry.setText("7");
  preview.read();
  EXPECT_EQ(7, preview.value);

  entry.setText("7+"); // trailing operator
  preview.read();

  loseFocus(entry);
  EXPECT_EQ("7", entry.text());
}

// Focus alone must not turn a text field into a numeric one.
TEST(EntryExpr, FocusEnterDoesNotMakeAPlainTextEntryNumeric)
{
  TestEntry entry(32, "123");

  entry.gainFocus();
  entry.setText("abc");
  loseFocus(entry);

  EXPECT_EQ("abc", entry.text());
}
