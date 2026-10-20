#include "ui/bitmap.h"

#include <algorithm>
#include <cstring>

namespace ui {

Bitmap::Bitmap(uint16_t width, uint16_t height, Color background)
    : width_(width),
      height_(height),
      stride_((width + 7u) / 8u),
      data_(stride_ * height, background == Color::kBlack ? 0xFF : 0x00) {}

Color Bitmap::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return Color::kWhite;
  const uint8_t byte = data_[y * stride_ + x / 8];
  return (byte & (0x80 >> (x % 8))) ? Color::kBlack : Color::kWhite;
}

void Bitmap::set(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  uint8_t& byte = data_[y * stride_ + x / 8];
  const uint8_t mask = 0x80 >> (x % 8);
  if (color == Color::kBlack) {
    byte |= mask;
  } else {
    byte &= static_cast<uint8_t>(~mask);
  }
}

void Bitmap::clear(Color color) {
  std::memset(data_.data(), color == Color::kBlack ? 0xFF : 0x00, data_.size());
}

BitmapCanvas::BitmapCanvas(Bitmap& bitmap)
    : BitmapCanvas(bitmap, 0, 0, bitmap.width(), bitmap.height()) {}

BitmapCanvas::BitmapCanvas(Bitmap& bitmap, int x, int y, int width, int height) : bitmap_(bitmap) {
  const int x0 = std::clamp(x, 0, static_cast<int>(bitmap.width()));
  const int y0 = std::clamp(y, 0, static_cast<int>(bitmap.height()));
  const int x1 = std::clamp(x + std::max(width, 0), 0, static_cast<int>(bitmap.width()));
  const int y1 = std::clamp(y + std::max(height, 0), 0, static_cast<int>(bitmap.height()));
  x_ = x0;
  y_ = y0;
  width_ = static_cast<uint16_t>(x1 - x0);
  height_ = static_cast<uint16_t>(y1 - y0);
}

void BitmapCanvas::setPixel(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  bitmap_.set(x_ + x, y_ + y, color);
}

void BitmapCanvas::fillRect(int x, int y, int w, int h, Color color) {
  const int x0 = std::max(x, 0);
  const int y0 = std::max(y, 0);
  const int x1 = std::min(x + w, static_cast<int>(width_));
  const int y1 = std::min(y + h, static_cast<int>(height_));
  for (int py = y0; py < y1; py++) {
    for (int px = x0; px < x1; px++) bitmap_.set(x_ + px, y_ + py, color);
  }
}

}  // namespace ui
