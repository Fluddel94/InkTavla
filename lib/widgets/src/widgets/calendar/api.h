#pragma once

// The private iCal address of a calendar, requested over HTTPS. The address
// is a secret (whoever knows it can read the calendar): it is read from NVS
// (core/secrets.h) and never printed.

#include "widgets/calendar/widget.h"

namespace widgets::calendar {

// Largest iCal file read (calendars with many years of history are a few
// megabytes).
inline constexpr size_t kMaxCalendarBytes = 8 * 1024 * 1024;
// Longest a normal fetch may take: the main loop (clock, departures) waits
// meanwhile. About 7 MB at the speed measured on the device (~80 KB/s).
inline constexpr uint32_t kFetchLimitMs = 90000;

#if defined(ARDUINO)
class ApiSource : public Source {
 public:
  // Reads whether an address is set (once; saving one restarts the device).
  void begin();
  bool configured() const override { return configured_; }
  bool fetchCalendar(Parser& parser) override;
  FetchError lastError() const override { return error_; }

 private:
  bool configured_ = false;
  FetchError error_ = FetchError::kNone;
};
#endif

}  // namespace widgets::calendar
