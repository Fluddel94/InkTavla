#include "ui/elements.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/text.h"

namespace ui {

namespace {

// Shortens a spaced text like fitText() does for normal text.
std::string fitTracked(const Font& font, const std::string& text, int spacing_x8, int max_width) {
  if (trackedTextWidth(font, text, spacing_x8) <= max_width) return text;
  std::string result = text;
  while (!result.empty()) {
    // Remove the last character (UTF-8 continuation bytes included).
    size_t end = result.size() - 1;
    while (end > 0 && (static_cast<uint8_t>(result[end]) & 0xC0) == 0x80) end--;
    result.erase(end);
    while (!result.empty() && result.back() == ' ') result.pop_back();
    if (trackedTextWidth(font, result + ".", spacing_x8) <= max_width) break;
  }
  return result + ".";
}

}  // namespace

int warningSignWidth(int height) { return (height * 23 + 10) / 20; }  // 1.15 x height

int drawWarningSign(Canvas& canvas, int x, int baseline, int height, Color color) {
  const int w = warningSignWidth(height);
  const int bottom = baseline - 1;
  const int top = baseline - height;
  const int centre = x + (w - 1) / 2;
  canvas.fillTriangle(x, bottom, x + w - 1, bottom, centre, top, color);

  // Exclamation mark in the background colour.
  const Color mark = color == Color::kBlack ? Color::kWhite : Color::kBlack;
  const int t = std::max(1, (height + 3) / 7);
  const int bar_top = top + (height * 38 + 50) / 100;
  const int bar_bottom = bottom - (height * 36 + 50) / 100;
  canvas.fillRect(centre - t / 2, bar_top, t, bar_bottom - bar_top + 1, mark);
  const int dot = bottom - (height * 12 + 50) / 100 - t + 1;
  canvas.fillRect(centre - t / 2, dot, t, t, mark);
  return w;
}

void drawWifiSymbol(Canvas& canvas, int x, int y, int size, bool connected) {
  // Geometry of the design drawing: arcs around a point near the bottom,
  // opening upwards (90 degrees), stroke about size / 9.
  const double n = size;
  const double cx = n / 2;
  const double cy = n * 0.86;
  const double stroke = std::max(1.6, n / 9);
  const double radii[] = {n * 0.78, n * 0.52};
  const double dot_y = cy - n * 0.04;
  const double dot_r = n * 0.1;
  for (int py = 0; py < size; py++) {
    for (int px = 0; px < size; px++) {
      const double dx = px + 0.5 - cx;
      const double dy = py + 0.5 - cy;
      const double d = std::sqrt(dx * dx + dy * dy);
      bool on = false;
      if (dy < 0 && std::fabs(dx) <= -dy) {
        for (double r : radii) on = on || (d <= r && d > r - stroke);
      }
      const double ddy = py + 0.5 - dot_y;
      on = on || dx * dx + ddy * ddy <= dot_r * dot_r;
      if (on) canvas.setPixel(x + px, y + py, Color::kBlack);
    }
  }
  if (connected) return;
  // Diagonal stroke from bottom left to top right, 2 px wide with a white
  // margin so it stands out from the arcs.
  for (int i = -1; i <= size; i++) {
    for (int t = -2; t <= 2; t++) {
      const Color c = (t == -1 || t == 0) ? Color::kBlack : Color::kWhite;
      const int px = i + t;
      const int py = size - 1 - i;
      if (px >= 0 && px < size && py >= 0 && py < size) canvas.setPixel(x + px, y + py, c);
    }
  }
}

void drawLineSymbol(Canvas& canvas, const Font& font, int x, int y, int width, int height,
                    std::string_view label, SymbolShape shape) {
  const int baseline = y + (height + font.cap_height) / 2;
  const int centre = x + width / 2;
  Color text = Color::kWhite;
  switch (shape) {
    case SymbolShape::kBox:
      canvas.fillRoundRect(x, y, width, height, 3, Color::kBlack);
      break;
    case SymbolShape::kRect:
      canvas.fillRect(x, y, width, height, Color::kBlack);
      break;
    case SymbolShape::kPill:
      canvas.fillRoundRect(x, y, width, height, height / 2, Color::kBlack);
      break;
    case SymbolShape::kFrame:
      canvas.fillRect(x, y, width, height, Color::kWhite);
      canvas.drawRect(x, y, width, height, Color::kBlack, kSymbolFrame);
      text = Color::kBlack;
      break;
  }
  drawText(canvas, font, centre, baseline, label, text, Align::kCenter);
}

// --- Titles and messages ---------------------------------------------------------

namespace {

constexpr int kColumnTitleSpacing = 16;  // 2 px
constexpr int kColumnNoteGap = 10;       // minimum space between title and note
constexpr int kSmallBadgePadding = 5;
constexpr int kSmallBadgeGap = 4;
constexpr int kSmallBadgeWarningHeight = 11;

const Font& columnTitleFont() { return fonts::kRobotoCondensedBold15; }
const Font& columnNoteFont() { return fonts::kRobotoCondensedMedium15; }

}  // namespace

int drawColumnTitle(Canvas& canvas, int x0, int x1, int baseline, std::string_view title,
                    std::string_view note, TitleIcon icon, bool alert) {
  int x = x0;
  if (icon == TitleIcon::kArrow) {
    const int centre = baseline - 5;
    canvas.fillRect(x, centre - 1, 8, 2, Color::kBlack);
    canvas.fillTriangle(x + 6, centre - 5, x + 12, centre, x + 6, centre + 4, Color::kBlack);
    x += 18;
  } else if (icon == TitleIcon::kPin) {
    drawIcon(canvas, x, baseline - 15, kPinIcon);
    x += 22;
  }
  std::string text = toUpper(title);
  const Font& font = columnTitleFont();

  if (alert && !note.empty()) {
    // Small black badge with a warning sign and the note in white.
    const int text_width = textWidth(font, note);
    const int w = kSmallBadgePadding + warningSignWidth(kSmallBadgeWarningHeight) + kSmallBadgeGap +
                  text_width + kSmallBadgePadding;
    const int left = x1 - w;
    canvas.fillRoundRect(left, baseline - 15, w, 19, 3, Color::kBlack);
    drawWarningSign(canvas, left + kSmallBadgePadding, baseline - 1, kSmallBadgeWarningHeight,
                    Color::kWhite);
    drawText(canvas, font, x1 - kSmallBadgePadding, baseline, note, Color::kWhite, Align::kRight);
    text = fitTracked(font, text, kColumnTitleSpacing, left - kColumnNoteGap - x);
    return drawTrackedText(canvas, font, x, baseline, text, kColumnTitleSpacing);
  }

  text = fitTracked(font, text, kColumnTitleSpacing, x1 - x);
  const int end = drawTrackedText(canvas, font, x, baseline, text, kColumnTitleSpacing);
  if (!note.empty() && end + kColumnNoteGap <= x1 - textWidth(columnNoteFont(), note)) {
    drawText(canvas, columnNoteFont(), x1, baseline, note, Color::kBlack, Align::kRight);
  }
  return end;
}

void drawColumnMessage(Canvas& canvas, std::string_view message, std::string_view detail,
                       bool warning) {
  const int centre_x = canvas.width() / 2;
  const Font& big = fonts::kRobotoCondensedMedium20;
  const Font& small = fonts::kRobotoCondensedMedium15;
  const std::vector<std::string> lines = wrapText(big, message, canvas.width(), 2);
  const std::vector<std::string> details =
      detail.empty() ? std::vector<std::string>{} : wrapText(small, detail, canvas.width(), 3);
  const int sign = warning ? 22 + 8 : 0;
  const int height = sign + 24 * static_cast<int>(lines.size()) +
                     (details.empty() ? 0 : 4 + 18 * static_cast<int>(details.size()));
  const int top = kColumnTitleBaseline + 8;
  int y = top + std::max(0, (canvas.height() - top - height) / 2);
  if (warning) drawWarningSign(canvas, centre_x - warningSignWidth(22) / 2, y + 22, 22);
  y += sign;
  for (const std::string& l : lines) {
    y += 24;
    drawText(canvas, big, centre_x, y - 6, l, Color::kBlack, Align::kCenter);
  }
  y += 4;
  for (const std::string& l : details) {
    y += 18;
    drawText(canvas, small, centre_x, y - 4, l, Color::kBlack, Align::kCenter);
  }
}

}  // namespace ui
