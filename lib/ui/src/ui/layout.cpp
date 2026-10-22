#include "ui/layout.h"

namespace ui {

Rect contentOf(const Layout& layout, size_t i) {
  if (i < layout.content.size()) return layout.content[i];
  return i < layout.slots.size() ? layout.slots[i] : Rect{};
}

const char* slotSizeName(SlotSize size) {
  switch (size) {
    case SlotSize::kBriefingWide:
      return "wide column";
    case SlotSize::kBriefingColumn:
      return "column";
  }
  return "";
}

Layout screenLayout(const LayoutOptions& options) {
  Layout l;
  // Band; the rule at y 204 is drawn by the band's view.
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
  if (options.equal_columns) {
    // Columns: 28-260, 284-516, 540-772; the slots split the gaps.
    l.slots = {{0, top, 272, h}, {272, top, 256, h}, {528, top, kScreenWidth - 528, h}};
    l.content = {{28, top, 232, content_h}, {284, top, 232, content_h}, {540, top, 232, content_h}};
  } else {
    // Columns: 28-292, 318-532, 558-772; the slots split the gaps.
    l.slots = {{0, top, 305, h}, {305, top, 240, h}, {545, top, kScreenWidth - 545, h}};
    l.content = {{28, top, 264, content_h}, {318, top, 214, content_h}, {558, top, 214, content_h}};
  }
  l.sizes = {SlotSize::kBriefingWide, SlotSize::kBriefingColumn, SlotSize::kBriefingColumn};
  if (options.dividers) {
    // In the middle of the gaps, i.e. at the left edge of slots 2 and 3.
    for (size_t i = 1; i < l.slots.size(); i++) {
      l.dividers.push_back({l.slots[i].x, kDividerTop, 1, kBriefingContentBottom - kDividerTop});
    }
  }
  return l;
}

}  // namespace ui
