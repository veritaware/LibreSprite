// Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "base/safe_ptr.h"

using namespace base;

namespace {

// A plain type using the manually-embedded safe_ptr pattern described in
// safe_ptr.h's header comment.
struct Manual {
  base::safe_ptr<Manual> ptr{this};
  int value = 42;
};

// A type with a virtual destructor, usable with base::make_safe().
struct Virtual {
  virtual ~Virtual() = default;
  int value = 7;
};

} // namespace

TEST(SafePtr, NullConstruction)
{
  safe_ptr<Manual> p{nullptr};
  EXPECT_FALSE(p);
}

TEST(SafePtr, BecomesNullWhenOwnerDestroyed)
{
  auto* obj = new Manual();
  safe_ptr<Manual> copy = obj->ptr;

  EXPECT_TRUE(copy);
  EXPECT_EQ(42, copy->value);

  delete obj;

  EXPECT_FALSE(copy);
}

TEST(SafePtr, CopiesShareTheSameStorage)
{
  auto* obj = new Manual();
  safe_ptr<Manual> a = obj->ptr;
  safe_ptr<Manual> b = a;
  safe_ptr<Manual> c{nullptr};
  c = b;

  EXPECT_TRUE(a);
  EXPECT_TRUE(b);
  EXPECT_TRUE(c);

  delete obj;

  EXPECT_FALSE(a);
  EXPECT_FALSE(b);
  EXPECT_FALSE(c);
}

TEST(SafePtr, MoveKeepsValidity)
{
  auto* obj = new Manual();
  safe_ptr<Manual> a = obj->ptr;
  safe_ptr<Manual> b{nullptr};
  b = std::move(a);

  EXPECT_TRUE(b);
  EXPECT_EQ(42, b->value);

  delete obj;

  EXPECT_FALSE(b);
}

TEST(SafePtr, GetReturnsRawPointerWhileAlive)
{
  auto* obj = new Manual();
  safe_ptr<Manual> p = obj->ptr;

  EXPECT_EQ(obj, p.get());

  delete obj;
}

TEST(SafePtr, MakeSafeOwnsAVirtualDtorInstance)
{
  auto safe = base::make_safe<Virtual>();
  ASSERT_TRUE(safe);
  EXPECT_EQ(7, safe->value);

  auto copy = safe;
  EXPECT_TRUE(copy);

  delete safe.get();

  EXPECT_FALSE(safe);
  EXPECT_FALSE(copy);
}

TEST(SafePtr, FindSafePtrReturnsNullForUnknownRawPointer)
{
  Manual stackObj;
  auto found = base::findSafePtr(&stackObj);
  EXPECT_FALSE(found);
}

TEST(SafePtr, FindSafePtrReturnsRegisteredCopy)
{
  auto safe = base::make_safe<Virtual>();
  auto* raw = safe.get();

  auto found = base::findSafePtr(raw);
  ASSERT_TRUE(found);
  EXPECT_EQ(raw, found.get());

  delete raw;
  EXPECT_FALSE(found);
}

TEST(SafePtr, PurgeSafePtrsDropsDeadEntriesWithoutCrashing)
{
  auto* raw = base::make_safe<Virtual>().get();
  ASSERT_TRUE(base::findSafePtr(raw));

  delete raw;

  // The index still holds a (now-null) entry for `raw` until purged; the
  // lookup itself is already false because the safe_ptr's storage was
  // cleared by the destructor.
  EXPECT_FALSE(base::findSafePtr(raw));

  // Purging should be safe to call and must not resurrect the dead entry.
  base::purgeSafePtrs();
  EXPECT_FALSE(base::findSafePtr(raw));
}

TEST(SafePtr, SaveSafePtrOverwritesAStaleEntryAtAReusedAddress)
{
  // A make_safe()'d object can be deleted and the allocator can hand its
  // exact address back out to a *new* make_safe() call before anyone runs
  // purgeSafePtrs(). The stale (dead) index entry for that address must
  // not shadow the new, live registration.
  auto* first = base::make_safe<Virtual>().get();
  ASSERT_TRUE(base::findSafePtr(first));
  delete first;
  ASSERT_FALSE(base::findSafePtr(first));

  void* reused = nullptr;
  for (int attempt = 0; attempt < 64 && !reused; ++attempt) {
    auto* candidate = base::make_safe<Virtual>().get();
    if (static_cast<void*>(candidate) == static_cast<void*>(first)) {
      reused = candidate;
    }
    else {
      delete candidate;
    }
  }

  if (!reused) {
    GTEST_SKIP() << "allocator never reused the freed address in 64 attempts";
    return;
  }

  auto found = base::findSafePtr(reinterpret_cast<Virtual*>(reused));
  ASSERT_TRUE(found);
  EXPECT_EQ(reused, found.get());

  delete reinterpret_cast<Virtual*>(reused);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
