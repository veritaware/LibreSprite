// Besprited | Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/cmd_fill.h"

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

#include "fill.xml.h"

namespace app {

using namespace doc;

class FillWindow : public app::gen::Fill {
};

// Fills the pixels of the active layer/cel that fall inside the current
// selection with the given color/opacity. Used by both Fill and Quick Fill.
// Declared in cmd_fill.h.
void fill_mask(Context* context, const app::Color& color, int opacity,
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

  gfx::Rect fillBounds = sprite->bounds() & mask->bounds();
  if (fillBounds.isEmpty())
    return;

  color_t fillColor = color_utils::color_for_layer(color, layer);

  Transaction transaction(writer.context(), actionName);
  {
    Site site = *writer.site();
    ExpandCelCanvas expand(site, layer, TiledMode::NONE, transaction,
                           ExpandCelCanvas::None);
    expand.validateDestCanvas(gfx::Region(fillBounds));
    Image* image = expand.getDestCanvas();

    const gfx::Rect& maskBounds = mask->bounds();
    const LockImageBits<BitmapTraits> maskBits(mask->bitmap());
    auto it = maskBits.begin();

    for (int v=0; v<maskBounds.h; ++v) {
      for (int u=0; u<maskBounds.w; ++u, ++it) {
        if (*it) {
          int x = maskBounds.x + u;
          int y = maskBounds.y + v;
          if (fillBounds.contains(x, y))
            blend_rect(image, x, y, x, y, fillColor, opacity);
        }
      }
    }

    expand.commit();
  }
  transaction.commit();

  update_screen_for_document(document);
}

class FillCommand : public Command {
public:
  FillCommand();
  [[nodiscard]] Command* clone() const override { return new FillCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

FillCommand::FillCommand()
  : Command("Fill",
            "Fill",
            CmdRecordableFlag)
{
}

bool FillCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::ActiveLayerIsEditable |
                             ContextFlags::ActiveLayerIsImage |
                             ContextFlags::HasVisibleMask);
}

void FillCommand::onExecute(Context* context)
{
  Preferences& pref = Preferences::instance();

  app::Color color = ColorBar::instance()->getFgColor();
  int opacity = pref.selection.fillOpacity();

  FillWindow window;
  window.color()->setColor(color);
  window.opacity()->setValue(opacity);

  window.openWindowInForeground();
  if (window.closer() != window.ok())
    return;

  color = window.color()->getColor();
  opacity = window.opacity()->getValue();
  pref.selection.fillOpacity(opacity);

  fill_mask(context, color, opacity, "Fill Selection");
}

Command* CommandFactory::createFillCommand()
{
  return new FillCommand;
}

class QuickFillCommand : public Command {
public:
  QuickFillCommand();
  [[nodiscard]] Command* clone() const override { return new QuickFillCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

QuickFillCommand::QuickFillCommand()
  : Command("QuickFill",
            "Quick Fill",
            CmdRecordableFlag)
{
}

bool QuickFillCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::ActiveLayerIsEditable |
                             ContextFlags::ActiveLayerIsImage |
                             ContextFlags::HasVisibleMask);
}

void QuickFillCommand::onExecute(Context* context)
{
  fill_mask(context, ColorBar::instance()->getFgColor(), 255,
            "Quick Fill Selection");
}

Command* CommandFactory::createQuickFillCommand()
{
  return new QuickFillCommand;
}

} // namespace app
