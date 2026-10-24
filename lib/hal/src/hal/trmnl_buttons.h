#pragma once

// The three buttons of the EE04 driver board of the TRMNL 7.5" (OG) DIY
// Kit: KEY1 GPIO2, KEY2 GPIO3, KEY3 GPIO5, with pull-ups (pressed = LOW).
//
// Every edge raises an interrupt that latches a press with its time, so a
// press is not lost while the main loop is blocked (panel refresh, HTTPS).
// The evaluation (short/long, debouncing) is core::ButtonLogic.

#if defined(ARDUINO)

#include <cstddef>

#include "hal/buttons.h"

namespace hal {

class TrmnlButtons {
 public:
  static constexpr size_t kCount = 3;

  // Pins and interrupts; once in setup().
  void begin();

  // What happened at button i since the last call (takes the latched
  // press).
  ButtonSample sample(size_t i);
};

}  // namespace hal

#endif
