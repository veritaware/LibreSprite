// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#define TEST_GUI
#include "tests.h"

#include "app/app.h"
#include "app/commands/commands.h"
#include "doc/color.h"
#include "doc/image.h"
#include "doc/primitives.h"
#include "she/system.h"
#include "script/engine.h"
#include "script/engine_delegate.h"
#include "script/script_object.h"

#include <cstring>
#include <memory>
#include <string>

using script::Engine;
using script::EngineDelegate;
using script::ScriptObject;
using script::Value;

namespace {

// Bridges the test's C++ side and JS: "native.capture(x)" records whatever
// JS passes so the test can assert on it afterwards, and "native.image"
// hands a wrapped doc::Image (set by the test beforehand) to JS the same
// way AppScriptObject::activeImage does for the real active document.
class TestBridgeScriptObject : public ScriptObject {
public:
  Value captured;
  doc::Image* image = nullptr;

  TestBridgeScriptObject() {
    addFunction("capture", [this](Value v) { captured = v; return Value{}; });
    addProperty("image", [this] { return getEngine()->getScriptObject(image); });
    makeGlobal("native");
  }
};

// app's script API objects tagged {"global"} (AppScriptObject "app",
// ColorModeScriptObject "ColorMode", StorageScriptObject "storage", ...) are
// constructed eagerly by Engine::initGlobals() on the first eval() of every
// test - and AppScriptObject eagerly constructs a CommandScriptObject
// ("app.command"), whose constructor iterates every registered Command via
// app::CommandsModule::instance(), which is otherwise null. Like
// fill_stroke_mask_tests.cpp's App, this is deliberately leaked and
// constructed at most once for the whole binary (CommandsModule's own
// ASSERT(m_instance == NULL) means it can't safely be built more than once
// per process either).
void ensureAppAndCommands()
{
  static app::App* app = new app::App();
  app->initializeCoreModulesForTesting();
  static app::CommandsModule* commands = new app::CommandsModule();
  (void)commands;
}

class AppScriptApiTest : public ::testing::Test {
protected:
  inject<Engine> engine{"qjs"};
  TestBridgeScriptObject bridge; // global "native"; constructed after `engine` (declaration order)

  // Must run before any fixture is constructed, same reasoning as
  // test/script/engine_tests.cpp's EngineTest::SetUpTestSuite().
  static void SetUpTestSuite() {
    ASSERT_TRUE(EngineDelegate::setDefault("stdout"));
    ensureAppAndCommands();
  }

  void SetUp() override {
    ASSERT_TRUE(engine);
  }
};

} // namespace

TEST_F(AppScriptApiTest, PixelColorRgbaRoundTripsThroughAllChannelAccessors)
{
  ASSERT_TRUE(engine->eval(
    "native.capture(app.pixelColor.rgba(10, 20, 30, 200));"));
  EXPECT_EQ(doc::rgba(10, 20, 30, 200), static_cast<doc::color_t>(int(bridge.captured)));

  ASSERT_TRUE(engine->eval(
    "var c = app.pixelColor.rgba(10, 20, 30, 200);"
    "native.capture([app.pixelColor.rgbaR(c), app.pixelColor.rgbaG(c),"
    "                app.pixelColor.rgbaB(c), app.pixelColor.rgbaA(c)].join(','));"));
  EXPECT_EQ("10,20,30,200", bridge.captured.str());
}

TEST_F(AppScriptApiTest, PixelColorGrayaRoundTrips)
{
  ASSERT_TRUE(engine->eval(
    "var c = app.pixelColor.graya(128, 64);"
    "native.capture([app.pixelColor.grayaV(c), app.pixelColor.grayaA(c)].join(','));"));
  EXPECT_EQ("128,64", bridge.captured.str());
}

TEST_F(AppScriptApiTest, ColorModeConstantsMatchTheDocPixelFormatEnum)
{
  ASSERT_TRUE(engine->eval(
    "native.capture([ColorMode.RGB, ColorMode.GRAYSCALE, ColorMode.INDEXED, ColorMode.BITMAP].join(','));"));
  std::string expected =
    std::to_string(int(doc::IMAGE_RGB)) + "," +
    std::to_string(int(doc::IMAGE_GRAYSCALE)) + "," +
    std::to_string(int(doc::IMAGE_INDEXED)) + "," +
    std::to_string(int(doc::IMAGE_BITMAP));
  EXPECT_EQ(expected, bridge.captured.str());
}

TEST_F(AppScriptApiTest, AppVersionAndPlatformAreNonEmptyStrings)
{
  ASSERT_TRUE(engine->eval("native.capture(app.version);"));
  EXPECT_FALSE(bridge.captured.str().empty());

  ASSERT_TRUE(engine->eval("native.capture(app.platform);"));
  EXPECT_FALSE(bridge.captured.str().empty());
}

