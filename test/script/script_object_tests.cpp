// Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "base/with_handle.h"
#include "script/engine.h" // completes script::Engine, forward-declared by
                            // script_object.h - InternalScriptObject's
                            // `inject<Engine> m_engine` member needs the
                            // full type wherever it's instantiated.
#include "script/script_object.h"

using script::DocumentedFunction;
using script::Function;
using script::InternalScriptObject;
using script::ObjectProperty;
using script::ScriptObject;
using script::Value;

namespace {

// A minimal concrete InternalScriptObject, registered as the default (""),
// so plain ScriptObjects can be constructed without an Engine/JS runtime.
// The scripting engine tests (test/script/engine_tests.cpp) build a real
// QuickJS-backed engine instead; this file is deliberately engine-free.
class TestInternal : public InternalScriptObject {
public:
  bool madeGlobal = false;
  std::string globalName;

  void makeGlobal(const std::string& name) override {
    madeGlobal = true;
    globalName = name;
  }
};
InternalScriptObject::Regular<TestInternal> g_regInternal("");

// InternalScriptObject::m_engine is unconditionally default-injected, so a
// default Engine must exist even though none of these tests eval() any
// script. A trivial no-op keeps that lookup quiet without pulling in the
// real QuickJS engine (exercised separately by engine_tests.cpp).
//
// Must be a Singleton, not a Regular: every Engine privately self-registers
// as the default ("") via its own Provides member on construction, and a
// Provides unregisters itself (erasing the "" slot outright) once its
// owning instance is destroyed. With Regular, the first NullEngine's
// Provides would clobber this registration, and destroying that first
// instance at the end of whichever test created it would then erase the
// slot for every later test. Singleton's instance is never destroyed
// mid-run, so the self-registration just keeps pointing at itself.
class NullEngine : public script::Engine {
public:
  bool eval(const std::string&) override { return true; }
  bool raiseEvent(const std::vector<Value>&) override { return true; }
};
script::Engine::Singleton<NullEngine> g_regEngine("");

struct Widget : public WithHandle<Widget> {
  static inline bool destroyed = false;
  ~Widget() { destroyed = true; }
};

class WidgetScriptObject : public ScriptObject {
public:
  Handle build() override { return (new Widget())->handle(); }
};

class Counter : public ScriptObject {
public:
  int value = 0;

  Counter() {
    addProperty(
      "value",
      [this] { return value; },
      [this](int v) { value = v; return Value{}; });
    addFunction("increment", [this] { return ++value; });
  }
};

struct External {
  int sideEffect = 0;
  int mul(int a, int b) { return a * b; }
  void bump(int by) { sideEffect += by; }
};

class MethodHost : public ScriptObject {
public:
  External ext;
  int state = 10;

  int addToState(int x) { return state += x; }
  void resetState() { state = 0; }

  MethodHost() {
    addMethod("mul", &ext, &External::mul);
    addMethod("bump", &ext, &External::bump);
    addMethod("addToState", &MethodHost::addToState);
    addMethod("resetState", &MethodHost::resetState);
  }
};

class DocumentedHost : public ScriptObject {
public:
  DocumentedHost() {
    addProperty(
      "x",
      [] { return 1; },
      [](const Value&) { return Value{}; })
      .doc("the x value");
    addFunction("f", [] { return 1; })
      .doc("does f")
      .docArg("n", "unused")
      .docReturns("one");
  }
};

} // namespace

TEST(ObjectProperty, DocSetsTheDocString)
{
  ObjectProperty p;
  ObjectProperty& chained = p.doc("a property");
  EXPECT_EQ(&p, &chained);
  EXPECT_EQ("a property", p.docStr);
}

TEST(DocumentedFunction, DefaultDocReturnsStrIsNothing)
{
  Function f;
  DocumentedFunction df(f);
  EXPECT_EQ("Nothing", df.docReturnsStr);
  EXPECT_TRUE(df.docStr.empty());
  EXPECT_TRUE(df.docArgs.empty());
}

TEST(DocumentedFunction, DocMetadataChainsAndAccumulatesArgs)
{
  Function f([] { return 1; });
  DocumentedFunction df(f);

  df.doc("does a thing").docArg("x", "the x value").docArg("y", "the y value").docReturns("a number");

  EXPECT_EQ("does a thing", df.docStr);
  ASSERT_EQ(2u, df.docArgs.size());
  EXPECT_EQ("x", df.docArgs[0].name);
  EXPECT_EQ("the x value", df.docArgs[0].docStr);
  EXPECT_EQ("y", df.docArgs[1].name);
  EXPECT_EQ("a number", df.docReturnsStr);

  df();
  EXPECT_EQ(1, (int)df.result); // still callable like the Function it wraps
}

