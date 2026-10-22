#pragma once

// Layout of the screen ("Briefing" design): a band at the top (header line
// with date and clock, a sentence and a weather picture) and three columns
// below it, without frames. Views get a canvas for their area.
//
// Every column has a size type. A widget's view is made for certain size
// types and is only placed in columns of those (see widgets/registry.h);
// otherwise the column shows a message instead of a clipped view.

#include <vector>

#include "ui/canvas.h"

namespace ui {

inline constexpr int kScreenWidth = 800;
inline constexpr int kScreenHeight = 480;

// Height of the band (the rule is its last row), of its header line and
// width of the part of the header line with date and clock; bottom edge of
// the column contents.
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

// Content sizes of the columns.
enum class SlotSize {
  kBriefingWide,    // departures, 264 x 259 (232 x 259 in the equal grid)
  kBriefingColumn,  // calendar and weather, 214 x 259 (232 x 259 in the equal grid)
};

// Name of a size type for messages: "wide column", "column".
const char* slotSizeName(SlotSize size);

struct Layout {
  Rect header;  // the band (header line, sentence, picture)
  // The columns ("slots"): they cover the screen below the band without gaps
  // and are what the display refreshes.
  std::vector<Rect> slots;
  std::vector<SlotSize> sizes;  // size type of every column
  // Area of every column the view draws into (without the margins).
  std::vector<Rect> content;
  // Areas of the band checked separately for changes (partial refresh): the
  // line with the clock, the rest of the header line, the rest of the band.
  std::vector<Rect> header_areas;
  // Dotted vertical lines between the columns (width 1), drawn by the
  // screen; they lie in the first pixel column of a slot.
  std::vector<Rect> dividers;
};

// Variants of the layout (configuration "layout.grid" and
// "layout.dividers"), off by default.
struct LayoutOptions {
  // Three columns of 232 px with gaps of 24 px instead of 264/214/214.
  bool equal_columns = false;
  // Dotted vertical lines between the columns.
  bool dividers = false;
};

// Content area of column i (see Layout::content).
Rect contentOf(const Layout& layout, size_t i);

// The band of 800 x 205 (header line, sentence, weather picture, rule at y
// 204) and three columns with margins of 28 px at the edges and 26 px
// between them: departures 264 px, calendar and weather 214 px each, from y
// 205 to the bottom edge of the content at y 464. With equal_columns all
// three are 232 px wide (24 px between them); the first one stays the "wide
// column" for the departures.
Layout screenLayout(const LayoutOptions& options = {});

// Top of the dotted lines between the columns (at the cap height of the
// first column title).
inline constexpr int kDividerTop = 222;

}  // namespace ui
