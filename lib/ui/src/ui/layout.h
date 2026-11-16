#pragma once

// Layout of the screen ("Briefing" design): a band at the top (header line
// with date and clock, a sentence and a weather picture) and two or three
// columns below it, without frames. Views get a canvas for their area.
//
// The columns only come in three widths, the size steps S, M and L. A widget
// names the smallest step its view is made for (see widgets/registry.h) and
// is only placed in columns of at least that step; otherwise the column
// shows a message instead of a clipped view.

#include <vector>

#include "ui/canvas.h"

namespace ui {

inline constexpr int kScreenWidth = 800;
inline constexpr int kScreenHeight = 480;

// Margins of the screen: the band and the columns keep this distance from
// the left, right and top edge (the date line starts at its height), and
// kMarginBottom from the bottom edge. The numbers to change if the case
// covers part of the panel; the columns, slots and dividers follow from them
// (the departures column takes what is left).
inline constexpr int kMargin = 20;
inline constexpr int kMarginBottom = 15;

// Height of the band (the rule is its last row), of its header line and
// width of the part of the header line with date and clock; bottom edge of
// the column contents.
inline constexpr int kBriefingBandHeight = 205;
inline constexpr int kBriefingHeaderHeight = 44;
inline constexpr int kBriefingClockAreaWidth = 600;
inline constexpr int kBriefingContentBottom = kScreenHeight - kMarginBottom;

// Widths of the columns (size steps) and the gap between two columns. Band
// + 3 has one column of M and two of S, band + 2 two of L. S is fixed, M and
// L follow from the margins: M is what two columns of S leave, L half of the
// content width.
inline constexpr int kColumnGap = 26;
inline constexpr int kContentWidth = kScreenWidth - 2 * kMargin;
inline constexpr int kStepWidthS = 214;
inline constexpr int kStepWidthM = kContentWidth - 2 * kStepWidthS - 2 * kColumnGap;
inline constexpr int kStepWidthL = (kContentWidth - kColumnGap) / 2;

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

// Width steps of the columns, from narrow to wide.
enum class SizeStep {
  kS,  // 214 x 260
  kM,  // 280 x 260
  kL,  // 367 x 260
};

// Content width of a column of this step in pixels.
int stepWidth(SizeStep step);

// Name of a step: "S", "M", "L".
const char* sizeStepName(SizeStep step);

struct Layout {
  Rect header;  // the band (header line, sentence, picture)
  // The columns ("slots"): they cover the screen below the band without gaps
  // and are what the display refreshes.
  std::vector<Rect> slots;
  std::vector<SizeStep> steps;  // size step of every column
  // Area of every column the view draws into (without the margins).
  std::vector<Rect> content;
  // Areas of the band checked separately for changes (partial refresh): the
  // line with the clock, the rest of the header line, the rest of the band.
  std::vector<Rect> header_areas;
  // Dotted vertical lines between the columns (width 1), drawn by the
  // screen; they lie in the first pixel column of a slot.
  std::vector<Rect> dividers;
};

// Number of columns of the two layouts.
inline constexpr int kMinColumns = 2;
inline constexpr int kMaxColumns = 3;

// What the configuration chooses ("layout.slots" and "layout.dividers").
struct LayoutOptions {
  // Number of columns: 3 (band + 3, one column of M and two of S) or 2
  // (band + 2, two columns of L).
  int columns = 3;
  // Band + 3: index of the column that is M wide (0 to 2). It is where the
  // widget sits that needs M, see widgets::layoutOptions().
  int wide_column = 0;
  // Dotted vertical lines between the columns (on by default).
  bool dividers = true;
};

// Content area of column i (see Layout::content).
Rect contentOf(const Layout& layout, size_t i);

// The band of 800 x 205 (header line, sentence, weather picture, rule at y
// 204) and the columns below it, from y 205 to the bottom edge of the
// content at y 465 (kMarginBottom 15 px), with the margin kMargin (20 px) at
// the sides and 26 px between them:
//   band + 3: 280 | 214 | 214 px, the 280 at options.wide_column
//   band + 2: 367 | 367 px
// The slots (refresh areas) split the gaps between the columns.
Layout screenLayout(const LayoutOptions& options = {});

// Top of the dotted lines between the columns (at the cap height of the
// first column title).
inline constexpr int kDividerTop = 222;

}  // namespace ui
