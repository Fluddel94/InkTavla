#include "ui/layout.h"

namespace ui {

Rect contentOf(const Layout& layout, size_t i) {
  if (i < layout.content.size()) return layout.content[i];
  return i < layout.slots.size() ? layout.slots[i] : Rect{};
}

int stepWidth(SizeStep step) {
  switch (step) {
    case SizeStep::kS:
      return kStepWidthS;
    case SizeStep::kM:
      return kStepWidthM;
    case SizeStep::kL:
      return kStepWidthL;
  }
  return 0;
}

const char* sizeStepName(SizeStep step) {
  switch (step) {
    case SizeStep::kS:
      return "S";
    case SizeStep::kM:
      return "M";
    case SizeStep::kL:
      return "L";
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
  if (options.columns == 2) {
    l.steps = {SizeStep::kL, SizeStep::kL};
  } else {
    l.steps = {SizeStep::kS, SizeStep::kS, SizeStep::kS};
    const int wide = options.wide_column >= 0 && options.wide_column < 3 ? options.wide_column : 0;
    l.steps[wide] = SizeStep::kM;
  }
  int x = kMargin;
  int slot_x = 0;
  for (size_t i = 0; i < l.steps.size(); i++) {
    const int w = stepWidth(l.steps[i]);
    l.content.push_back({x, top, w, content_h});
    const int slot_end = i + 1 < l.steps.size() ? x + w + kColumnGap / 2 : kScreenWidth;
    l.slots.push_back({slot_x, top, slot_end - slot_x, h});
    slot_x = slot_end;
    x += w + kColumnGap;
  }
  if (options.dividers) {
    // In the middle of the gaps, i.e. at the left edge of every slot but the first.
    for (size_t i = 1; i < l.slots.size(); i++) {
      l.dividers.push_back({l.slots[i].x, kDividerTop, 1, kBriefingContentBottom - kDividerTop});
    }
  }
  return l;
}

}  // namespace ui