TEST_F(AppScriptApiTest, CommandSetParameterAndClearParametersAreChainable)
{
  // No UIContext exists in this headless test, so actually running a
  // command would just no-op (CommandScriptObject checks `if (!ctx) return
  // 0;`) - this only pins that setParameter()/clearParameters() return
  // something that itself has "clearParameters" (i.e. another command-like
  // object), so JS call chains like
  // app.command.setParameter(...).clearParameters() work without throwing.
  ASSERT_TRUE(engine->eval(
    "native.capture(typeof app.command.setParameter('a', 'b').clearParameters);"));
  EXPECT_EQ("function", bridge.captured.str());
}

TEST_F(AppScriptApiTest, ImageGetPixelPutPixelRoundTrip)
{
  std::unique_ptr<doc::Image> img(doc::Image::create(doc::IMAGE_RGB, 4, 4));
  doc::clear_image(img.get(), 0);
  bridge.image = img.get();

  ASSERT_TRUE(engine->eval(
    "native.image.putPixel(1, 1, app.pixelColor.rgba(5, 6, 7, 255));"
    "native.capture(native.image.getPixel(1, 1));"));
  EXPECT_EQ(doc::rgba(5, 6, 7, 255), static_cast<doc::color_t>(int(bridge.captured)));
}

TEST_F(AppScriptApiTest, ImagePutImageDataRejectsAWronglySizedBufferWithoutCorruptingTheImage)
{
  std::unique_ptr<doc::Image> img(doc::Image::create(doc::IMAGE_RGB, 4, 4));
  doc::clear_image(img.get(), doc::rgba(1, 2, 3, 4));
  bridge.image = img.get();

  // A 1-byte buffer can never match a 4x4 RGBA image's byte size - the
  // size-mismatch guard in ImageScriptObject::putImageData() must reject it
  // and leave every pixel exactly as it was.
  ASSERT_TRUE(engine->eval("native.image.putImageData(new Uint8Array(1));"));

  EXPECT_EQ(doc::rgba(1, 2, 3, 4), img->getPixel(0, 0));
  EXPECT_EQ(doc::rgba(1, 2, 3, 4), img->getPixel(3, 3));
}

TEST_F(AppScriptApiTest, ImageGetImageDataThenPutImageDataRoundTripsEveryPixel)
{
  std::unique_ptr<doc::Image> img(doc::Image::create(doc::IMAGE_RGB, 2, 2));
  img->putPixel(0, 0, doc::rgba(1, 2, 3, 255));
  img->putPixel(1, 0, doc::rgba(4, 5, 6, 255));
  img->putPixel(0, 1, doc::rgba(7, 8, 9, 255));
  img->putPixel(1, 1, doc::rgba(10, 11, 12, 255));
  bridge.image = img.get();

  // Correctly-sized data must be accepted: round trip through JS, then
  // clear the image natively and write the captured bytes straight back.
  ASSERT_TRUE(engine->eval("native.capture(native.image.getImageData());"));

  doc::clear_image(img.get(), 0);
  ASSERT_NE(0u, bridge.captured.buffer().size());
  std::memcpy(img->getPixelAddress(0, 0), bridge.captured.buffer().data(),
              bridge.captured.buffer().size());

  EXPECT_EQ(doc::rgba(1, 2, 3, 255), img->getPixel(0, 0));
  EXPECT_EQ(doc::rgba(4, 5, 6, 255), img->getPixel(1, 0));
  EXPECT_EQ(doc::rgba(7, 8, 9, 255), img->getPixel(0, 1));
  EXPECT_EQ(doc::rgba(10, 11, 12, 255), img->getPixel(1, 1));
}

TEST_F(AppScriptApiTest, ImageGetPNGDataReturnsABase64PngDataUri)
{
  // getPNGData() goes through she::instance() (createRgbaSurface,
  // encodeSurfaceAsPNG) - TEST_GUI's ui::Manager deliberately doesn't create
  // a she::System (see tests.h), so this needs its own, scoped to this test.
  std::unique_ptr<she::System> sys(she::create_system());

  std::unique_ptr<doc::Image> img(doc::Image::create(doc::IMAGE_RGB, 2, 2));
  doc::clear_image(img.get(), doc::rgba(9, 9, 9, 255));
  bridge.image = img.get();

  ASSERT_TRUE(engine->eval("native.capture(native.image.getPNGData());"));

  std::string uri = bridge.captured.str();
  const std::string prefix = "data:image/png;base64,";
  ASSERT_GT(uri.size(), prefix.size());
  EXPECT_EQ(prefix, uri.substr(0, prefix.size()));
  EXPECT_GT(uri.size(), prefix.size() + 8) << "should carry actual encoded PNG data, not just the prefix";
}
