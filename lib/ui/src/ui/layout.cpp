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
  // Columns from left to right; the slots split the gaps between them.
  int widths[3] = {kWideColumnWidth, kColumnWidth, kColumnWidth};
  int gap = kColumnGap;
  if (options.equal_columns) {
    const int content_w = kScreenWidth - 2 * kMargin;
    const int w = (content_w - 2 * kEqualColumnGap) / 3;
    widths[0] = widths[1] = widths[2] = w;
    gap = (content_w - 3 * w) / 2;  // the rest of the division goes into the gaps
  }
  int x = kMargin;
  int slot_x = 0;
  for (int i = 0; i < 3; i++) {
    l.content.push_back({x, top, widths[i], content_h});
    const int slot_end = i < 2 ? x + widths[i] + gap / 2 : kScreenWidth;
    l.slots.push_back({slot_x, top, slot_end - slot_x, h});
    slot_x = slot_end;
    x += widths[i] + gap;
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
