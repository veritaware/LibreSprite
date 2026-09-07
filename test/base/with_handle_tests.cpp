// Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "base/with_handle.h"

namespace {

bool g_fooDestroyed = false;

struct Foo : public WithHandle<Foo> {
  int value = 5;
  ~Foo() { g_fooDestroyed = true; }
};

struct SubFoo : public Foo {
  int extra = 9;
};

struct Bar : public WithHandle<Bar> {
  int other = 1;
};

} // namespace

TEST(WithHandle, ValidWhileOwnerAlive)
{
  auto* obj = new Foo();
  Handle h = obj->handle();

  EXPECT_TRUE(h);
  EXPECT_EQ(obj, h.get<Foo>());

  delete obj;
}

TEST(WithHandle, ExpiresWhenOwnerDestroyed)
{
  g_fooDestroyed = false;
  auto* obj = new Foo();
  Handle h = obj->handle();
  ASSERT_TRUE(h);

  delete obj;

  EXPECT_TRUE(g_fooDestroyed);
  EXPECT_FALSE(h);
  EXPECT_EQ(nullptr, h.get<Foo>());
}

TEST(WithHandle, GetReturnsNullOnTypeMismatch)
{
  auto* obj = new Foo();
  Handle h = obj->handle();

  // Bar was never involved in constructing this handle, so its type hash
  // does not match Foo's - a cross-type get() must fail closed.
  EXPECT_EQ(nullptr, h.get<Bar>());

  delete obj;
}

TEST(WithHandle, GetWithDerivedCastsDownWhenTypeMatches)
{
  auto* obj = new SubFoo();
  Handle h = obj->handle(); // constructed against Foo (the WithHandle<Foo> base)

  SubFoo* back = h.get<Foo, SubFoo>();
  ASSERT_NE(nullptr, back);
  EXPECT_EQ(obj, back);
  EXPECT_EQ(9, back->extra);

  delete obj;
}

TEST(WithHandle, VoidGetBypassesTypeCheck)
{
  auto* obj = new Foo();
  Handle h = obj->handle();

  void* raw = h.get<void>();
  EXPECT_EQ(static_cast<void*>(obj), raw);

  delete obj;
}

TEST(WithHandle, ResetInvalidatesTheHandle)
{
  auto* obj = new Foo();
  Handle h = obj->handle();
  ASSERT_TRUE(h);

  h.reset();

  EXPECT_FALSE(h);
  EXPECT_EQ(nullptr, h.get<Foo>());

  delete obj;
}

TEST(WithHandle, DisposeDeletesTheUnderlyingObjectOnce)
{
  g_fooDestroyed = false;
  auto* obj = new Foo();
  Handle h = obj->handle();

  h.dispose();

  EXPECT_TRUE(g_fooDestroyed);
  EXPECT_FALSE(h);
}

TEST(WithHandle, DisposeOnAnAlreadyExpiredHandleIsANoop)
{
  auto* obj = new Foo();
  Handle h = obj->handle();
  delete obj;
  ASSERT_FALSE(h);

  // Must not double-free or crash.
  h.dispose();
  EXPECT_FALSE(h);
}

TEST(WithHandle, DefaultConstructedHandleIsInvalid)
{
  Handle h;
  EXPECT_FALSE(h);
  EXPECT_EQ(nullptr, h.get<Foo>());
  h.dispose(); // no-op, must not crash
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
