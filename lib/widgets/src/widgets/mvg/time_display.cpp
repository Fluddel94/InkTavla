#include "widgets/mvg/time_display.h"

#include <algorithm>

#include "core/time.h"

namespace widgets::mvg {

int effectiveDelay(int delay_minutes) {
  if (delay_minutes < 0 && delay_minutes > -kEarlyDepartureMinutes) return 0;
  return delay_minutes;
}

TimeDisplay timeDisplay(const Departure& departure, TimeFormat format, int64_t now_s) {
  TimeDisplay t;
  const int delay = effectiveDelay(departure.delay_minutes);

  // Minutes until the expected departure (planned + delay), counted as the
  // difference of the minute values like the clock in the band
  // (14:07 -> 14:09 = "2 min"). From kMinutesModeLimit on the clock time.
  bool as_minutes = false;
  if (format == TimeFormat::kMinutes && core::isValidUnixTime(now_s)) {
    const int64_t expected = departure.planned + static_cast<int64_t>(delay) * 60;
    const int64_t diff = core::minutesUntil(now_s, expected);
    if (diff < kMinutesModeLimit) {
      as_minutes = true;
      t.kind = diff <= 0 ? TimeDisplay::Kind::kNow : TimeDisplay::Kind::kMinutes;
      t.minutes = diff <= 0 ? 0 : static_cast<int>(diff);
    }
  }
  if (!as_minutes) {
    core::LocalTime local;
    if (core::toLocalTime(departure.planned, local)) {
      t.hour = local.hour;
      t.minute = local.minute;
    }
  }

  // A cancellation shows the same warning sign as any other warning, before
  // the time (once, even with a message on top).
  t.warning_before_time = departure.warning || departure.cancelled;
  if (departure.cancelled) {
    t.struck = true;
  } else if (!as_minutes && delay != 0) {
    // A deviation of 0 (including -1/-2 set to 0) is never shown.
    t.show_delay = true;
    t.delay = std::clamp(delay, -kMaxShownDelay, kMaxShownDelay);
  }
  return t;
}

}  // namespace widgets::mvg