TEST(DocumentedFunction, ConstructibleFromAnRvalueFunction)
{
  DocumentedFunction df{Function([] { return 2; })};
  df();
  EXPECT_EQ(2, (int)df.result);
}

TEST(ScriptObject, GetSetAndCallRoundTripThroughProperties)
{
  Counter c;
  EXPECT_EQ(0, (int)c.get("value"));

  c.set("value", 5);
  EXPECT_EQ(5, c.value);
  EXPECT_EQ(5, (int)c.get("value"));

  EXPECT_EQ(6, (int)c.call("increment"));
  EXPECT_EQ(6, c.value);
}

TEST(ScriptObject, GetSetAndCallOnUnknownNamesAreSafeNoops)
{
  Counter c;
  EXPECT_EQ(Value::Type::UNDEFINED, c.get("nope").type);
  EXPECT_EQ(Value::Type::UNDEFINED, c.call("nope").type);
  c.set("nope", 42); // must not crash, and must not create the property
  EXPECT_EQ(Value::Type::UNDEFINED, c.get("nope").type);
}

TEST(ScriptObject, AddMethodExplicitInstanceOverloads)
{
  MethodHost host;
  EXPECT_EQ(6, (int)host.call("mul", 2, 3));

  host.call("bump", 5);
  EXPECT_EQ(5, host.ext.sideEffect);
}

TEST(ScriptObject, AddMethodImplicitThisOverloads)
{
  MethodHost host;
  EXPECT_EQ(15, (int)host.call("addToState", 5));
  EXPECT_EQ(15, host.state);

  host.call("resetState");
  EXPECT_EQ(0, host.state);
}

TEST(ScriptObject, AddPropertyAndAddFunctionReturnDocumentableReferences)
{
  DocumentedHost h;
  auto* internal = h.getInternalScriptObject();

  ASSERT_EQ(1u, internal->properties.count("x"));
  EXPECT_EQ("the x value", internal->properties.at("x").docStr);

  ASSERT_EQ(1u, internal->functions.count("f"));
  EXPECT_EQ("does f", internal->functions.at("f").docStr);
  EXPECT_EQ("one", internal->functions.at("f").docReturnsStr);
}

TEST(ScriptObject, MakeGlobalDelegatesToTheInternalScriptObject)
{
  class GlobalObj : public ScriptObject {
  public:
    GlobalObj() { makeGlobal("myGlobal"); }
  };

  GlobalObj g;
  auto* internal = static_cast<TestInternal*>(g.getInternalScriptObject());
  EXPECT_TRUE(internal->madeGlobal);
  EXPECT_EQ("myGlobal", internal->globalName);
}

TEST(ScriptObject, HandleTracksTheWrappedObjectAndExpiresWhenItDies)
{
  auto* w = new Widget();
  ScriptObject so;
  so.setWrapped(w->handle(), false); // non-owning

  EXPECT_EQ(w, so.handle<Widget>());

  delete w;
  EXPECT_EQ(nullptr, so.handle<Widget>());
}

TEST(ScriptObject, SetWrappedNonOwningDoesNotDisposeOnDestruction)
{
  Widget::destroyed = false;
  auto* w = new Widget();
  {
    ScriptObject so;
    so.setWrapped(w->handle(), false);
  }
  EXPECT_FALSE(Widget::destroyed);
  delete w;
}

TEST(ScriptObject, CreateOwnsAndDisposesTheBuiltHandleOnDestruction)
{
  // Regression test: ScriptObject's destructor (which disposes an owned
  // handle) used to be compiled only under #if _DEBUG, so in a normal
  // RelWithDebInfo/Release build an owned handle was never disposed at all.
  Widget::destroyed = false;
  {
    WidgetScriptObject wso;
    Widget* w = wso.create<Widget>();
    ASSERT_NE(nullptr, w);
    EXPECT_FALSE(Widget::destroyed);
  }
  EXPECT_TRUE(Widget::destroyed);
}

TEST(ScriptObject, SetWrappedReplacingAnOwnedHandleDisposesThePreviousOne)
{
  Widget::destroyed = false;
  auto* first = new Widget();
  auto* second = new Widget();
  {
    ScriptObject so;
    so.setWrapped(first->handle(), true);
    EXPECT_FALSE(Widget::destroyed);

    so.setWrapped(second->handle(), false); // must dispose `first` first
    EXPECT_TRUE(Widget::destroyed);
  }
  delete second;
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
