#include "ui/elements.h"

#include <algorithm>

#include "ui/text.h"

namespace ui {

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
                    std::string_view label, bool frame) {
  const int baseline = y + (height + font.cap_height) / 2;
  const int centre = x + width / 2;
  if (frame) {
    canvas.fillRoundRect(x, y, width, height, height / 3, Color::kWhite);
    canvas.drawRoundRect(x, y, width, height, height / 3, Color::kBlack, 2);
    drawText(canvas, font, centre, baseline, label, Color::kBlack, Align::kCenter);
  } else {
    canvas.fillRect(x, y, width, height, Color::kBlack);
    drawText(canvas, font, centre, baseline, label, Color::kWhite, Align::kCenter);
  }
}

}  // namespace ui
