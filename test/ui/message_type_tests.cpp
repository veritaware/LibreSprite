// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "ui/message_type.h"

using namespace ui;

TEST(MessageType, EveryRegisteredValueHasANonEmptyName)
{
  const MessageType types[] = {
    kOpenMessage,          kCloseMessage,      kCloseDisplayMessage,
    kResizeDisplayMessage, kPaintMessage,      kTimerMessage,
    kDropFilesMessage,     kWinMoveMessage,    kKeyDownMessage,
    kKeyUpMessage,         kFocusEnterMessage, kFocusLeaveMessage,
    kMouseDownMessage,     kMouseUpMessage,    kDoubleClickMessage,
    kMouseEnterMessage,    kMouseLeaveMessage, kMouseMoveMessage,
    kSetCursorMessage,     kMouseWheelMessage, kTouchMagnifyMessage,
  };

  for (MessageType type : types)
    EXPECT_STRNE("", to_string(type)) << "type index " << int(type);
}

TEST(MessageType, ValuesAtOrPastFirstRegisteredReturnAnEmptyString)
{
  EXPECT_STREQ("", to_string(kFirstRegisteredMessage));
  EXPECT_STREQ("", to_string(static_cast<MessageType>(kFirstRegisteredMessage + 1)));
  EXPECT_STREQ("", to_string(kLastRegisteredMessage));
}
