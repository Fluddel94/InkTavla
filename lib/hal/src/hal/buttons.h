#pragma once

// What the interrupt of a button records (see hal/trmnl_buttons.h); the
// evaluation is core::ButtonLogic.

#include <cstdint>

namespace hal {

// An edge to "pressed" starts a press only if the previous edge (press or
// release) was at least this long ago, so bouncing contacts cause no extra
// presses.
inline constexpr int64_t kButtonEdgeStableMs = 30;

struct ButtonSample {
  bool press_latched = false;  // a press since the last sample
  int64_t press_at_ms = 0;     // time of that press
  bool pressed_now = false;    // current level of the pin (pressed = true)
  int64_t last_edge_ms = 0;    // time of the last edge (press or release)
};

// The edge rule above.
inline bool acceptPressEdge(bool pressed, int64_t now_ms, int64_t last_edge_ms) {
  return pressed && now_ms - last_edge_ms >= kButtonEdgeStableMs;
}

}  // namespace hal
