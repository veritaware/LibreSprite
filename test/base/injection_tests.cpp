// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "base/injection.h"

#include <set>
#include <string>

namespace {

// --- Regular vs Singleton policies -----------------------------------

int g_stdoutAlive = 0;

struct Logger : public Injectable<Logger> {
  virtual ~Logger() = default;
  virtual std::string speak() = 0;
};

struct StdoutLogger : public Logger {
  StdoutLogger() { ++g_stdoutAlive; }
  ~StdoutLogger() override { --g_stdoutAlive; }
  std::string speak() override { return "stdout"; }
};
Logger::Regular<StdoutLogger> g_regStdout{"stdout"};

struct FileLogger : public Logger {
  std::string speak() override { return "file"; }
};
Logger::Singleton<FileLogger> g_regFile{"file"};

// --- Provides policy ----------------------------------------------------

struct Account : public Injectable<Account> {
  virtual ~Account() = default;
  virtual int amount() = 0;
};

struct RealAccount : public Account {
  int amount() override { return 100; }
};

// --- setDefault / flags / getAll ----------------------------------------

struct Thing : public Injectable<Thing> {
  virtual ~Thing() = default;
  virtual std::string id() = 0;
};

struct ThingA : public Thing {
  std::string id() override { return "A"; }
};

struct ThingB : public Thing {
  std::string id() override { return "B"; }
};

Thing::Regular<ThingA> g_regA{"a", {"flagA"}};
Thing::Regular<ThingB> g_regB{"b", {"flagB"}};

// --- inject<>::get<Derived>() dynamic_cast --------------------------------

struct Base2 : public Injectable<Base2> {
  virtual ~Base2() = default;
};

struct Derived2 : public Base2 {
  int val = 3;
};

Base2::Regular<Derived2> g_regDerived{"derived"};

} // namespace

TEST(Injection, RegularCreatesAFreshOwnedInstancePerInject)
{
  g_stdoutAlive = 0;
  {
    inject<Logger> a{"stdout"};
    EXPECT_EQ(1, g_stdoutAlive);

    inject<Logger> b{"stdout"};
    EXPECT_EQ(2, g_stdoutAlive);

    EXPECT_NE(static_cast<Logger*>(a), static_cast<Logger*>(b));
    EXPECT_EQ("stdout", a->speak());
  }
  // Both injects went out of scope: Regular's onDetach deletes the instance.
  EXPECT_EQ(0, g_stdoutAlive);
}

TEST(Injection, SingletonReturnsTheSameInstanceEveryTime)
{
  inject<Logger> a{"file"};
  inject<Logger> b{"file"};

  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  EXPECT_EQ(static_cast<Logger*>(a), static_cast<Logger*>(b));
  EXPECT_EQ("file", a->speak());
}

TEST(Injection, ProvidesRegistersAndUnregistersOnDestruction)
{
  {
    RealAccount acc;
    Account::Provides p{&acc}; // registers under the default ("") name

    inject<Account> viaDefault;
    ASSERT_TRUE(viaDefault);
    EXPECT_EQ(100, viaDefault->amount());
  }
  // `p` went out of scope above, unregistering the default entry.
  inject<Account> afterScope;
  EXPECT_FALSE(afterScope);
}

TEST(Injection, SetDefaultByNameSelectsThatEntry)
{
  ASSERT_TRUE(Thing::setDefault("a"));
  inject<Thing> def{""};
  ASSERT_TRUE(def);
  EXPECT_EQ("A", def->id());

  ASSERT_TRUE(Thing::setDefault("b"));
  inject<Thing> def2{""};
  ASSERT_TRUE(def2);
  EXPECT_EQ("B", def2->id());
}

TEST(Injection, SetDefaultByFlagSelectsAMatchingEntry)
{
  ASSERT_TRUE(Thing::setDefault("does-not-exist-as-a-name", {"flagB"}));
  inject<Thing> def{""};
  ASSERT_TRUE(def);
  EXPECT_EQ("B", def->id());
}

TEST(Injection, SetDefaultFailsWithNoNameMatchAndNoFlagMatch)
{
  EXPECT_FALSE(Thing::setDefault("nope", {"no-such-flag"}));
}

TEST(Injection, GetAllReturnsEveryNamedEntryOnceExcludingTheDefaultAlias)
{
  auto all = Thing::getAll();

  std::set<std::string> ids;
  for (auto& inj : all)
    ids.insert(inj->id());

  // "a" and "b" only; the "" alias created by setDefault() must not
  // duplicate one of them.
  EXPECT_EQ(2u, ids.size());
  EXPECT_EQ(1u, ids.count("A"));
  EXPECT_EQ(1u, ids.count("B"));
}

TEST(Injection, GetAllWithFlagFiltersByFlag)
{
  auto onlyB = Thing::getAllWithFlag("flagB");
  ASSERT_EQ(1u, onlyB.size());
  EXPECT_EQ("B", onlyB[0]->id());

  auto none = Thing::getAllWithFlag("no-such-flag");
  EXPECT_TRUE(none.empty());
}

TEST(Injection, UnknownNameYieldsANullInject)
{
  inject<Thing> missing{"this-name-was-never-registered"};
  EXPECT_FALSE(missing);
  EXPECT_EQ(nullptr, missing.get<Thing>());
}

TEST(Injection, NullptrConstructionYieldsAnInvalidInject)
{
  inject<Logger> i{nullptr};
  EXPECT_FALSE(i);
}

TEST(Injection, InjectIsMovable)
{
  inject<Logger> a{"stdout"};
  Logger* raw = a;

  inject<Logger> b{std::move(a)};
  EXPECT_EQ(raw, static_cast<Logger*>(b));
}

TEST(Injection, InjectGetTemplateDynamicCastsToADerivedType)
{
  inject<Base2> i{"derived"};
  Derived2* d = i.get<Derived2>();
  ASSERT_NE(nullptr, d);
  EXPECT_EQ(3, d->val);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
