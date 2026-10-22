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

// "briefing": height of the band (the rule is its last row), of its header
// line and width of the part of the header line with date and clock; bottom
// edge of the column contents.
inline constexpr int kBriefingBandHeight = 205;
inline constexpr int kBriefingHeaderHeight = 44;
inline constexpr int kBriefingClockAreaWidth = 600;
inline constexpr int kBriefingContentBottom = 464;

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
  // "briefing": columns without frames below the band (content sizes).
  kBriefingWide,    // departures, 264 x 259
  kBriefingColumn,  // calendar and weather, 214 x 259
};

// Name of a size type for messages: "column", "tile", "narrow column".
const char* slotSizeName(SlotSize size);

struct Layout {
  Rect header;  // for "briefing": the band (header line, sentence, picture)
  std::vector<Rect> slots;
  std::vector<SlotSize> sizes;   // size type of every slot
  std::vector<Rect> separators;  // black lines between the areas
  // Area of every slot the view draws into, if smaller than the slot (the
  // margins of "briefing"); empty = the slots themselves.
  std::vector<Rect> content;
  // Areas of the header checked separately for changes (partial refresh),
  // e.g. the line with the clock; empty = the header as one area.
  std::vector<Rect> header_areas;
  // The "Briefing" design: band instead of a header, columns without frames.
  bool briefing = false;
};

// Content area of slot i (see Layout::content).
Rect contentOf(const Layout& layout, size_t i);

// "one_plus_two": header, a 358 px column on the left (360 px with its
// separator) and two 440 x 218 tiles on the right.
// "three_columns": header and columns of 358, 219 and 219 px (column and
// two narrow columns).
// "briefing": band of 800 x 205 (header line, sentence, weather picture,
// rule at y 204) and three columns with margins of 28 px at the edges and 26
// px between them: departures 264 px, calendar and weather 214 px each, from
// y 205 to the bottom edge of the content at y 464.
// Unknown names give "one_plus_two".
Layout layoutFor(std::string_view name);

void drawSeparators(Canvas& canvas, const Layout& layout);

}  // namespace ui
