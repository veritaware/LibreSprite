// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include <vector>

namespace app {

  // Pure guard checks used by RemoveLayerCommand::onExecute() before it
  // shows any confirmation dialog. Exposed here (rather than kept
  // file-local) so at least these decisions are directly testable - the
  // ui::Alert::show() confirmations themselves still need a live,
  // message-pumping ui::Manager and aren't yet injectable/mockable (see
  // issue #180), so the full command flow, including what happens after a
  // Yes/No choice, isn't reachable from a headless test.

  // True if removing the selected range would leave the sprite with no
  // layers at all.
  bool wouldRemoveAllLayers(int layersInRange, int totalLayers);

  // True if the sprite has only a single layer left to remove.
  bool wouldRemoveTheLastLayer(int totalLayers);

  // True if any of the given layers (by visibility, in any order) is
  // hidden - used to decide whether to warn before deleting it/them.
  bool anyLayerHidden(const std::vector<bool>& layerVisibility);

} // namespace app
