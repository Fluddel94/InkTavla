#include "widgets/calendar/timezone.h"

#include <ctime>
#include <limits>

#include "core/time.h"

namespace widgets::calendar {

namespace {

constexpr int64_t kNone = std::numeric_limits<int64_t>::min();

// Latest onset of an observance at or before utc (kNone if none).
int64_t latestOnset(const Observance& o, int64_t utc) {
  int64_t best = kNone;
  auto consider = [&](const DateTime& dt) {
    const int64_t onset =
        dt.zone == kZoneUtc ? dt.localSeconds() : dt.localSeconds() - o.offset_from;
    if (onset <= utc && onset > best) best = onset;
  };
  if (o.rule.frequency == Frequency::kNone) {
    consider(o.start);
  } else {
    const int64_t day = floorDiv(utc, kSecondsPerDay);
    int64_t until = kNone;
    if (o.rule.until.valid()) {
      until = o.rule.until.zone == kZoneUtc ? o.rule.until.localSeconds()
                                            : o.rule.until.localSeconds() - o.offset_from;
    }
    expandRule(o.rule, o.start, day - 400, day + 1, [&](int64_t local) {
      const int64_t onset = local - o.offset_from;
      if (until != kNone && onset > until) return false;
      if (onset > utc) return false;
      if (onset > best) best = onset;
      return true;
    });
  }
  for (const DateTime& dt : o.rdates) consider(dt);
  return best;
}

}  // namespace

int zoneOffsetAt(const Zone& zone, int64_t utc) {
  if (zone.observances.empty()) return 0;
  int64_t best = kNone;
  int offset = 0;
  const Observance* earliest = &zone.observances.front();
  for (const Observance& o : zone.observances) {
    const int64_t onset = latestOnset(o, utc);
    if (onset != kNone && onset >= best) {
      best = onset;
      offset = o.offset_to;
    }
    if (o.start.localSeconds() < earliest->start.localSeconds()) earliest = &o;
  }
  return best == kNone ? earliest->offset_from : offset;
}

int64_t zoneToUtc(const Zone& zone, int64_t local_s) {
  int64_t result = kNone;
  auto check = [&](int offset) {
    const int64_t utc = local_s - offset;
    if (zoneOffsetAt(zone, utc) == offset && (result == kNone || utc < result)) result = utc;
  };
  for (const Observance& o : zone.observances) {
    check(o.offset_to);
    check(o.offset_from);
  }
  if (result != kNone) return result;
  // In a gap: the offset in effect a few hours earlier.
  return local_s - zoneOffsetAt(zone, local_s - zoneOffsetAt(zone, local_s) - 6 * 3600);
}

int64_t deviceToUtc(int64_t local_s) {
  const DateTime dt = fromLocalSeconds(local_s);
  struct tm tm = {};
  tm.tm_year = dt.year - 1900;
  tm.tm_mon = dt.month - 1;
  tm.tm_mday = dt.day;
  tm.tm_hour = dt.hour;
  tm.tm_min = dt.minute;
  tm.tm_sec = dt.second;
  tm.tm_isdst = -1;
  const time_t t = mktime(&tm);
  if (t == static_cast<time_t>(-1)) return local_s;
  return static_cast<int64_t>(t);
}

int64_t deviceLocalSeconds(int64_t utc) {
  core::LocalTime lt;
  if (!core::toLocalTime(utc, lt)) return utc;
  return daysFromCivil(lt.year, lt.month, lt.day) * kSecondsPerDay + lt.hour * 3600 +
         lt.minute * 60 + lt.second;
}

}  // namespace widgets::calendar
