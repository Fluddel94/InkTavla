#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ui/canvas.h"

namespace ui {

// 1-bit image buffer: rows top to bottom, each row padded to whole bytes,
// most significant bit = leftmost pixel, bit set = black. The HAL converts
// this into whatever the panel expects. 800 x 480 pixels take 48 000 bytes;
// on the ESP32-S3 such a large allocation goes to PSRAM.
class Bitmap {
 public:
  Bitmap(uint16_t width, uint16_t height, Color background = Color::kWhite);

  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  size_t stride() const { return stride_; }  // bytes per row
  const uint8_t* data() const { return data_.data(); }
  size_t size() const { return data_.size(); }

  // Pixel colour; outside the bitmap: white.
  Color get(int x, int y) const;
  void set(int x, int y, Color color);
  void clear(Color color);

 private:
  uint16_t width_;
  uint16_t height_;
  size_t stride_;
  std::vector<uint8_t> data_;
};

// Canvas on a rectangular area of a bitmap: (0, 0) is the top-left corner
// of the area and all drawing is clipped to it. The area itself is clipped
// to the bitmap.
class BitmapCanvas : public Canvas {
 public:
  explicit BitmapCanvas(Bitmap& bitmap);
  BitmapCanvas(Bitmap& bitmap, int x, int y, int width, int height);

  uint16_t width() const override { return width_; }
  uint16_t height() const override { return height_; }
  void setPixel(int x, int y, Color color) override;
  void fillRect(int x, int y, int w, int h, Color color) override;

 private:
  Bitmap& bitmap_;
  int x_;
  int y_;
  uint16_t width_;
  uint16_t height_;
};

}  // namespace ui
