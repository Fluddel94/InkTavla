#pragma once

// Evaluation of the buttons: debouncing and short or long presses, from
// the MVG departure display (buttons.cpp there). Pure logic, tested on the
// PC; the pins and the interrupt are in the HAL (hal/trmnl_buttons.h).
//
// Why an interrupt: the main loop is blocked for seconds during a panel
// refresh or an HTTPS request. A short press in that time would be missed
// by polling. The interrupt latches the press (with its time); it is
// evaluated in the next pass of the main loop.
//
// Debouncing: a press only counts if the button was released for at least
// hal::kButtonEdgeStableMs before (bouncing of the contacts causes no extra presses),
// and if it comes at least kMinIntervalMs after the last accepted press.
// A latched press older than kLatchMaxAgeMs is dropped.

#include <cstdint>

#include "hal/buttons.h"

namespace core {

using hal::ButtonSample;

enum class ButtonEvent {
  kNone,
  kShort,  // released before kLongPressMs
  kLong,   // held for kLongPressMs (reported while still held)
};

class ButtonLogic {
 public:
  static constexpr int64_t kEdgeStableMs = hal::kButtonEdgeStableMs;
  static constexpr int64_t kMinIntervalMs = 500;
  static constexpr int64_t kLatchMaxAgeMs = 15000;
  static constexpr int64_t kLongPressMs = 5000;

  // with_long_press = false: every press is reported as kShort at once,
  // without waiting for the release (buttons without a long function).
  explicit ButtonLogic(bool with_long_press = true) : with_long_(with_long_press) {}

  void setLongPress(bool enabled) { with_long_ = enabled; }

  // Evaluates the sample of one pass of the main loop.
  ButtonEvent update(int64_t now_ms, const ButtonSample& sample);

 private:
  bool with_long_;
  bool accepted_once_ = false;
  int64_t last_accepted_ms_ = 0;
  bool held_ = false;  // press accepted, waiting for release or long press
  int64_t held_since_ms_ = 0;
  bool long_reported_ = false;
};

}  // namespace core
