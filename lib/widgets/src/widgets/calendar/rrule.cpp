#include "widgets/calendar/rrule.h"

#include <algorithm>
#include <utility>

namespace widgets::calendar {

namespace {

// Longest walk through the periods of one rule (e.g. 20000 days or weeks).
constexpr int64_t kMaxPeriods = 20000;

bool parseInt(std::string_view text, int min, int max, int& out) {
  if (text.empty()) return false;
  size_t i = 0;
  int sign = 1;
  if (text[0] == '+' || text[0] == '-') {
    sign = text[0] == '-' ? -1 : 1;
    i++;
  }
  if (i >= text.size()) return false;
  int value = 0;
  for (; i < text.size(); i++) {
    if (text[i] < '0' || text[i] > '9' || value > 100000) return false;
    value = value * 10 + (text[i] - '0');
  }
  value *= sign;
  if (value < min || value > max) return false;
  out = value;
  return true;
}

int weekdayCode(std::string_view code) {
  static constexpr const char* kCodes[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
  for (int i = 0; i < 7; i++) {
    if (code == kCodes[i]) return i;
  }
  return -1;
}

// Calls f for every item of a comma separated list; false if f fails.
template <typename F>
bool forEachItem(std::string_view list, F f) {
  while (true) {
    const size_t comma = list.find(',');
    if (!f(list.substr(0, comma))) return false;
    if (comma == std::string_view::npos) return true;
    list.remove_prefix(comma + 1);
  }
}

bool parseWeekday(std::string_view item, WeekdayRule& out) {
  if (item.size() < 2) return false;
  const int weekday = weekdayCode(item.substr(item.size() - 2));
  if (weekday < 0) return false;
  int ordinal = 0;
  if (item.size() > 2 && !parseInt(item.substr(0, item.size() - 2), -53, 53, ordinal)) return false;
  if (item.size() > 2 && ordinal == 0) return false;
  out.ordinal = static_cast<int8_t>(ordinal);
  out.weekday = static_cast<uint8_t>(weekday);
  return true;
}

bool inMonthMask(uint16_t mask, int month) { return mask == 0 || (mask & (1u << month)) != 0; }

bool matchesMonthDay(const std::vector<int8_t>& by_month_day, int year, int month, int day) {
  if (by_month_day.empty()) return true;
  const int dim = daysInMonth(year, month);
  for (int8_t md : by_month_day) {
    if (md > 0 && md == day) return true;
    if (md < 0 && dim + 1 + md == day) return true;
  }
  return false;
}

bool matchesWeekday(const std::vector<WeekdayRule>& by_day, int weekday) {
  if (by_day.empty()) return true;
  for (const WeekdayRule& w : by_day) {
    if (w.weekday == weekday) return true;
  }
  return false;
}

// Days (since 1970) from first to first + length - 1 that are the given
// weekday, the n-th (or n-th from the end) only if ordinal != 0.
void weekdaysIn(int64_t first, int64_t length, const WeekdayRule& w, std::vector<int64_t>& out) {
  const int64_t offset = (w.weekday - weekdayFromDays(first) + 7) % 7;
  const int64_t count = offset < length ? (length - 1 - offset) / 7 + 1 : 0;
  if (w.ordinal == 0) {
    for (int64_t i = 0; i < count; i++) out.push_back(first + offset + 7 * i);
  } else if (w.ordinal > 0 && w.ordinal <= count) {
    out.push_back(first + offset + 7 * (w.ordinal - 1));
  } else if (w.ordinal < 0 && -w.ordinal <= count) {
    out.push_back(first + offset + 7 * (count + w.ordinal));
  }
}

// Days of one month that match BYMONTHDAY and/or BYDAY; the day of the
// start if neither is given.
void monthDays(const Rule& rule, int year, int month, int default_day, std::vector<int64_t>& out) {
  const int dim = daysInMonth(year, month);
  const int64_t first = daysFromCivil(year, month, 1);
  if (rule.by_day.empty()) {
    if (rule.by_month_day.empty()) {
      if (default_day <= dim) out.push_back(first + default_day - 1);
      return;
    }
    for (int8_t md : rule.by_month_day) {
      const int day = md > 0 ? md : dim + 1 + md;
      if (day >= 1 && day <= dim) out.push_back(first + day - 1);
    }
    return;
  }
  std::vector<int64_t> days;
  for (const WeekdayRule& w : rule.by_day) weekdaysIn(first, dim, w, days);
  for (int64_t d : days) {
    if (matchesMonthDay(rule.by_month_day, year, month, static_cast<int>(d - first + 1))) {
      out.push_back(d);
    }
  }
}

int64_t monthIndex(int64_t days) {
  int y, m, d;
  civilFromDays(days, y, m, d);
  return static_cast<int64_t>(y) * 12 + (m - 1);
}

int64_t weekOf(int64_t day, int week_start) {
  return day - (weekdayFromDays(day) - week_start + 7) % 7;
}

// Candidate days of period k; returns the first day of the period.
int64_t candidates(const Rule& rule, const DateTime& start, int64_t k, std::vector<int64_t>& out) {
  out.clear();
  const int64_t start_day = start.days();
  switch (rule.frequency) {
    case Frequency::kDaily: {
      const int64_t d = start_day + k * rule.interval;
      int y, m, day;
      civilFromDays(d, y, m, day);
      if (inMonthMask(rule.by_month, m) && matchesMonthDay(rule.by_month_day, y, m, day) &&
          matchesWeekday(rule.by_day, weekdayFromDays(d))) {
        out.push_back(d);
      }
      return d;
    }
    case Frequency::kWeekly: {
      const int64_t week = weekOf(start_day, rule.week_start) + 7 * k * rule.interval;
      auto add = [&](int weekday) {
        const int64_t d = week + (weekday - rule.week_start + 7) % 7;
        int y, m, day;
        civilFromDays(d, y, m, day);
        if (inMonthMask(rule.by_month, m)) out.push_back(d);
      };
      if (rule.by_day.empty()) {
        add(weekdayFromDays(start_day));
      } else {
        for (const WeekdayRule& w : rule.by_day) add(w.weekday);
      }
      return week;
    }
    case Frequency::kMonthly: {
      const int64_t index = monthIndex(start_day) + k * rule.interval;
      const int year = static_cast<int>(floorDiv(index, 12));
      const int month = static_cast<int>(index - static_cast<int64_t>(year) * 12 + 1);
      if (inMonthMask(rule.by_month, month)) monthDays(rule, year, month, start.day, out);
      return daysFromCivil(year, month, 1);
    }
    case Frequency::kYearly: {
      const int year = static_cast<int>(start.year + k * rule.interval);
      if (rule.by_month != 0) {
        for (int m = 1; m <= 12; m++) {
          if (rule.by_month & (1u << m)) monthDays(rule, year, m, start.day, out);
        }
      } else if (!rule.by_day.empty() && rule.by_month_day.empty()) {
        const int64_t first = daysFromCivil(year, 1, 1);
        const int64_t length = daysFromCivil(year + 1, 1, 1) - first;
        for (const WeekdayRule& w : rule.by_day) weekdaysIn(first, length, w, out);
      } else {
        monthDays(rule, year, start.month, start.day, out);
      }
      return daysFromCivil(year, 1, 1);
    }
    case Frequency::kNone:
      break;
  }
  return start_day;
}

// First period that may contain first_day (without COUNT only).
int64_t firstPeriod(const Rule& rule, const DateTime& start, int64_t first_day) {
  const int64_t start_day = start.days();
  if (first_day <= start_day) return 0;
  int64_t k = 0;
  switch (rule.frequency) {
    case Frequency::kDaily:
      k = (first_day - start_day) / rule.interval;
      break;
    case Frequency::kWeekly:
      k = (first_day - weekOf(start_day, rule.week_start)) / (7 * rule.interval);
      break;
    case Frequency::kMonthly:
      k = (monthIndex(first_day) - monthIndex(start_day)) / rule.interval;
      break;
    case Frequency::kYearly: {
      int y, m, d;
      civilFromDays(first_day, y, m, d);
      k = (y - start.year) / rule.interval;
      break;
    }
    case Frequency::kNone:
      break;
  }
  return std::max<int64_t>(0, k - 1);
}

}  // namespace

bool parseRule(std::string_view text, uint8_t zone, Rule& rule) {
  Rule r;
  bool ok = true;
  while (!text.empty() && ok) {
    const size_t semicolon = text.find(';');
    const std::string_view part = text.substr(0, semicolon);
    text = semicolon == std::string_view::npos ? std::string_view() : text.substr(semicolon + 1);
    const size_t eq = part.find('=');
    if (eq == std::string_view::npos) {
      if (!part.empty()) ok = false;
      continue;
    }
    const std::string_view key = part.substr(0, eq);
    const std::string_view value = part.substr(eq + 1);
    int n = 0;
    if (key == "FREQ") {
      if (value == "DAILY") {
        r.frequency = Frequency::kDaily;
      } else if (value == "WEEKLY") {
        r.frequency = Frequency::kWeekly;
      } else if (value == "MONTHLY") {
        r.frequency = Frequency::kMonthly;
      } else if (value == "YEARLY") {
        r.frequency = Frequency::kYearly;
      } else if (value == "HOURLY" || value == "MINUTELY" || value == "SECONDLY") {
        r.frequency = Frequency::kDaily;
        r.supported = false;
      } else {
        ok = false;
      }
    } else if (key == "INTERVAL") {
      ok = parseInt(value, 1, 1000, r.interval);
    } else if (key == "COUNT") {
      ok = parseInt(value, 1, 100000, r.count);
    } else if (key == "UNTIL") {
      ok = parseDateTime(value, false, zone, r.until);
    } else if (key == "WKST") {
      n = weekdayCode(value);
      ok = n >= 0;
      r.week_start = static_cast<uint8_t>(n);
    } else if (key == "BYDAY") {
      ok = forEachItem(value, [&](std::string_view item) {
        WeekdayRule w;
        if (!parseWeekday(item, w)) return false;
        r.by_day.push_back(w);
        return true;
      });
    } else if (key == "BYMONTHDAY") {
      ok = forEachItem(value, [&](std::string_view item) {
        if (!parseInt(item, -31, 31, n) || n == 0) return false;
        r.by_month_day.push_back(static_cast<int8_t>(n));
        return true;
      });
    } else if (key == "BYMONTH") {
      ok = forEachItem(value, [&](std::string_view item) {
        if (!parseInt(item, 1, 12, n)) return false;
        r.by_month |= static_cast<uint16_t>(1u << n);
        return true;
      });
    } else if (key == "BYSETPOS") {
      ok = forEachItem(value, [&](std::string_view item) {
        if (!parseInt(item, -366, 366, n) || n == 0) return false;
        r.by_set_pos.push_back(static_cast<int16_t>(n));
        return true;
      });
    } else if (key == "BYHOUR" || key == "BYMINUTE" || key == "BYSECOND" || key == "BYWEEKNO" ||
               key == "BYYEARDAY") {
      r.supported = false;
    }
    // Other keys (extensions such as X-...) are ignored.
  }
  if (!ok || r.frequency == Frequency::kNone) return false;
  rule = std::move(r);
  return true;
}

void expandRule(const Rule& rule, const DateTime& start, int64_t first_day, int64_t last_day,
                const std::function<bool(int64_t local_s)>& visit) {
  const int64_t start_day = start.days();
  if (start_day > last_day) return;
  const int64_t time_of_day = start.localSeconds() - start_day * kSecondsPerDay;
  if (!visit(start.localSeconds())) return;
  if (rule.frequency == Frequency::kNone || !rule.supported || rule.count == 1) return;

  int emitted = 1;
  std::vector<int64_t> days;
  std::vector<int64_t> selected;
  const int64_t k0 = rule.count == 0 ? firstPeriod(rule, start, first_day) : 0;
  for (int64_t k = k0; k < k0 + kMaxPeriods; k++) {
    const int64_t period_start = candidates(rule, start, k, days);
    if (period_start > last_day) return;
    std::sort(days.begin(), days.end());
    days.erase(std::unique(days.begin(), days.end()), days.end());
    const std::vector<int64_t>* list = &days;
    if (!rule.by_set_pos.empty()) {
      selected.clear();
      const int64_t size = static_cast<int64_t>(days.size());
      for (int16_t pos : rule.by_set_pos) {
        const int64_t i = pos > 0 ? pos - 1 : size + pos;
        if (i >= 0 && i < size) selected.push_back(days[static_cast<size_t>(i)]);
      }
      std::sort(selected.begin(), selected.end());
      selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
      list = &selected;
    }
    for (int64_t d : *list) {
      if (d <= start_day) continue;
      if (d > last_day) return;
      if (!visit(d * kSecondsPerDay + time_of_day)) return;
      if (rule.count != 0 && ++emitted >= rule.count) return;
    }
  }
}

}  // namespace widgets::calendar
