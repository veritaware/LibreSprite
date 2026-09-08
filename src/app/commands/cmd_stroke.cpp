// Besprited | Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/cmd_stroke.h"

#include "app/color.h"
#include "app/color_utils.h"
#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/modules/gui.h"
#include "app/pref/preferences.h"
#include "app/transaction.h"
#include "app/ui/color_bar.h"
#include "app/util/expand_cel_canvas.h"
#include "doc/image.h"
#include "doc/image_impl.h"
#include "doc/layer.h"
#include "doc/mask.h"
#include "doc/primitives.h"
#include "doc/sprite.h"

#include "stroke.xml.h"

#include <algorithm>
#include <vector>

namespace app {

using namespace doc;

class StrokeWindow : public app::gen::Stroke {
};

// Paints a pixel-perfect (non anti-aliased) band of the given width along
// the edge of the current selection, positioned inside/outside/centered on
// the selection edge, with the given color/opacity. Used by both Stroke and
// Quick Stroke. Declared in cmd_stroke.h.
void stroke_mask(Context* context, const app::Color& color, int opacity,
                 int width, app::gen::StrokePosition position,
                 const char* actionName)
{
  ContextWriter writer(context);
  Document* document = writer.document();
  Sprite* sprite = writer.sprite();
  Layer* layer = writer.layer();
  if (!layer || !layer->isImage())
    return;

  Mask* mask = document->mask();
  if (!mask->bitmap())
    return;

  width = std::max(1, width);

  // rOut is how far the stroke grows outside the selection edge, rIn is how
  // far it grows inside it (in the Chebyshev/square-neighborhood sense).
  int rOut, rIn;
  switch (position) {
    case app::gen::StrokePosition::OUTSIDE:
      rOut = width;
      rIn = 0;
      break;
    case app::gen::StrokePosition::CENTER:
      rIn = width / 2;
      rOut = width - rIn;
      break;
    case app::gen::StrokePosition::INSIDE:
    default:
      rOut = 0;
      rIn = width;
      break;
  }

  const int radius = std::max(rOut, rIn);
  gfx::Rect workBounds = mask->bounds();
  workBounds.enlarge(radius);

  gfx::Rect paintBounds = sprite->bounds() & workBounds;
  if (paintBounds.isEmpty())
    return;

  // Copy the selection into a plain local grid so the inner loops below can
  // do plain array look-ups instead of going through Mask::containsPoint()
  // (which re-checks bounds and calls into Image::getPixel() for every
  // neighbor we look at).
  std::vector<bool> sel(static_cast<std::size_t>(workBounds.w) * workBounds.h, false);
  auto selAt = [&](int x, int y) -> bool {
    const int lx = x - workBounds.x;
    const int ly = y - workBounds.y;
    if (lx < 0 || ly < 0 || lx >= workBounds.w || ly >= workBounds.h)
      return false;
    return sel[static_cast<std::size_t>(ly)*workBounds.w + lx];
  };
  {
    const gfx::Rect& maskBounds = mask->bounds();
    const LockImageBits<BitmapTraits> maskBits(mask->bitmap());
    auto it = maskBits.begin();
    for (int v=0; v<maskBounds.h; ++v) {
      for (int u=0; u<maskBounds.w; ++u, ++it) {
        if (*it) {
          int lx = maskBounds.x + u - workBounds.x;
          int ly = maskBounds.y + v - workBounds.y;
          sel[static_cast<std::size_t>(ly)*workBounds.w + lx] = true;
        }
      }
    }
  }

  color_t strokeColor = color_utils::color_for_layer(color, layer);

  Transaction transaction(writer.context(), actionName);
  {
    Site site = *writer.site();
    ExpandCelCanvas expand(site, layer, TiledMode::NONE, transaction,
                           ExpandCelCanvas::None);
    expand.validateDestCanvas(gfx::Region(paintBounds));
    Image* image = expand.getDestCanvas();

    // NOTE: like ModifySelectionCommand::applyModifier(), this is a
    // brute-force O(area * radius^2) neighborhood scan. It's not optimal,
    // but is good enough for the pixel-art canvas/stroke sizes this command
    // is meant for.
    for (int y=paintBounds.y; y<paintBounds.y2(); ++y) {
      for (int x=paintBounds.x; x<paintBounds.x2(); ++x) {
        bool outer = false;
        for (int dy=-rOut; dy<=rOut && !outer; ++dy)
          for (int dx=-rOut; dx<=rOut && !outer; ++dx)
            if (selAt(x+dx, y+dy))
              outer = true;
        if (!outer)
          continue;

        bool inner = true;
        for (int dy=-rIn; dy<=rIn && inner; ++dy)
          for (int dx=-rIn; dx<=rIn && inner; ++dx)
            if (!selAt(x+dx, y+dy))
              inner = false;

        if (!inner)
          blend_rect(image, x, y, x, y, strokeColor, opacity);
      }
    }

    expand.commit();
  }
  transaction.commit();

  update_screen_for_document(document);
}

class StrokeCommand : public Command {
public:
  StrokeCommand();
  [[nodiscard]] Command* clone() const override { return new StrokeCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

StrokeCommand::StrokeCommand()
  : Command("Stroke",
            "Stroke",
            CmdRecordableFlag)
{
}

bool StrokeCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::ActiveLayerIsEditable |
                             ContextFlags::ActiveLayerIsImage |
                             ContextFlags::HasVisibleMask);
}

void StrokeCommand::onExecute(Context* context)
{
  Preferences& pref = Preferences::instance();

  app::Color color = ColorBar::instance()->getFgColor();
  int opacity = pref.selection.strokeOpacity();
  int width = pref.selection.strokeWidth();
  app::gen::StrokePosition position = pref.selection.strokePosition();

  StrokeWindow window;
  window.color()->setColor(color);
  window.opacity()->setValue(opacity);
  window.width()->setValue(width);
  window.inside()->setSelected(position == app::gen::StrokePosition::INSIDE);
  window.center()->setSelected(position == app::gen::StrokePosition::CENTER);
  window.outside()->setSelected(position == app::gen::StrokePosition::OUTSIDE);

  window.openWindowInForeground();
  if (window.closer() != window.ok())
    return;

  color = window.color()->getColor();
  opacity = window.opacity()->getValue();
  width = window.width()->getValue();
  position = (window.center()->isSelected() ? app::gen::StrokePosition::CENTER:
             (window.outside()->isSelected() ? app::gen::StrokePosition::OUTSIDE:
                                               app::gen::StrokePosition::INSIDE));

  pref.selection.strokeOpacity(opacity);
  pref.selection.strokeWidth(width);
  pref.selection.strokePosition(position);

  stroke_mask(context, color, opacity, width, position, "Stroke Selection");
}

Command* CommandFactory::createStrokeCommand()
{
  return new StrokeCommand;
}

class QuickStrokeCommand : public Command {
public:
  QuickStrokeCommand();
  [[nodiscard]] Command* clone() const override { return new QuickStrokeCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

QuickStrokeCommand::QuickStrokeCommand()
  : Command("QuickStroke",
            "Quick Stroke",
            CmdRecordableFlag)
{
}

bool QuickStrokeCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::ActiveLayerIsEditable |
                             ContextFlags::ActiveLayerIsImage |
                             ContextFlags::HasVisibleMask);
}

void QuickStrokeCommand::onExecute(Context* context)
{
  stroke_mask(context, ColorBar::instance()->getFgColor(), 255, 1,
             app::gen::StrokePosition::INSIDE, "Quick Stroke Selection");
}

Command* CommandFactory::createQuickStrokeCommand()
{
  return new QuickStrokeCommand;
}

} // namespace app
