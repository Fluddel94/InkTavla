#pragma once

// Layout templates: they divide the screen into a header and slots without
// gaps. Views get a canvas for their slot; the separator lines between the
// areas are drawn by drawSeparators().
//
// Every slot has a size type. A widget's view is made for certain size
// types and is only placed on slots of those (see widgets/registry.h);
// otherwise the slot shows a message instead of a clipped view.

#include <string_view>
#include <vector>

#include "ui/canvas.h"

namespace ui {

inline constexpr int kScreenWidth = 800;
inline constexpr int kScreenHeight = 480;

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

enum class SlotSize {
  kColumn,        // full-height column, 358 x 438
  kTile,          // wide tile, 440 x 218
  kNarrowColumn,  // narrow full-height column, 219 x 438
};

// Name of a size type for messages: "column", "tile", "narrow column".
const char* slotSizeName(SlotSize size);

struct Layout {
  Rect header;
  std::vector<Rect> slots;
  std::vector<SlotSize> sizes;   // size type of every slot
  std::vector<Rect> separators;  // black lines between the areas
};

// "one_plus_two": header, a 358 px column on the left (360 px with its
// separator) and two 440 x 218 tiles on the right.
// "three_columns": header and columns of 358, 219 and 219 px (column and
// two narrow columns).
// Unknown names give "one_plus_two".
Layout layoutFor(std::string_view name);

void drawSeparators(Canvas& canvas, const Layout& layout);

}  // namespace ui
