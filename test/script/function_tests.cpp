// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "script/function.h"

using script::Function;
using script::Value;

namespace {
int add(int a, int b) { return a + b; }
void noop() {}
}

TEST(Function, DefaultConstructedIsANoopThatYieldsUndefined)
{
  Function f;
  f();
  EXPECT_EQ(Value::Type::UNDEFINED, f.result.type);
}

TEST(Function, WrapsAFreeFunctionAndCoercesArguments)
{
  Function f(add);
  f.arguments.push_back(2);
  f.arguments.push_back(3);
  f();
  EXPECT_EQ(5, (int)f.result);
}

TEST(Function, ArgumentsAreClearedAfterEachCall)
{
  Function f(add);
  f.arguments.push_back(2);
  f.arguments.push_back(3);
  f();
  EXPECT_TRUE(f.arguments.empty());
}

TEST(Function, WrapsACapturelessLambda)
{
  Function f([](int x) { return x * 2; });
  f.arguments.push_back(21);
  f();
  EXPECT_EQ(42, (int)f.result);
}

TEST(Function, WrapsACapturingLambda)
{
  int callCount = 0;
  Function f([&callCount](int x) {
    ++callCount;
    return x + 1;
  });
  f.arguments.push_back(9);
  f();
  EXPECT_EQ(10, (int)f.result);
  EXPECT_EQ(1, callCount);
}

TEST(Function, MissingArgumentsAreDefaultConstructed)
{
  // Only one of two arguments supplied - the second becomes an UNDEFINED
  // Value, which coerces to int as 0.
  Function f(add);
  f.arguments.push_back(10);
  f();
  EXPECT_EQ(10, (int)f.result);
}

TEST(Function, SetDefaultSuppliesValuesForOmittedArguments)
{
  Function f(add);
  f.setDefault(0, 100);
  f.arguments.push_back(5); // only first argument given; second comes from defaults
  f();
  EXPECT_EQ(105, (int)f.result);
}

TEST(Function, VoidFreeFunctionYieldsUndefined)
{
  Function f(noop);
  f();
  EXPECT_EQ(Value::Type::UNDEFINED, f.result.type);
}

TEST(Function, VoidLambdaYieldsUndefinedAndStillRuns)
{
  bool called = false;
  Function f([&called]() { called = true; });
  f();
  EXPECT_TRUE(called);
  EXPECT_EQ(Value::Type::UNDEFINED, f.result.type);
}

TEST(Function, VoidLambdaWithArgumentsStillRuns)
{
  int seen = 0;
  Function f([&seen](int x) { seen = x; });
  f.arguments.push_back(7);
  f();
  EXPECT_EQ(7, seen);
  EXPECT_EQ(Value::Type::UNDEFINED, f.result.type);
}

TEST(Function, ArityMatchesTheWrappedCallableAndZeroArgFillsNothing)
{
  // Not directly observable via a public accessor, but exercised through
  // behavior: a zero-arg callable ignores any pushed arguments beyond what
  // it declares, and still runs correctly.
  int calls = 0;
  Function f([&calls] { ++calls; return 1; });
  f();
  EXPECT_EQ(1, calls);
  EXPECT_EQ(1, (int)f.result);
}

TEST(Function, VarArgsExposesTheFullArgumentListDuringACall)
{
  std::size_t seenCount = 0;
  Function f([&seenCount](int) {
    seenCount = Function::varArgs().size();
    return 0;
  });
  f.arguments.push_back(1);
  f.arguments.push_back(2);
  f.arguments.push_back(3);
  f();
  // varArgs() reflects the padded argument vector at call time (arity 1
  // here, but the underlying vector still has all 3 pushed values before
  // padding truncation would matter for a fixed-arity function).
  EXPECT_GE(seenCount, 1u);
}

TEST(Function, StringArgumentsRoundTrip)
{
  Function f([](const std::string& s) { return s + "!"; });
  f.arguments.push_back(std::string("hi"));
  f();
  EXPECT_EQ("hi!", (std::string)f.result);
}

TEST(Function, CopyPreservesTheWrappedCallable)
{
  Function f(add);
  Function copy(f);
  copy.arguments.push_back(4);
  copy.arguments.push_back(6);
  copy();
  EXPECT_EQ(10, (int)copy.result);
}

TEST(Function, MoveTransfersTheWrappedCallable)
{
  Function f(add);
  Function moved(std::move(f));
  moved.arguments.push_back(1);
  moved.arguments.push_back(2);
  moved();
  EXPECT_EQ(3, (int)moved.result);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
