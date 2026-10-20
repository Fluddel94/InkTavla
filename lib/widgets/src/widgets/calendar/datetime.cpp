#include "widgets/calendar/datetime.h"

namespace widgets::calendar {

namespace {

bool digits(std::string_view text, size_t pos, size_t count, int& out) {
  if (pos + count > text.size()) return false;
  out = 0;
  for (size_t i = pos; i < pos + count; i++) {
    const char c = text[i];
    if (c < '0' || c > '9') return false;
    out = out * 10 + (c - '0');
  }
  return true;
}

}  // namespace

int64_t floorDiv(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
  return q;
}

// Algorithms by Howard Hinnant ("chrono-Compatible Low-Level Date
// Algorithms"), public domain.
int64_t daysFromCivil(int year, int month, int day) {
  const int64_t y = static_cast<int64_t>(year) - (month <= 2 ? 1 : 0);
  const int64_t era = floorDiv(y, 400);
  const int64_t yoe = y - era * 400;
  const int64_t mp = (month + 9) % 12;
  const int64_t doy = (153 * mp + 2) / 5 + day - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void civilFromDays(int64_t days, int& year, int& month, int& day) {
  const int64_t z = days + 719468;
  const int64_t era = floorDiv(z, 146097);
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(yoe + era * 400 + (month <= 2 ? 1 : 0));
}

int weekdayFromDays(int64_t days) { return static_cast<int>(((days % 7) + 11) % 7); }

int isoWeek(int64_t days) {
  // Thursday of the same week decides the year.
  const int64_t thursday = days - (weekdayFromDays(days) + 6) % 7 + 3;
  int y, m, d;
  civilFromDays(thursday, y, m, d);
  return static_cast<int>((thursday - daysFromCivil(y, 1, 1)) / 7 + 1);
}

int daysInMonth(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return leap ? 29 : 28;
  }
  return kDays[month - 1];
}

int64_t DateTime::localSeconds() const {
  return days() * kSecondsPerDay + hour * 3600 + minute * 60 + second;
}

bool parseDateTime(std::string_view text, bool date_only, uint8_t zone, DateTime& out) {
  DateTime dt;
  int y, m, d;
  if (!digits(text, 0, 4, y) || !digits(text, 4, 2, m) || !digits(text, 6, 2, d)) return false;
  if (m < 1 || m > 12 || d < 1 || d > daysInMonth(y, m)) return false;
  dt.year = static_cast<int16_t>(y);
  dt.month = static_cast<int8_t>(m);
  dt.day = static_cast<int8_t>(d);
  if (text.size() == 8) {
    dt.date_only = true;  // a date, even if VALUE=DATE is missing
  } else {
    if (date_only || text.size() < 15 || text[8] != 'T') return false;
    int h, mi, s;
    if (!digits(text, 9, 2, h) || !digits(text, 11, 2, mi) || !digits(text, 13, 2, s)) {
      return false;
    }
    if (h > 23 || mi > 59 || s > 60) return false;
    dt.hour = static_cast<int8_t>(h);
    dt.minute = static_cast<int8_t>(mi);
    dt.second = static_cast<int8_t>(s > 59 ? 59 : s);
    if (text.size() == 16 && text[15] == 'Z') {
      dt.zone = kZoneUtc;
    } else if (text.size() == 15) {
      dt.zone = zone;
    } else {
      return false;
    }
  }
  out = dt;
  return true;
}

DateTime fromLocalSeconds(int64_t local_s, bool date_only, uint8_t zone) {
  const int64_t days = floorDiv(local_s, kSecondsPerDay);
  const int64_t rest = local_s - days * kSecondsPerDay;
  int y, m, d;
  civilFromDays(days, y, m, d);
  DateTime dt;
  dt.year = static_cast<int16_t>(y);
  dt.month = static_cast<int8_t>(m);
  dt.day = static_cast<int8_t>(d);
  dt.date_only = date_only;
  dt.zone = zone;
  if (!date_only) {
    dt.hour = static_cast<int8_t>(rest / 3600);
    dt.minute = static_cast<int8_t>(rest / 60 % 60);
    dt.second = static_cast<int8_t>(rest % 60);
  }
  return dt;
}

bool parseDuration(std::string_view text, int64_t& seconds) {
  size_t i = 0;
  int sign = 1;
  if (i < text.size() && (text[i] == '+' || text[i] == '-')) sign = text[i++] == '-' ? -1 : 1;
  if (i >= text.size() || text[i++] != 'P') return false;
  int64_t total = 0;
  bool time_part = false;
  bool any = false;
  while (i < text.size()) {
    if (text[i] == 'T') {
      time_part = true;
      i++;
      continue;
    }
    int64_t n = 0;
    size_t start = i;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') n = n * 10 + (text[i++] - '0');
    if (i == start || i >= text.size() || n > 100000) return false;
    const char unit = text[i++];
    if (!time_part && unit == 'W') {
      total += n * 7 * kSecondsPerDay;
    } else if (!time_part && unit == 'D') {
      total += n * kSecondsPerDay;
    } else if (time_part && unit == 'H') {
      total += n * 3600;
    } else if (time_part && unit == 'M') {
      total += n * 60;
    } else if (time_part && unit == 'S') {
      total += n;
    } else {
      return false;
    }
    any = true;
  }
  if (!any) return false;
  seconds = sign * total;
  return true;
}

bool parseUtcOffset(std::string_view text, int& seconds) {
  if (text.size() != 5 && text.size() != 7) return false;
  if (text[0] != '+' && text[0] != '-') return false;
  int h, m, s = 0;
  if (!digits(text, 1, 2, h) || !digits(text, 3, 2, m)) return false;
  if (text.size() == 7 && !digits(text, 5, 2, s)) return false;
  if (h > 23 || m > 59 || s > 59) return false;
  seconds = (text[0] == '-' ? -1 : 1) * (h * 3600 + m * 60 + s);
  return true;
}

}  // namespace widgets::calendar
