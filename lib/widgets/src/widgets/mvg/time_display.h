#pragma once

// Rules for the time column of the MVG departure list, ported from the MVG
// departure display. They describe *what* is shown; fonts, positions and
// translations ("now", "min") belong to the view.

#include <cstdint>

#include "widgets/mvg/departure.h"

namespace widgets::mvg {

// Departures less than this many minutes early count as on time: "-1" is
// confusing and mostly rounding or a forecast.
inline constexpr int kEarlyDepartureMinutes = 3;

// Larger delays are shown as +99 / -99 (the column has room for two digits).
inline constexpr int kMaxShownDelay = 99;

// In minutes mode, departures this many minutes away or more show the clock
// time again.
inline constexpr int kMinutesModeLimit = 60;

enum class TimeFormat {
  kClock,    // planned time "14:32" plus delay "+3"
  kMinutes,  // "4 min" / "now" until kMinutesModeLimit, then clock time
};

// Delay after the early-departure rule: -1 and -2 become 0.
int effectiveDelay(int delay_minutes);

struct TimeDisplay {
  enum class Kind {
    kClock,    // hour:minute of the planned departure, local time
    kMinutes,  // minutes until the expected departure (1..59)
    kNow,      // expected departure in this minute or already passed
  };
  Kind kind = Kind::kClock;
  int hour = 0;
  int minute = 0;
  int minutes = 0;

  // Cancelled: strike the time through and show the warning sign where the
  // delay would be.
  bool struck = false;
  bool warning_in_time_column = false;

  // Warning sign right before the time (warning, but not cancelled); the
  // signs of all rows line up in one column (2026-10-26).
  bool warning_before_time = false;

  // Delay text "+3" / "-4" (clock time only, never "+0"), limited to
  // +/-kMaxShownDelay.
  bool show_delay = false;
  int delay = 0;
};

// What the time column shows for a departure. now_s is the current Unix
// time; without a valid time the minutes mode falls back to the clock time.
TimeDisplay timeDisplay(const Departure& departure, TimeFormat format, int64_t now_s);

}  // namespace widgets::mvg
