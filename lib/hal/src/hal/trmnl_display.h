#pragma once

// Panel of the reference hardware: Seeed TRMNL 7.5" (OG) DIY Kit, 800 x 480
// UC8179 on the EE04 driver board.
//
// Placeholder until the kit is available (phase B): it reports every update
// on the serial monitor but does not drive the panel yet.

#if defined(ARDUINO)

#include "hal/display.h"

namespace hal {

class TrmnlDisplay : public Display {
 public:
  uint16_t width() const override { return 800; }
  uint16_t height() const override { return 480; }
  void show(const uint8_t* frame, size_t stride, Refresh mode, const Rect* regions,
            size_t region_count) override;
};

}  // namespace hal

#endif
