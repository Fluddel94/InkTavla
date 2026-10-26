#pragma once

// What the interrupt of a button records (see hal/trmnl_buttons.h); the
// evaluation is core::ButtonLogic.

#include <cstdint>

namespace hal {

// An edge to "pressed" starts a press only if the button was released at
// the previous edge and that edge was at least this long ago, so bouncing
// contacts cause no extra presses.
inline constexpr int64_t kButtonEdgeStableMs = 30;

struct ButtonSample {
  bool press_latched = false;  // a press since the last sample
  int64_t press_at_ms = 0;     // time of that press
  bool pressed_now = false;    // current level of the pin (pressed = true)
  int64_t last_edge_ms = 0;    // time of the last edge (press or release)
};

// The edge rule above. last_pressed is the level read at the previous edge.
// Without it, the first bouncing edge of a release after a long hold could
// read "pressed" and count as a new press (the previous edge was seconds
// ago): a long press of KEY3 opened the settings page and closed it again.
inline bool acceptPressEdge(bool pressed, int64_t now_ms, int64_t last_edge_ms, bool last_pressed) {
  return pressed && !last_pressed && now_ms - last_edge_ms >= kButtonEdgeStableMs;
}

}  // namespace hal
