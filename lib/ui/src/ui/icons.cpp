#include "ui/icons.h"

#include <cstdlib>

namespace ui {

void drawIcon(Canvas& canvas, int x, int y, const Icon& icon, Color color) {
  const int stride = (icon.width + 7) / 8;
  for (int row = 0; row < icon.height; row++) {
    for (int col = 0; col < icon.width; col++) {
      if (icon.bits[row * stride + col / 8] & (0x80 >> (col % 8))) {
        canvas.setPixel(x + col, y + row, color);
      }
    }
  }
}

const Icon& weatherIcon(WeatherIcon kind, int size) {
  const WeatherIconEntry* best = &kWeatherIcons[0];
  for (size_t i = 0; i < kWeatherIconCount; i++) {
    const WeatherIconEntry& e = kWeatherIcons[i];
    if (e.kind != kind) continue;
    if (best->kind != kind || std::abs(e.icon.width - size) < std::abs(best->icon.width - size)) {
      best = &e;
    }
  }
  return best->icon;
}

}  // namespace ui
