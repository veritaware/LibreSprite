// Besprited | Copyright (C) 2026 Veritaware
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>

#include "script/value.h"

using script::Value;

TEST(Value, DefaultIsUndefined)
{
  Value v;
  EXPECT_EQ(Value::Type::UNDEFINED, v.type);
  EXPECT_FALSE(v);
  EXPECT_EQ(0, (int)v);
  EXPECT_EQ(0.0, (double)v);
  EXPECT_EQ("", (std::string)v);
}

TEST(Value, IntRoundTripAndConversions)
{
  Value zero{0};
  Value five{5};

  EXPECT_EQ(Value::Type::INT, five.type);
  EXPECT_EQ(5, (int)five);
  EXPECT_DOUBLE_EQ(5.0, (double)five);
  EXPECT_EQ("5", five.str());
  EXPECT_TRUE(five);
  EXPECT_FALSE(zero);

  Value fromUnsigned{7u};
  EXPECT_EQ(Value::Type::INT, fromUnsigned.type);
  EXPECT_EQ(7, (int)fromUnsigned);
}

TEST(Value, DoubleRoundTripAndConversions)
{
  Value pi{3.75};

  EXPECT_EQ(Value::Type::DOUBLE, pi.type);
  EXPECT_DOUBLE_EQ(3.75, (double)pi);
  EXPECT_EQ(3, (int)pi); // truncates, doesn't round
  EXPECT_TRUE(pi);

  Value zero{0.0};
  EXPECT_FALSE(zero);
}

TEST(Value, StringRoundTripAndConversions)
{
  Value fromLiteral{"hello"};
  EXPECT_EQ(Value::Type::STRING, fromLiteral.type);
  EXPECT_EQ("hello", fromLiteral.str());
  EXPECT_STREQ("hello", (const char*)fromLiteral);
  EXPECT_TRUE(fromLiteral);

  Value empty{""};
  EXPECT_FALSE(empty);

  Value fromLvalue{std::string("world")};
  EXPECT_EQ("world", fromLvalue.str());

  std::string moved = "movable";
  Value fromRvalue{std::move(moved)};
  EXPECT_EQ("movable", fromRvalue.str());

  Value numeric{"42"};
  EXPECT_EQ(42, (int)numeric);

  Value fractional{"3.5"};
  EXPECT_DOUBLE_EQ(3.5, (double)fractional);

  Value notANumber{"abc"};
  EXPECT_EQ(0, (int)notANumber); // atoi() on non-numeric text
}

TEST(Value, ReassigningAStringOntoAnExistingStringReusesStorage)
{
  Value v{"first"};
  auto* beforePtr = v.data.string_v;

  v = "second"; // still STRING -> reuses the existing std::string, doesn't reallocate
  EXPECT_EQ(beforePtr, v.data.string_v);
  EXPECT_EQ("second", v.str());

  v = 5; // switches type away from STRING -> old string is freed
  v = "third"; // now allocates a brand new std::string
  EXPECT_NE(beforePtr, v.data.string_v);
  EXPECT_EQ("third", v.str());
}

TEST(Value, CopyIsDeepForStrings)
{
  Value a{"original"};
  Value b = a;

  EXPECT_EQ("original", b.str());
  EXPECT_NE(a.data.string_v, b.data.string_v); // independent allocations

  b = "changed";
  EXPECT_EQ("original", a.str()); // a is unaffected by mutating b
}

TEST(Value, MoveIsShallowAndLeavesSourceUndefined)
{
  Value a{"original"};
  auto* originalPtr = a.data.string_v;

  Value b = std::move(a);

  EXPECT_EQ(originalPtr, b.data.string_v); // ownership transferred, not copied
  EXPECT_EQ("original", b.str());
  EXPECT_EQ(Value::Type::UNDEFINED, a.type); // moved-from is left undefined
}

