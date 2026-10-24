#include "core/buttons.h"

namespace core {

ButtonEvent ButtonLogic::update(int64_t now_ms, const ButtonSample& sample) {
  if (!held_ && sample.press_latched) {
    const int64_t at = sample.press_at_ms;
    const bool too_old = now_ms - at > kLatchMaxAgeMs;
    const bool too_soon = accepted_once_ && at - last_accepted_ms_ < kMinIntervalMs;
    if (!too_old && !too_soon) {
      accepted_once_ = true;
      last_accepted_ms_ = at;
      if (!with_long_) return ButtonEvent::kShort;
      held_ = true;
      held_since_ms_ = at;  // hold time counts from the real press
      long_reported_ = false;
    }
  }
  if (!held_) return ButtonEvent::kNone;

  if (sample.pressed_now && !long_reported_ && now_ms - held_since_ms_ >= kLongPressMs) {
    long_reported_ = true;
    return ButtonEvent::kLong;
  }
  // Released only when the pin is stable (not in the middle of bouncing).
  const bool released = !sample.pressed_now && now_ms - sample.last_edge_ms >= kEdgeStableMs;
  if (released) {
    held_ = false;
    if (!long_reported_) return ButtonEvent::kShort;
  }
  return ButtonEvent::kNone;
}

}  // namespace core
