#include "ui/layout.h"

namespace ui {

namespace {

constexpr int kHeaderHeight = 40;
constexpr int kLine = 2;  // separator thickness
constexpr int kBodyTop = kHeaderHeight + kLine;
constexpr int kBodyHeight = kScreenHeight - kBodyTop;
constexpr int kColumnWidth = 358;  // departures column, 45 % with separator

}  // namespace

Layout layoutFor(std::string_view name) {
  Layout l;
  l.header = {0, 0, kScreenWidth, kHeaderHeight};
  l.separators.push_back({0, kHeaderHeight, kScreenWidth, kLine});
  const int right = kColumnWidth + kLine;
  if (name == "three_columns") {
    const int narrow = (kScreenWidth - right - kLine) / 2;  // 219
    l.slots = {
        {0, kBodyTop, kColumnWidth, kBodyHeight},
        {right, kBodyTop, narrow, kBodyHeight},
        {right + narrow + kLine, kBodyTop, kScreenWidth - right - narrow - kLine, kBodyHeight}};
    l.separators.push_back({kColumnWidth, kBodyTop, kLine, kBodyHeight});
    l.separators.push_back({right + narrow, kBodyTop, kLine, kBodyHeight});
    return l;
  }
  const int tile_height = (kBodyHeight - kLine) / 2;  // 218
  l.slots = {{0, kBodyTop, kColumnWidth, kBodyHeight},
             {right, kBodyTop, kScreenWidth - right, tile_height},
             {right, kBodyTop + tile_height + kLine, kScreenWidth - right, tile_height}};
  l.separators.push_back({kColumnWidth, kBodyTop, kLine, kBodyHeight});
  l.separators.push_back({right, kBodyTop + tile_height, kScreenWidth - right, kLine});
  return l;
}

void drawSeparators(Canvas& canvas, const Layout& layout) {
  for (const Rect& r : layout.separators) canvas.fillRect(r.x, r.y, r.w, r.h, Color::kBlack);
}

}  // namespace ui
