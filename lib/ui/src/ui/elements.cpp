#include "ui/elements.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/text.h"

namespace ui {

namespace {

// Tile title (sizes from the overall UI design).
constexpr int kTitleBaseline = 24;
constexpr int kTitleRule = 32;
constexpr int kTitleSpacing = 16;  // 2 px, in 1/8 px
constexpr int kNoteSpacing = 12;   // 1.5 px
constexpr int kNoteGap = 16;       // minimum space between title and note

// Badge of an alert note.
constexpr int kBadgeTop = 6;
constexpr int kBadgeHeight = 24;
constexpr int kBadgeRadius = 4;
constexpr int kBadgePadding = 7;
constexpr int kBadgeGap = 6;
constexpr int kBadgeWarningHeight = 13;

// Tile message.
constexpr int kMessageWarningHeight = 28;

// Section title.
constexpr int kSectionSpacing = 13;  // 1.6 px
constexpr int kArrowWidth = 12;

const Font& titleFont() { return fonts::kRobotoCondensedBold17; }
const Font& noteFont() { return fonts::kRobotoCondensedMedium17; }
const Font& sectionFont() { return fonts::kRobotoCondensedBold16; }

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

void drawTileTitle(Canvas& canvas, std::string_view title, std::string_view note, bool alert) {
  const int x0 = kTilePadding;
  const int x1 = canvas.width() - kTilePadding;
  int note_width = 0;
  if (!note.empty() && alert) {
    // Black badge: warning sign and text in white.
    const int text_width = textWidth(titleFont(), note);
    const int w = kBadgePadding + warningSignWidth(kBadgeWarningHeight) + kBadgeGap + text_width +
                  kBadgePadding;
    canvas.fillRoundRect(x1 - w, kBadgeTop, w, kBadgeHeight, kBadgeRadius, Color::kBlack);
    drawWarningSign(canvas, x1 - w + kBadgePadding, kTitleBaseline, kBadgeWarningHeight,
                    Color::kWhite);
    drawText(canvas, titleFont(), x1 - kBadgePadding, kTitleBaseline, note, Color::kWhite,
             Align::kRight);
    note_width = w + kNoteGap;
  } else if (!note.empty()) {
    const std::string upper = toUpper(note);
    drawTrackedText(canvas, noteFont(), x1, kTitleBaseline, upper, kNoteSpacing, Color::kBlack,
                    Align::kRight);
    note_width = trackedTextWidth(noteFont(), upper, kNoteSpacing) + kNoteGap;
  }
  const std::string text =
      fitTracked(titleFont(), toUpper(title), kTitleSpacing, x1 - x0 - note_width);
  drawTrackedText(canvas, titleFont(), x0, kTitleBaseline, text, kTitleSpacing);
  canvas.hLine(x0, kTitleRule, x1 - x0, Color::kBlack);
}

void drawTileMessage(Canvas& canvas, std::string_view message, std::string_view detail) {
  const int centre_x = canvas.width() / 2;
  const int centre_y = kTileTitleHeight + (canvas.height() - kTileTitleHeight) / 2;
  const int max_width = canvas.width() - 2 * kTilePadding;
  drawWarningSign(canvas, centre_x - warningSignWidth(kMessageWarningHeight) / 2, centre_y - 14,
                  kMessageWarningHeight);
  const Font& big = fonts::kRobotoCondensedMedium24;
  drawText(canvas, big, centre_x, centre_y + 20, fitText(big, message, max_width), Color::kBlack,
           Align::kCenter);
  if (!detail.empty()) {
    const Font& small = fonts::kRobotoCondensedMedium17;
    drawText(canvas, small, centre_x, centre_y + 44, fitText(small, detail, max_width),
             Color::kBlack, Align::kCenter);
  }
}

void drawSectionTitle(Canvas& canvas, int x0, int x1, int y, std::string_view name) {
  const int centre = y + 11;
  // Arrow: 2 px shaft, triangular head.
  canvas.fillRect(x0, centre - 1, kArrowWidth - 4, 2, Color::kBlack);
  canvas.fillTriangle(x0 + kArrowWidth - 6, centre - 5, x0 + kArrowWidth, centre,
                      x0 + kArrowWidth - 6, centre + 4, Color::kBlack);
  const int text_end = drawTrackedText(canvas, sectionFont(), x0 + kArrowWidth + 6, y + 17,
                                       toUpper(name), kSectionSpacing);
  const int dots = text_end + 10;
  if (dots < x1) canvas.dottedHLine(dots, centre, x1 - dots + 1, 2);
}

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

void drawXbm(Canvas& canvas, int x, int y, int w, int h, const uint8_t* bits, int scale,
             Color color) {
  const int stride = (w + 7) / 8;
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      if (bits[row * stride + col / 8] & (1 << (col % 8))) {
        canvas.fillRect(x + col * scale, y + row * scale, scale, scale, color);
      }
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
      canvas.drawRect(x, y, width, height, Color::kBlack, 2);
      text = Color::kBlack;
      break;
    case SymbolShape::kRoundFrame:
      canvas.fillRoundRect(x, y, width, height, height / 2, Color::kWhite);
      canvas.drawRoundRect(x, y, width, height, height / 2, Color::kBlack, 2);
      text = Color::kBlack;
      break;
  }
  drawText(canvas, font, centre, baseline, label, text, Align::kCenter);
}

// --- Briefing ------------------------------------------------------------------

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

int drawLineSymbolChain(Canvas& canvas, const Font& font, const Font& small, int x, int centre_y,
                        int height, const std::vector<ChainLink>& links, bool draw) {
  for (size_t i = 0; i < links.size(); i++) {
    const ChainLink& link = links[i];
    const int w = link.width > 0 ? link.width : textWidth(font, link.label) + 16;
    if (draw) {
      drawLineSymbol(canvas, font, x, centre_y - height / 2, w, height, link.label, link.shape);
    }
    x += w;
    if (i + 1 == links.size()) break;
    if (draw) {
      canvas.hLine(x + 3, centre_y, kChainGap - 6, Color::kBlack);
      if (!link.transfer.empty()) {
        drawText(canvas, small, x + kChainGap / 2, centre_y + height / 2 + 12, link.transfer,
                 Color::kBlack, Align::kCenter);
      }
    }
    x += kChainGap;
  }
  return x;
}

}  // namespace ui
