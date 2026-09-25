#pragma once

// Grid layout for tiling windows. Pure arithmetic, no Win32, so it is unit
// tested on any platform (tests/test_core.cpp).

#include <vector>

namespace core {

struct Rect {
  int x, y, w, h;
  bool operator==(const Rect &o) const {
    return x == o.x && y == o.y && w == o.w && h == o.h;
  }
};

struct TilePlan {
  // One rect per window, in the order given. Empty when count <= 0.
  std::vector<Rect> rects;
  // A single window is maximized rather than moved.
  bool maximizeSingle = false;
};

// Layout for `count` windows inside `area` (the monitor's work area):
//   1 -> maximize, 2 -> left/right halves, 3 -> left half + two stacked
//   quarters, otherwise a near-square grid (cols = ceil(sqrt(n))).
// Cells share out the leftover pixels, so the tiles cover the area exactly
// with no gap at the right or bottom edge.
TilePlan PlanTiles(int count, const Rect &area);

} // namespace core
