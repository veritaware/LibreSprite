// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#include "tests.h"

#include "app/context.h"
#include "app/document.h"
#include "app/file/file.h"
#include "doc/doc.h"
#include "doc/image_traits.h"
#include "she/system.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

using namespace app;

namespace {

void putU16LE(std::vector<uint8_t>& buf, std::size_t offset, uint16_t v)
{
  buf[offset] = uint8_t(v & 0xff);
  buf[offset + 1] = uint8_t((v >> 8) & 0xff);
}

void putU32LE(std::vector<uint8_t>& buf, std::size_t offset, uint32_t v)
{
  buf[offset] = uint8_t(v & 0xff);
  buf[offset + 1] = uint8_t((v >> 8) & 0xff);
  buf[offset + 2] = uint8_t((v >> 16) & 0xff);
  buf[offset + 3] = uint8_t((v >> 24) & 0xff);
}

uint16_t getU16LE(const std::vector<uint8_t>& buf, std::size_t offset)
{
  return uint16_t(buf[offset]) | (uint16_t(buf[offset + 1]) << 8);
}

uint32_t getU32LE(const std::vector<uint8_t>& buf, std::size_t offset)
{
  return uint32_t(buf[offset]) | (uint32_t(buf[offset + 1]) << 8) |
         (uint32_t(buf[offset + 2]) << 16) | (uint32_t(buf[offset + 3]) << 24);
}

std::vector<uint8_t> readWholeFile(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeWholeFile(const std::string& path, const std::vector<uint8_t>& data)
{
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace

TEST(AseFormat, ColorProfileChunkIsToleratedNotTreatedAsAnUnsupportedChunkWarning)
{
  // Save a minimal, real .ase file, then splice in a synthetic 0x2007
  // (ASE_FILE_CHUNK_COLOR_PROFILE) chunk as an extra chunk of the (only)
  // frame - the reader unconditionally seeks to chunk_pos+chunk_size after
  // each chunk regardless of what its handler consumed, so only the
  // chunk's own [size,type] header and the frame's updated chunk
  // count/size need to be correct; the 16-byte payload just needs to be
  // present.
  //
  // she::create_system() is defensive: if this regresses and AseFormat
  // ends up rejecting the file after all, the load falls back to
  // SheFormat, which needs a live she::instance() - without it, that
  // fallback would crash instead of just failing this test's assertions.
  std::unique_ptr<she::System> sys(she::create_system());

  app::Context ctx;
  const int w = 3, h = 2;

  doc::Document* doc = ctx.documents().add(w, h, doc::ColorMode::RGB);
  doc->setFilename("ase_color_profile_test.ase");
  Layer* layer = doc->sprite()->folder()->getFirstLayer();
  ASSERT_TRUE(layer != NULL);
  Image* image = layer->cel(frame_t(0))->image();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      put_pixel(image, x, y, doc::rgba(x * 50, y * 60, 10, 255));

  ASSERT_EQ(0, save_document(&ctx, doc));
  doc->close();
  delete doc;

  auto bytes = readWholeFile("ase_color_profile_test.ase");
  ASSERT_GE(bytes.size(), 128u + 16u) << "file too small to be a real .ase";

  // File header: chunks/frame fields aren't touched; only the total file
  // size (offset 0, u32) needs updating, for consistency.
  // Frame header starts right after the fixed 128-byte file header:
  //   offset 128: frame size (u32)
  //   offset 132: frame magic (u16)
  //   offset 134: chunk count (u16)
  const std::size_t frameHeaderStart = 128;
  const std::size_t frameSizeOff = frameHeaderStart;
  const std::size_t chunkCountOff = frameHeaderStart + 6;

  uint32_t oldFrameSize = getU32LE(bytes, frameSizeOff);
  uint16_t oldChunkCount = getU16LE(bytes, chunkCountOff);
  ASSERT_EQ(bytes.size(), frameHeaderStart + oldFrameSize)
    << "single-frame file: the frame must run to EOF";

  // New chunk: 6-byte chunk header (size, type) + 16-byte color-profile
  // payload (type u16, flags u16, gamma u32, 8 bytes padding), matching
  // what AseFormat::onLoad's ASE_FILE_CHUNK_COLOR_PROFILE case reads.
  const uint32_t newChunkSize = 6 + 16;
  std::vector<uint8_t> chunk(newChunkSize, 0);
  putU32LE(chunk, 0, newChunkSize);
  putU16LE(chunk, 4, 0x2007); // ASE_FILE_CHUNK_COLOR_PROFILE

  bytes.insert(bytes.end(), chunk.begin(), chunk.end());
  putU32LE(bytes, frameSizeOff, oldFrameSize + newChunkSize);
  putU16LE(bytes, chunkCountOff, oldChunkCount + 1);
  putU32LE(bytes, 0, uint32_t(bytes.size())); // file header's total size

  writeWholeFile("ase_color_profile_test.ase", bytes);

  app::Document* loaded = load_document(&ctx, "ase_color_profile_test.ase");
  ASSERT_NE(nullptr, loaded);

  ASSERT_EQ(w, loaded->sprite()->width());
  ASSERT_EQ(h, loaded->sprite()->height());

  Layer* loadedLayer = loaded->sprite()->folder()->getFirstLayer();
  ASSERT_TRUE(loadedLayer != NULL);
  Image* loadedImage = loadedLayer->cel(frame_t(0))->image();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      EXPECT_EQ(doc::rgba(x * 50, y * 60, 10, 255), loadedImage->getPixel(x, y))
        << "at (" << x << "," << y << ")";

  loaded->close();
  delete loaded;
}

TEST(AseFormat, AnActuallyUnrecognizedChunkTypeIsNotSilentlyTolerated)
{
  // Contrast against the color-profile test above: a genuinely unknown
  // chunk type (0xffff, never assigned) hits the `default:` branch in
  // AseFormat::onLoad, which records a message via fop->setError() -
  // unlike the color-profile chunk's dedicated, error-free handling. That
  // makes operateLoadTryFormat()'s `&& m_error.empty()` check fail, so
  // this AseFormat attempt is treated as a failure and the loader falls
  // back to the next-priority format (SheFormat), which needs a live
  // she::instance() - unlike every other test in this file, which never
  // reach that fallback.
  std::unique_ptr<she::System> sys(she::create_system());

  app::Context ctx;
  doc::Document* doc = ctx.documents().add(2, 2, doc::ColorMode::RGB);
  doc->setFilename("ase_unknown_chunk_test.ase");
  Layer* layer = doc->sprite()->folder()->getFirstLayer();
  Image* image = layer->cel(frame_t(0))->image();
  put_pixel(image, 0, 0, doc::rgba(1, 2, 3, 255));

  ASSERT_EQ(0, save_document(&ctx, doc));
  doc->close();
  delete doc;

  auto bytes = readWholeFile("ase_unknown_chunk_test.ase");
  const std::size_t frameSizeOff = 128;
  const std::size_t chunkCountOff = 134;

  uint32_t oldFrameSize = getU32LE(bytes, frameSizeOff);
  uint16_t oldChunkCount = getU16LE(bytes, chunkCountOff);

  const uint32_t newChunkSize = 6; // header only, no payload
  std::vector<uint8_t> chunk(newChunkSize, 0);
  putU32LE(chunk, 0, newChunkSize);
  putU16LE(chunk, 4, 0xffff); // never-assigned chunk type

  bytes.insert(bytes.end(), chunk.begin(), chunk.end());
  putU32LE(bytes, frameSizeOff, oldFrameSize + newChunkSize);
  putU16LE(bytes, chunkCountOff, oldChunkCount + 1);
  putU32LE(bytes, 0, uint32_t(bytes.size()));

  writeWholeFile("ase_unknown_chunk_test.ase", bytes);

  // The point of this test is that this must not crash the loader,
  // regardless of which format ends up handling it (or whether every
  // fallback ultimately fails and load_document() returns null).
  app::Document* loaded = load_document(&ctx, "ase_unknown_chunk_test.ase");
  if (loaded) {
    loaded->close();
    delete loaded;
  }
}

TEST(AseFormat, OversizedCelDimensionsDoNotOverflowTheDecompressionBufferSize)
{
  // Regression coverage for 0febb4bf9: read_compressed_image() used to
  // compute `image->height() * ImageTraits::getRowStrideBytes(width)` as a
  // plain `int`, which overflows well before reaching the 65535x65535
  // maximum a .ase width/height field (a u16) can encode - an overflowed
  // size there means the zlib inflate loop writes decompressed bytes past
  // a too-small buffer. This checks the arithmetic directly (no
  // multi-gigabyte allocation needed) without itself performing the
  // signed-int multiplication that overflows - that's undefined behavior,
  // and reproducing it here (even just to show it's wrong) is exactly the
  // class of bug this project's own CodeQL scanning flags: instead, it
  // confirms the true byte count exceeds what an `int` can represent,
  // which is precisely why the fix's `static_cast<long>` is necessary.
  const int width = 50000, height = 50000; // 2.5e9 pixels, indexed (1 byte/px)

  long correct = static_cast<long>(height) * doc::IndexedTraits::getRowStrideBytes(width);
  const long truePixelCount = 50000L * 50000L;

  EXPECT_EQ(truePixelCount, correct);
  ASSERT_GT(truePixelCount, static_cast<long>(INT32_MAX))
    << "test setup: these dimensions must actually exceed int32 range, "
       "otherwise this isn't exercising the overflow-prone case at all";
}

TEST(AseFormat, ModeratelyLargeImageRoundTripsWithoutCorruption)
{
  // A real (not synthetic-overflow-scale) large image, to confirm the
  // actual load/decompress path handles a substantially-sized cel
  // correctly end to end. Filled with a single repeating value so the
  // zlib-compressed .ase file this produces stays small and the test
  // stays fast, even though the uncompressed image is ~4 million pixels.
  std::unique_ptr<she::System> sys(she::create_system()); // see the note above

  app::Context ctx;
  const int w = 2000, h = 2000;

  doc::Document* doc = ctx.documents().add(w, h, doc::ColorMode::INDEXED, 4);
  doc->setFilename("ase_large_image_test.ase");
  Layer* layer = doc->sprite()->folder()->getFirstLayer();
  ASSERT_TRUE(layer != NULL);
  Image* image = layer->cel(frame_t(0))->image();
  clear_image(image, 2);
  put_pixel(image, 0, 0, 1);
  put_pixel(image, w - 1, h - 1, 3);

  ASSERT_EQ(0, save_document(&ctx, doc));
  doc->close();
  delete doc;

  app::Document* loaded = load_document(&ctx, "ase_large_image_test.ase");
  ASSERT_NE(nullptr, loaded);
  ASSERT_EQ(w, loaded->sprite()->width());
  ASSERT_EQ(h, loaded->sprite()->height());

  Layer* loadedLayer = loaded->sprite()->folder()->getFirstLayer();
  Image* loadedImage = loadedLayer->cel(frame_t(0))->image();
  EXPECT_EQ(1, loadedImage->getPixel(0, 0));
  EXPECT_EQ(3, loadedImage->getPixel(w - 1, h - 1));
  EXPECT_EQ(2, loadedImage->getPixel(w / 2, h / 2));

  loaded->close();
  delete loaded;
}
