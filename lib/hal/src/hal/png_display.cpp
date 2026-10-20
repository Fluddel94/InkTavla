#if !defined(ARDUINO)

#include "hal/png_display.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utility>

#include "hal/png.h"

namespace hal {

PngDisplay::PngDisplay(uint16_t width, uint16_t height, std::string directory, std::string prefix)
    : width_(width),
      height_(height),
      stride_((width + 7u) / 8u),
      directory_(std::move(directory)),
      prefix_(std::move(prefix)),
      panel_(stride_ * height, 0x00) {}

void PngDisplay::show(const uint8_t* frame, size_t stride, Refresh mode, const Rect* regions,
                      size_t region_count) {
  Update update{mode, {}};
  if (mode == Refresh::kFull) {
    for (int y = 0; y < height_; y++) {
      std::copy(frame + y * stride, frame + y * stride + stride_, panel_.begin() + y * stride_);
    }
  } else {
    for (size_t i = 0; i < region_count; i++) {
      const Rect& r = regions[i];
      update.regions.push_back(r);
      for (int y = std::max(r.y, 0); y < std::min(r.y + r.h, static_cast<int>(height_)); y++) {
        for (int x = std::max(r.x, 0); x < std::min(r.x + r.w, static_cast<int>(width_)); x++) {
          const uint8_t mask = 0x80 >> (x % 8);
          uint8_t& byte = panel_[y * stride_ + x / 8];
          byte = (frame[y * stride + x / 8] & mask) ? (byte | mask) : (byte & ~mask);
        }
      }
    }
  }
  updates_.push_back(update);
  if (directory_.empty()) return;
  std::filesystem::create_directories(directory_);
  char name[32];
  std::snprintf(name, sizeof(name), "%04u.png", static_cast<unsigned>(updates_.size()));
  std::ofstream(directory_ + "/" + prefix_ + name, std::ios::binary) << png();
}

std::string PngDisplay::png() const { return png::encode(panel_.data(), width_, height_, stride_); }

}  // namespace hal

#endif