TEST(Value, BufferOwnsAndReferenceCountsItsBytes)
{
  auto* bytes = new uint8_t[3]{1, 2, 3};
  Value v{bytes, 3, true};

  EXPECT_EQ(Value::Type::BUFFER, v.type);
  EXPECT_EQ(3u, v.size());
  EXPECT_FALSE(v.buffer().empty());
  EXPECT_TRUE(v.buffer().canSteal()); // sole owner so far

  {
    Value copy = v; // shares the underlying bytes, bumps the refcount
    EXPECT_FALSE(v.buffer().canSteal());
    EXPECT_FALSE(copy.buffer().canSteal());
    EXPECT_EQ(v.buffer().data(), copy.buffer().data());
  }

  // `copy` went out of scope: v is the sole owner again.
  EXPECT_TRUE(v.buffer().canSteal());
  EXPECT_EQ(1, v.buffer()[0]);
  EXPECT_EQ(2, v.buffer()[1]);
}

TEST(Value, BufferStealTransfersOwnershipWhenSoleOwner)
{
  auto* bytes = new uint8_t[2]{9, 8};
  Value v{bytes, 2, true};

  ASSERT_TRUE(v.buffer().canSteal());
  uint8_t* stolen = v.buffer().steal();
  ASSERT_EQ(bytes, stolen);
  EXPECT_FALSE(v.buffer().canSteal()); // ownership already given up

  delete[] stolen; // caller now owns it
}

TEST(Value, BufferStealFailsWhenShared)
{
  auto* bytes = new uint8_t[2]{9, 8};
  Value v{bytes, 2, true};
  Value copy = v; // now shared, refcount 2

  EXPECT_EQ(nullptr, copy.buffer().steal());
  EXPECT_EQ(nullptr, v.buffer().steal());
}

TEST(Value, BufferNonOwningDoesNotFreeOnDestruction)
{
  uint8_t stackBytes[2] = {4, 5};
  {
    Value v{stackBytes, 2, false};
    EXPECT_EQ(2u, v.size());
    EXPECT_FALSE(v.buffer().canSteal()); // no refcount at all when non-owning
  }
  // stackBytes must still be valid/unmodified here - nothing should have
  // deleted it.
  EXPECT_EQ(4, stackBytes[0]);
  EXPECT_EQ(5, stackBytes[1]);
}

TEST(Value, MapRoundTrip)
{
  using MapData = script::Value::Map::data_t;
  // RefCount<T>::release() always frees via delete[] (it's shared with
  // Buffer, whose bytes really are array-allocated), so an owning Map
  // pointer must be allocated as new MapData[1], not plain `new MapData`
  // - see the note on Value::Map's constructor.
  auto* map = new MapData[1];
  (*map)["a"] = 1;
  (*map)["b"] = "two";

  Value v{map, true};
  EXPECT_EQ(Value::Type::MAP, v.type);
  EXPECT_TRUE(v);
  EXPECT_EQ("[Object object]", v.str());

  MapData* back = v;
  ASSERT_NE(nullptr, back);
  EXPECT_EQ(1, (int)(*back)["a"]);
  EXPECT_EQ("two", (*back)["b"].str());
}

TEST(Value, MapWithNoElementsIsStillTruthyAndOtherTypesDontConvertToMap)
{
  // Map::empty() only checks whether the RefCount holds an allocated
  // pointer at all, not the wrapped unordered_map's element count - so an
  // allocated-but-empty map is truthy, unlike an empty Buffer or string.
  auto* map = new script::Value::Map::data_t[1]; // see the delete[] note above
  Value v{map, true};
  EXPECT_TRUE(v);

  Value notAMap{5};
  EXPECT_EQ(nullptr, (script::Value::Map::data_t*)notAMap);
}

TEST(Value, ScriptObjectPointerRoundTrip)
{
  // Value never dereferences the ScriptObject*, so an opaque non-null
  // pointer is enough to verify identity round-tripping without pulling in
  // the full ScriptObject definition.
  auto* fakeObject = reinterpret_cast<script::ScriptObject*>(0x1);

  Value v{fakeObject};
  EXPECT_EQ(Value::Type::OBJECT, v.type);
  EXPECT_TRUE(v);
  EXPECT_EQ(fakeObject, (script::ScriptObject*)v);

  Value notAnObject{5};
  EXPECT_EQ(nullptr, (script::ScriptObject*)notAnObject);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
