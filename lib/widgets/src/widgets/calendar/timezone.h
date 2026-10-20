#pragma once

// Time zones of an iCalendar file (VTIMEZONE, RFC 5545 section 3.6.5):
// observances with their UTC offsets and the rules for the changes between
// standard and daylight saving time. Times without a zone ("floating") and
// TZIDs without a VTIMEZONE use the time zone of the device.

#include <cstdint>
#include <string>
#include <vector>

#include "widgets/calendar/datetime.h"
#include "widgets/calendar/rrule.h"

namespace widgets::calendar {

// STANDARD or DAYLIGHT part of a VTIMEZONE: from its onsets on, the zone
// has offset_to.
struct Observance {
  DateTime start;  // first onset, local time before it (offset_from)
  int offset_from = 0;
  int offset_to = 0;
  Rule rule;  // frequency kNone: no RRULE
  std::vector<DateTime> rdates;
};

struct Zone {
  std::string name;  // TZID
  std::vector<Observance> observances;
};

// UTC offset (seconds) the zone has at the instant utc.
int zoneOffsetAt(const Zone& zone, int64_t utc);

// Instant of a local time in the zone. A time that exists twice (end of
// daylight saving time) is the first of them; a time that does not exist
// (start of daylight saving time) uses the offset before the change, as RFC
// 5545 asks.
int64_t zoneToUtc(const Zone& zone, int64_t local_s);

// The same with the time zone of the device (the process-wide TZ setting).
int64_t deviceToUtc(int64_t local_s);
// Local seconds of an instant in the time zone of the device.
int64_t deviceLocalSeconds(int64_t utc);

}  // namespace widgets::calendar
