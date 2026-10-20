#pragma once

#include <cstdint>

namespace core {

// Time helpers. All calculations use Unix time (seconds since 1970 UTC);
// local time only appears when a time is shown.

// Wall-clock times before this (2024-01-01 00:00 UTC) are treated as "not
// synchronised yet" (the clock starts at 1970 until NTP has answered).
inline constexpr int64_t kMinValidWallTime = 1704067200;

// Default time zone: Central European Time with daylight saving time.
inline constexpr char kDefaultTimeZone[] = "CET-1CEST,M3.5.0,M10.5.0/3";

// Milliseconds since boot from a 64-bit timer. Unlike millis() it does not
// overflow after 49.7 days. Compare durations as `now - start >= duration`.
int64_t monotonicMs();

// True if t looks like a synchronised wall-clock time.
inline bool isValidUnixTime(int64_t t) { return t >= kMinValidWallTime; }

// Checks the syntax of a POSIX time zone string such as
// "CET-1CEST,M3.5.0,M10.5.0/3" or "<+0530>-5:30". The C library silently
// falls back to UTC for invalid strings, so configuration values are checked
// first.
bool isValidTimeZone(const char* posix_tz);

// Sets the time zone for all local-time conversions (process-wide, via TZ).
// Invalid strings are replaced by kDefaultTimeZone; returns false then.
bool setTimeZone(const char* posix_tz);

struct LocalTime {
  int year = 1970;
  int month = 1;     // 1..12
  int day = 1;       // 1..31
  int hour = 0;      // 0..23
  int minute = 0;    // 0..59
  int second = 0;    // 0..60
  int weekday = 4;   // 0 = Sunday .. 6 = Saturday
  bool dst = false;  // daylight saving time in effect
};

// Converts Unix time to local time in the current time zone. False if the
// conversion fails.
bool toLocalTime(int64_t unix_s, LocalTime& out);

// Minutes from now until t, counted as the difference of the minute values
// like a clock does: 14:07:59 -> 14:09:00 is 2 minutes. Negative if t is in
// an earlier minute.
int64_t minutesUntil(int64_t now_s, int64_t t_s);

}  // namespace core
