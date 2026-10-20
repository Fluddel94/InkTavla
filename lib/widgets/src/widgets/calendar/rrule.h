#pragma once

// Recurrence rules (RRULE, RFC 5545 section 3.3.10) of events and of the
// daylight saving rules in VTIMEZONE.
//
// Supported: FREQ=DAILY/WEEKLY/MONTHLY/YEARLY with INTERVAL, COUNT, UNTIL,
// WKST, BYDAY (with ordinals such as 2TU or -1SU), BYMONTHDAY, BYMONTH and
// BYSETPOS. Rules with other parts (BYHOUR, BYWEEKNO, BYYEARDAY, ...) or
// sub-daily frequencies are marked unsupported; such series only show their
// first occurrence.

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

#include "widgets/calendar/datetime.h"

namespace widgets::calendar {

enum class Frequency : uint8_t { kNone, kDaily, kWeekly, kMonthly, kYearly };

struct WeekdayRule {
  int8_t ordinal = 0;   // 0: every such weekday, 2: the second, -1: the last
  uint8_t weekday = 0;  // 0 = Sunday .. 6 = Saturday
};

struct Rule {
  Frequency frequency = Frequency::kNone;
  bool supported = true;
  int interval = 1;
  int count = 0;           // 0: no COUNT
  DateTime until;          // invalid: no UNTIL
  uint8_t week_start = 1;  // Monday
  std::vector<WeekdayRule> by_day;
  std::vector<int8_t> by_month_day;  // 1..31 or -31..-1
  uint16_t by_month = 0;             // bit m set for month m (1..12)
  std::vector<int16_t> by_set_pos;
};

// Reads the value of an RRULE property. zone is used for an UNTIL without
// "Z". False if FREQ is missing or a part is malformed.
bool parseRule(std::string_view text, uint8_t zone, Rule& rule);

// Visits the occurrences of a rule in order, as local seconds (wall time of
// start, "as if UTC"): first start itself, then the following ones, until
// visit returns false, COUNT is reached or the date passes last_day (days
// since 1970). Without COUNT, periods that end before first_day are skipped
// without visiting (a speed-up for old series). UNTIL is not checked here,
// because comparing it may need the time zone: visit stops at it.
void expandRule(const Rule& rule, const DateTime& start, int64_t first_day, int64_t last_day,
                const std::function<bool(int64_t local_s)>& visit);

}  // namespace widgets::calendar
