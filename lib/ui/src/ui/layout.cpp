#include "ui/layout.h"

namespace ui {

namespace {

constexpr int kHeaderHeight = 40;
constexpr int kLine = 2;  // separator thickness
constexpr int kBodyTop = kHeaderHeight + kLine;
constexpr int kBodyHeight = kScreenHeight - kBodyTop;
constexpr int kColumnWidth = 358;  // departures column, 45 % with separator

}  // namespace

Rect contentOf(const Layout& layout, size_t i) {
  if (i < layout.content.size()) return layout.content[i];
  return i < layout.slots.size() ? layout.slots[i] : Rect{};
}

const char* slotSizeName(SlotSize size) {
  switch (size) {
    case SlotSize::kBriefingWide:
      return "briefing wide column";
    case SlotSize::kBriefingColumn:
      return "briefing column";
    case SlotSize::kColumn:
      return "column";
    case SlotSize::kTile:
      return "tile";
    case SlotSize::kNarrowColumn:
      return "narrow column";
  }
  return "";
}

Layout layoutFor(std::string_view name) {
  Layout l;
  if (name == "briefing") {
    // Band; the rule at y 204 is drawn by the band's view.
    l.briefing = true;
    l.header = {0, 0, kScreenWidth, kBriefingBandHeight};
    // The header line (date, clock) changes every minute; the weather picture
    // on the right reaches into its height.
    l.header_areas = {
        {0, 0, kBriefingClockAreaWidth, kBriefingHeaderHeight},
        {kBriefingClockAreaWidth, 0, kScreenWidth - kBriefingClockAreaWidth, kBriefingHeaderHeight},
        {0, kBriefingHeaderHeight, kScreenWidth, kBriefingBandHeight - kBriefingHeaderHeight}};
    const int top = kBriefingBandHeight;
    const int h = kScreenHeight - top;
    const int content_h = kBriefingContentBottom - top;
    // Columns: 28-292, 318-532, 558-772; the slots split the gaps.
    l.slots = {{0, top, 305, h}, {305, top, 240, h}, {545, top, kScreenWidth - 545, h}};
    l.content = {{28, top, 264, content_h}, {318, top, 214, content_h}, {558, top, 214, content_h}};
    l.sizes = {SlotSize::kBriefingWide, SlotSize::kBriefingColumn, SlotSize::kBriefingColumn};
    return l;
  }
  l.header = {0, 0, kScreenWidth, kHeaderHeight};
  l.separators.push_back({0, kHeaderHeight, kScreenWidth, kLine});
  const int right = kColumnWidth + kLine;
  if (name == "three_columns") {
    const int narrow = (kScreenWidth - right - kLine) / 2;  // 219
    l.slots = {
        {0, kBodyTop, kColumnWidth, kBodyHeight},
        {right, kBodyTop, narrow, kBodyHeight},
        {right + narrow + kLine, kBodyTop, kScreenWidth - right - narrow - kLine, kBodyHeight}};
    l.sizes = {SlotSize::kColumn, SlotSize::kNarrowColumn, SlotSize::kNarrowColumn};
    l.separators.push_back({kColumnWidth, kBodyTop, kLine, kBodyHeight});
    l.separators.push_back({right + narrow, kBodyTop, kLine, kBodyHeight});
    return l;
  }
  const int tile_height = (kBodyHeight - kLine) / 2;  // 218
  l.slots = {{0, kBodyTop, kColumnWidth, kBodyHeight},
             {right, kBodyTop, kScreenWidth - right, tile_height},
             {right, kBodyTop + tile_height + kLine, kScreenWidth - right, tile_height}};
  l.sizes = {SlotSize::kColumn, SlotSize::kTile, SlotSize::kTile};
  l.separators.push_back({kColumnWidth, kBodyTop, kLine, kBodyHeight});
  l.separators.push_back({right, kBodyTop + tile_height, kScreenWidth - right, kLine});
  return l;
}

void drawSeparators(Canvas& canvas, const Layout& layout) {
  for (const Rect& r : layout.separators) canvas.fillRect(r.x, r.y, r.w, r.h, Color::kBlack);
}

}  // namespace ui
