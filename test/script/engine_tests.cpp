// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "base/with_handle.h"
#include "script/engine.h"
#include "script/engine_delegate.h"

using script::Engine;
using script::EngineDelegate;
using script::ObjectDestroyedException;
using script::ScriptObject;
using script::Value;

namespace {

// A ScriptObject that records whatever arguments JS passes to its native
// "capture" function - used to verify raiseEvent()'s round trip through a
// real JS onEvent handler.
class CapturingScriptObject : public ScriptObject {
public:
  std::vector<Value> captured;

  CapturingScriptObject() {
    addFunction("capture", [this](Value a, Value b, Value c) {
      captured.clear();
      captured.push_back(a);
      captured.push_back(b);
      captured.push_back(c);
      return Value{};
    });
    makeGlobal("native");
  }
};

struct Thing : public WithHandle<Thing> {};

class ThingScriptObject : public ScriptObject {
};

// Tracks whether the wrapped native object was actually deleted, to verify
// that execAfterEval() releases (and the owning ScriptObject wrapper then
// disposes) an object once JS drops its only reference to it.
struct Tracked : public WithHandle<Tracked> {
  static inline bool destroyed = false;
  ~Tracked() { destroyed = true; }
};

class TrackedScriptObject : public ScriptObject {
};

// Exposes a native "make" function that hands a fresh, engine-owned Tracked
// wrapper out to JS as a return value.
class ExposerScriptObject : public ScriptObject {
public:
  ExposerScriptObject() {
    addFunction("make", [this] {
      auto* t = new Tracked();
      return Value{getEngine()->getScriptObject(t, /*own=*/true)};
    });
    makeGlobal("exposer");
  }
};

// A ScriptObject whose native method throws ObjectDestroyedException once
// its target handle has expired - mirroring the real app/script/api/*.cpp
// pattern (e.g. ImageScriptObject::img()).
class FragileScriptObject : public ScriptObject {
public:
  Handle target;

  FragileScriptObject() {
    addFunction("poke", [this] {
      if (!target.get<Thing>())
        throw ObjectDestroyedException{};
      return Value{1};
    });
    makeGlobal("fragile");
  }
};

} // namespace

ScriptObject::Regular<ThingScriptObject> g_regThing(typeid(Thing*).name());
ScriptObject::Regular<TrackedScriptObject> g_regTracked(typeid(Tracked*).name());

class EngineTest : public ::testing::Test {
protected:
  inject<Engine> engine{"qjs"};

  // Must run before any fixture is constructed: QuickJSEngine's own
  // `inject<EngineDelegate> m_delegate` member (default name "") resolves
  // during the `engine` member-initializer above, i.e. before SetUp() -
  // registering "stdout" as default there would be one test too late for
  // the very first test.
  static void SetUpTestSuite() {
    ASSERT_TRUE(EngineDelegate::setDefault("stdout"));
  }

  void SetUp() override {
    ASSERT_TRUE(engine);
  }
};

TEST_F(EngineTest, EvalSucceedsOnValidCode)
{
  EXPECT_TRUE(engine->eval("1 + 1;"));
}

TEST_F(EngineTest, EvalFailsOnASyntaxError)
{
  EXPECT_FALSE(engine->eval("this is not valid javascript {{{"));
}

TEST_F(EngineTest, EvalFailsWhenTheScriptThrows)
{
  EXPECT_FALSE(engine->eval("throw new Error('boom');"));
}

TEST_F(EngineTest, PrintLastResultDefaultsToOffAndCanBeEnabled)
{
  EXPECT_FALSE(engine->getPrintLastResult());
  engine->printLastResult();
  EXPECT_TRUE(engine->getPrintLastResult());
  // Must not crash even though it now prints the completion value.
  EXPECT_TRUE(engine->eval("21 * 2;"));
}

TEST_F(EngineTest, AfterEvalListenersFireExactlyOnceThenAreCleared)
{
  int calls = 0;
  bool lastSuccess = false;
  engine->afterEval([&](bool success) {
    ++calls;
    lastSuccess = success;
  });

  ASSERT_TRUE(engine->eval("1;"));
  EXPECT_EQ(1, calls);
  EXPECT_TRUE(lastSuccess);

  // Not re-registered - a second eval must not fire it again.
  ASSERT_TRUE(engine->eval("2;"));
  EXPECT_EQ(1, calls);
}

TEST_F(EngineTest, AfterEvalListenerSeesFailureOnAThrowingScript)
{
  bool sawFailure = false;
  engine->afterEval([&](bool success) { sawFailure = !success; });
  EXPECT_FALSE(engine->eval("throw 1;"));
  EXPECT_TRUE(sawFailure);
}

TEST_F(EngineTest, RaiseEventDeliversHeterogeneousArgumentsToOnEvent)
{
  CapturingScriptObject capture;
  ASSERT_TRUE(engine->eval(
    "globalThis.onEvent = function(a, b, c) { native.capture(a, b, c); };"));

  std::vector<Value> args = {Value{42}, Value{std::string("hi")}, Value{3.5}};
  EXPECT_TRUE(engine->raiseEvent(args));

  ASSERT_EQ(3u, capture.captured.size());
  EXPECT_EQ(42, (int)capture.captured[0]);
  EXPECT_EQ("hi", (std::string)capture.captured[1]);
  EXPECT_DOUBLE_EQ(3.5, (double)capture.captured[2]);
}

TEST_F(EngineTest, RaiseEventWithNoOnEventHandlerStillSucceeds)
{
  EXPECT_TRUE(engine->raiseEvent({Value{1}}));
}

TEST_F(EngineTest, GetScriptObjectReturnsTheSameWrapperForTheSameRawPointer)
{
  Thing t;
  ScriptObject* a = engine->getScriptObject(&t);
  ScriptObject* b = engine->getScriptObject(&t);
  EXPECT_EQ(a, b);
}

TEST_F(EngineTest, GetScriptObjectOnANullPointerReturnsNull)
{
  EXPECT_EQ(nullptr, engine->getScriptObject<Thing>(nullptr));
}

TEST_F(EngineTest, GetScriptObjectGivesDistinctWrappersForDistinctObjects)
{
  Thing a, b;
  ScriptObject* wa = engine->getScriptObject(&a);
  ScriptObject* wb = engine->getScriptObject(&b);
  EXPECT_NE(wa, wb);
}

TEST_F(EngineTest, ExecAfterEvalReleasesAndDisposesAnObjectOnceJsDropsIt)
{
  // Regression coverage for ScriptObject's destructor previously being
  // #if _DEBUG-only (see script_object_tests.cpp): the Tracked native
  // object here is only ever deleted via the owning wrapper's dispose(),
  // which only runs from that destructor.
  Tracked::destroyed = false;
  ExposerScriptObject exposer;

  ASSERT_TRUE(engine->eval("var x = exposer.make(); x = null;"));

  EXPECT_TRUE(Tracked::destroyed);
}

TEST_F(EngineTest, ObjectDestroyedExceptionFromANativeCallDoesNotCrashEval)
{
  FragileScriptObject fragile;
  auto* thing = new Thing();
  fragile.target = thing->handle();

  EXPECT_TRUE(engine->eval("fragile.poke();")); // handle alive: succeeds

  delete thing;
  // The native call now throws ObjectDestroyedException; the engine must
  // catch it internally rather than letting it escape eval() or crash.
  EXPECT_TRUE(engine->eval("fragile.poke();"));
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
