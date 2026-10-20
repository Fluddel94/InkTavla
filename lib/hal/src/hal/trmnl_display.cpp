#if defined(ARDUINO)

#include "hal/trmnl_display.h"

#include <Arduino.h>

namespace hal {

void TrmnlDisplay::show(const uint8_t* frame, size_t stride, Refresh mode, const Rect* regions,
                        size_t region_count) {
  (void)frame;
  (void)stride;
  if (mode == Refresh::kFull) {
    Serial.println("Display: full refresh (panel driver follows in phase B)");
    return;
  }
  Serial.printf("Display: partial refresh of %u region(s)", static_cast<unsigned>(region_count));
  for (size_t i = 0; i < region_count; i++) {
    Serial.printf(" [%d,%d %dx%d]", regions[i].x, regions[i].y, regions[i].w, regions[i].h);
  }
  Serial.println();
}

}  // namespace hal

#endif
