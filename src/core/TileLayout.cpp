#include "core/TileLayout.h"

#include <cmath>

namespace core {

namespace {

// Start of cell `i` when `total` pixels are split into `parts` cells.
int Edge(int total, int parts, int i) {
  return static_cast<int>(static_cast<long long>(total) * i / parts);
}

Rect Cell(const Rect &area, int cols, int rows, int col, int row) {
  int x0 = Edge(area.w, cols, col), x1 = Edge(area.w, cols, col + 1);
  int y0 = Edge(area.h, rows, row), y1 = Edge(area.h, rows, row + 1);
  return {area.x + x0, area.y + y0, x1 - x0, y1 - y0};
}

} // namespace

TilePlan PlanTiles(int count, const Rect &area) {
  TilePlan plan;
  if (count <= 0)
    return plan;

  if (count == 1) {
    plan.rects.push_back(area);
    plan.maximizeSingle = true;
    return plan;
  }
  if (count == 2) {
    plan.rects.push_back(Cell(area, 2, 1, 0, 0));
    plan.rects.push_back(Cell(area, 2, 1, 1, 0));
    return plan;
  }
  if (count == 3) {
    plan.rects.push_back(Cell(area, 2, 1, 0, 0));
    plan.rects.push_back(Cell(area, 2, 2, 1, 0));
    plan.rects.push_back(Cell(area, 2, 2, 1, 1));
    return plan;
  }

  int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
  int rows = (count + cols - 1) / cols;
  for (int i = 0; i < count; ++i)
    plan.rects.push_back(Cell(area, cols, rows, i % cols, i / cols));
  return plan;
}

} // namespace core
