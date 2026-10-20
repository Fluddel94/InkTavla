#pragma once

#include <cstddef>
#include <cstdint>

namespace hal {

// Rectangle on the panel in pixels.
struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

enum class Refresh {
  kFull,     // whole panel, slow, clears ghosting
  kPartial,  // only the given regions, fast
};

// E-paper panel as seen by the rest of the firmware.
//
// The HAL is the only layer that knows the concrete hardware. Only the
// display controller in core talks to this interface. A frame is a 1-bit
// image of the panel's size: rows top to bottom, `stride` bytes per row,
// most significant bit = leftmost pixel, set bit = black.
class Display {
 public:
  virtual ~Display() = default;

  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;

  // Shows a frame. kFull refreshes the whole panel (regions are ignored);
  // kPartial only transfers and refreshes the given regions.
  virtual void show(const uint8_t* frame, size_t stride, Refresh mode, const Rect* regions,
                    size_t region_count) = 0;
};

}  // namespace hal
