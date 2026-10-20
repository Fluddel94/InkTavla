#pragma once

// Dates and times as they appear in iCalendar files (RFC 5545): calendar
// arithmetic without the C library, so that it does not depend on the time
// zone of the process.

#include <cstdint>
#include <string_view>

namespace widgets::calendar {

inline constexpr int64_t kSecondsPerDay = 86400;

// Days since 1970-01-01 for a date of the proleptic Gregorian calendar.
int64_t daysFromCivil(int year, int month, int day);
void civilFromDays(int64_t days, int& year, int& month, int& day);
// 0 = Sunday .. 6 = Saturday.
int weekdayFromDays(int64_t days);
int daysInMonth(int year, int month);
// ISO 8601 week number (1..53) of a day: weeks start on Monday, week 1
// contains the first Thursday of the year.
int isoWeek(int64_t days);
// Floor division, also for negative numbers.
int64_t floorDiv(int64_t a, int64_t b);

// Zone of a DATE-TIME: UTC ("Z"), floating (no zone: local time of the
// device) or an index into the TZID names of the parsed file.
inline constexpr uint8_t kZoneFloating = 0xFF;
inline constexpr uint8_t kZoneUtc = 0xFE;

struct DateTime {
  int16_t year = 0;
  int8_t month = 0;
  int8_t day = 0;
  int8_t hour = 0;
  int8_t minute = 0;
  int8_t second = 0;
  bool date_only = false;  // VALUE=DATE, e.g. all-day events
  uint8_t zone = kZoneFloating;

  bool valid() const { return month != 0; }
  int64_t days() const { return daysFromCivil(year, month, day); }
  // Wall time as seconds "as if UTC" (local seconds); midnight for dates.
  int64_t localSeconds() const;
};

// Reads "20261020", "20261020T083000" or "20261020T083000Z". zone is used
// unless the value ends with "Z". False for anything else.
bool parseDateTime(std::string_view text, bool date_only, uint8_t zone, DateTime& out);

// The date and time of local seconds.
DateTime fromLocalSeconds(int64_t local_s, bool date_only = false, uint8_t zone = kZoneFloating);

// Reads a DURATION or an offset: "PT1H30M", "-P1D", "P2W". False if invalid.
bool parseDuration(std::string_view text, int64_t& seconds);

// Reads a UTC offset of a VTIMEZONE: "+0100", "-0430", "+013000".
bool parseUtcOffset(std::string_view text, int& seconds);

}  // namespace widgets::calendar
