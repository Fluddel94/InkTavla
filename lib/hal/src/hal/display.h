#pragma once

#include <cstdint>

namespace hal {

// E-paper panel as seen by the rest of the firmware.
//
// The HAL is the only layer that knows the concrete hardware. The
// implementation for the TRMNL OG DIY Kit (UC8179 panel) follows with the
// first hardware test; refresh operations are added then. Only the display
// controller in core talks to this interface.
class Display {
 public:
  virtual ~Display() = default;

  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;
};

}  // namespace hal
