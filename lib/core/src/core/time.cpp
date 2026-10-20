#include "core/time.h"

#include <cctype>
#include <cstdlib>
#include <ctime>

#if defined(ARDUINO)
#include <Arduino.h>
#endif
#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#else
#include <chrono>
#endif

namespace core {

namespace {

// --- POSIX TZ syntax: std offset [dst [offset] [,rule,rule]] -------------------

// Zone name: at least three letters, or <...> with letters, digits, + and -.
bool parseName(const char*& p) {
  if (*p == '<') {
    const char* start = ++p;
    while (std::isalnum(static_cast<unsigned char>(*p)) || *p == '+' || *p == '-') p++;
    if (*p != '>' || p - start < 3) return false;
    p++;
    return true;
  }
  const char* start = p;
  while (std::isalpha(static_cast<unsigned char>(*p))) p++;
  return p - start >= 3;
}

// Number with 1..max_digits digits, value <= max_value.
bool parseNumber(const char*& p, int max_digits, int max_value) {
  int value = 0;
  int digits = 0;
  while (std::isdigit(static_cast<unsigned char>(*p)) && digits < max_digits) {
    value = value * 10 + (*p - '0');
    p++;
    digits++;
  }
  return digits > 0 && value <= max_value && !std::isdigit(static_cast<unsigned char>(*p));
}

// [+-]hh[:mm[:ss]]
bool parseTime(const char*& p, int max_hours) {
  if (*p == '+' || *p == '-') p++;
  if (!parseNumber(p, 3, max_hours)) return false;
  for (int i = 0; i < 2 && *p == ':'; i++) {
    p++;
    if (!parseNumber(p, 2, 59)) return false;
  }
  return true;
}

// Jn | n | Mm.w.d, optionally followed by /time
bool parseRule(const char*& p) {
  if (*p == 'J') {
    p++;
    if (!parseNumber(p, 3, 365)) return false;
  } else if (*p == 'M') {
    p++;
    if (!parseNumber(p, 2, 12) || *p++ != '.') return false;
    if (!parseNumber(p, 1, 5) || *p++ != '.') return false;
    if (!parseNumber(p, 1, 6)) return false;
  } else if (!parseNumber(p, 3, 365)) {
    return false;
  }
  if (*p == '/') {
    p++;
    if (!parseTime(p, 167)) return false;
  }
  return true;
}

int64_t floorDiv(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

}  // namespace

int64_t monotonicMs() {
#if defined(ESP_PLATFORM)
  return esp_timer_get_time() / 1000;
#else
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  using std::chrono::steady_clock;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#endif
}

bool isValidTimeZone(const char* posix_tz) {
  if (posix_tz == nullptr) return false;
  const char* p = posix_tz;
  if (!parseName(p) || !parseTime(p, 24)) return false;
  if (*p == '\0') return true;
  if (!parseName(p)) return false;
  if (*p != ',' && *p != '\0' && !parseTime(p, 24)) return false;
  if (*p == '\0') return true;
  if (*p++ != ',' || !parseRule(p)) return false;
  if (*p++ != ',' || !parseRule(p)) return false;
  return *p == '\0';
}

bool setTimeZone(const char* posix_tz) {
  const bool valid = isValidTimeZone(posix_tz);
  setenv("TZ", valid ? posix_tz : kDefaultTimeZone, 1);
  tzset();
  return valid;
}

bool toLocalTime(int64_t unix_s, LocalTime& out) {
  const time_t t = static_cast<time_t>(unix_s);
  struct tm tm;
  if (localtime_r(&t, &tm) == nullptr) return false;
  out.year = tm.tm_year + 1900;
  out.month = tm.tm_mon + 1;
  out.day = tm.tm_mday;
  out.hour = tm.tm_hour;
  out.minute = tm.tm_min;
  out.second = tm.tm_sec;
  out.weekday = tm.tm_wday;
  out.dst = tm.tm_isdst > 0;
  return true;
}

int64_t minutesUntil(int64_t now_s, int64_t t_s) { return floorDiv(t_s, 60) - floorDiv(now_s, 60); }

#if defined(ARDUINO)
void startTimeSync(const char* posix_tz, const char* ntp_server) {
  const char* tz = isValidTimeZone(posix_tz) ? posix_tz : kDefaultTimeZone;
  configTzTime(tz, ntp_server);
}
#endif

}  // namespace core
