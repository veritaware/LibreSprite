// Aseprite  | Copyright (C) 2001-2015 David Capello
// Besprited | Copyright (C) 2026      Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/flatten_layers.h"

#include "app/cmd/add_layer.h"
#include "app/cmd/set_layer_name.h"
#include "app/cmd/configure_background.h"
#include "app/cmd/copy_rect.h"
#include "app/cmd/remove_layer.h"
#include "app/cmd/remove_layer.h"
#include "app/cmd/set_layer_flags.h"
#include "app/cmd/unlink_cel.h"
#include "app/document.h"
#include "doc/blend_mode.h"
#include "doc/cel.h"
#include "doc/layer.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "render/render.h"

namespace app {
namespace cmd {

FlattenLayers::FlattenLayers(Sprite* sprite)
  : WithSprite(sprite)
{
}

void FlattenLayers::onExecute()
{
  Sprite* sprite = this->sprite();
  app::Document* doc = static_cast<app::Document*>(sprite->document());

  // Create a temporary image. Its mask color must match the sprite's
  // transparent color, as the image is composited onto the background
  // color below (indexed images use it to know which pixels are empty).
  ImageRef image(Image::create(sprite->pixelFormat(),
      sprite->width(),
      sprite->height()));
  image->setMaskColor(sprite->transparentColor());

  LayerImage* flatLayer;  // The layer onto which everything will be flattened.
  color_t     bgcolor;    // The background color to use for flatLayer.
  bool        ontoBackground;

  flatLayer = sprite->backgroundLayer();
  if (flatLayer && flatLayer->isVisible()) {
    // There exists a visible background layer, so we will flatten onto that.
    bgcolor = doc->bgColor(flatLayer);
    ontoBackground = true;
  }
  else {
    // Create a new transparent layer to flatten everything onto.
    flatLayer = new LayerImage(sprite);
    ASSERT(flatLayer->isVisible());
    executeAndAdd(new cmd::AddLayer(sprite->folder(), flatLayer, nullptr));
    executeAndAdd(new cmd::SetLayerName(flatLayer, "Flattened"));
    bgcolor = sprite->transparentColor();
    ontoBackground = false;
  }

  // Buffer to composite the background color underneath the rendered
  // frame (see below). It's only needed when flattening onto a
  // background layer, as bgcolor is the transparent color otherwise.
  ImageRef bgImage;
  if (ontoBackground)
    bgImage.reset(Image::create(sprite->pixelFormat(),
        sprite->width(),
        sprite->height()));

  render::Render render;
  render.setBgType(render::BgType::NONE);

  // Copy all frames to the background.
  for (frame_t frame(0); frame<sprite->totalFrames(); ++frame) {
    // Render this frame onto an empty backdrop. Pre-filling the buffer
    // with the background color would hide the sprite's real per-pixel
    // alpha from the layers' blend functions, which fall back to Normal
    // blending only where the backdrop is empty (see doc/blend_funcs.cpp),
    // so every special-blend layer would end up blended against the
    // background color instead of against "no content".
    clear_image(image.get(), sprite->transparentColor());
    render.renderSprite(image.get(), sprite, frame);

    // Now that the frame is composited with its own alpha, drop the
    // background color in behind it.
    Image* flatImage = image.get();
    if (bgImage) {
      clear_image(bgImage.get(), bgcolor);
      render::composite_image(bgImage.get(), image.get(),
          sprite->palette(frame), 0, 0, 255, BlendMode::NORMAL);
      flatImage = bgImage.get();
    }

    // TODO Keep cel links when possible

    ImageRef cel_image;
    auto cel = flatLayer->cel(frame);
    if (cel) {
      if (cel->links())
        executeAndAdd(new cmd::UnlinkCel(cel));

      cel_image = cel->imageRef();
      ASSERT(cel_image);

      executeAndAdd(new cmd::CopyRect(cel_image.get(), flatImage,
          gfx::Clip(0, 0, flatImage->bounds())));
    }
    else {
      cel_image.reset(Image::createCopy(flatImage));
      cel = std::make_shared<Cel>(frame, cel_image);
      flatLayer->addCel(cel);
    }
  }

  // Delete old layers.
  LayerList layers = sprite->folder()->getLayersList();
  for (Layer* layer : layers)
    if (layer != flatLayer)
      executeAndAdd(new cmd::RemoveLayer(layer));
}

} // namespace cmd
} // namespace app
