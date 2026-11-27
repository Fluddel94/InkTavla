#include "widgets/calendar/agenda.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "core/time.h"
#include "ui/emoji.h"
#include "widgets/calendar/timezone.h"

namespace widgets::calendar {

namespace {

int64_t toUtc(const Parser& parser, const DateTime& dt) {
  if (dt.zone == kZoneUtc) return dt.localSeconds();
  const Zone* zone = parser.zone(dt.zone);
  return zone != nullptr ? zoneToUtc(*zone, dt.localSeconds()) : deviceToUtc(dt.localSeconds());
}

// Identifies an occurrence for EXDATE and RECURRENCE-ID: the day for
// all-day events, the instant otherwise.
int64_t occurrenceKey(const Parser& parser, const DateTime& dt) {
  return dt.date_only ? dt.days() : toUtc(parser, dt);
}

// Length of an event: days for all-day events, seconds otherwise.
int64_t lengthOf(const Parser& parser, const Event& e) {
  if (e.start.date_only) {
    if (e.end.valid()) return std::max<int64_t>(1, e.end.days() - e.start.days());
    if (e.duration >= 0) return std::max<int64_t>(1, e.duration / kSecondsPerDay);
    return 1;
  }
  if (e.end.valid()) return std::max<int64_t>(0, toUtc(parser, e.end) - toUtc(parser, e.start));
  return e.duration >= 0 ? e.duration : 0;
}

Occurrence makeOccurrence(const Parser& parser, const Event& e, const DateTime& start,
                          int64_t length) {
  Occurrence o;
  o.summary = e.summary;
  o.location = e.location;
  if (start.date_only) {
    o.all_day = true;
    o.first_day = start.days();
    o.last_day = o.first_day + length - 1;
    o.start = deviceToUtc(o.first_day * kSecondsPerDay);
    o.end = deviceToUtc((o.last_day + 1) * kSecondsPerDay);
  } else {
    o.start = toUtc(parser, start);
    o.end = o.start + length;
    o.first_day = localDay(o.start);
    o.last_day = localDay(std::max(o.start, o.end - 1));
  }
  return o;
}

std::string clockText(int64_t utc) {
  const int64_t local = deviceLocalSeconds(utc);
  const int64_t seconds = local - floorDiv(local, kSecondsPerDay) * kSecondsPerDay;
  char text[8];
  std::snprintf(text, sizeof(text), "%02d:%02d", static_cast<int>(seconds / 3600),
                static_cast<int>(seconds / 60 % 60));
  return text;
}

// "bis Fr" for a last day within the week, "bis 3.11." after that.
std::string untilDay(int64_t last_day, int64_t day, ui::Language language) {
  std::string out = ui::text(language, ui::Text::kUntil);
  out += ' ';
  if (last_day - day < 7) {
    out += ui::weekdayShort(language, weekdayFromDays(last_day));
  } else {
    int y, m, d;
    civilFromDays(last_day, y, m, d);
    out += std::to_string(d) + "." + std::to_string(m) + ".";
  }
  return out;
}

std::string countdown(int64_t now_s, int64_t start, ui::Language language) {
  const int64_t minutes = core::minutesUntil(now_s, start);
  std::string out = ui::text(language, ui::Text::kIn);
  out += ' ';
  if (minutes < kCountdownMinutes) {
    out += std::to_string(minutes) + " " + ui::text(language, ui::Text::kMinutesUnit);
  } else {
    out += std::to_string((minutes + 30) / 60) + " " + ui::text(language, ui::Text::kHoursUnit);
  }
  return out;
}

}  // namespace

int64_t localDay(int64_t utc) { return floorDiv(deviceLocalSeconds(utc), kSecondsPerDay); }

Occurrences expandEvents(const Parser& parser, int64_t first_day, int64_t last_day,
                         bool* truncated) {
  struct Override {
    uint64_t uid;
    int64_t key;
  };
  std::vector<Override> overrides;
  for (const Event& e : parser.events()) {
    if (e.recurrence_id.valid())
      overrides.push_back({e.uid, occurrenceKey(parser, e.recurrence_id)});
  }

  const auto earlier = [](const Occurrence& a, const Occurrence& b) {
    if (a.first_day != b.first_day) return a.first_day < b.first_day;
    if (a.all_day != b.all_day) return a.all_day;
    return a.start < b.start;
  };
  Occurrences out;
  bool full = false;
  auto add = [&](Occurrence o) {
    if (o.last_day < first_day || o.first_day > last_day) return;
    if (out.size() < kMaxOccurrences) {
      out.push_back(std::move(o));
      return;
    }
    // Full: the latest one gives way to an earlier one, so the limit cuts
    // the last days, never today (CR2 M3). Rare, so a simple search.
    full = true;
    auto latest = std::max_element(out.begin(), out.end(), earlier);
    if (earlier(o, *latest)) *latest = std::move(o);
  };

  for (const Event& e : parser.events()) {
    const int64_t length = lengthOf(parser, e);
    if (e.recurrence_id.valid()) {
      if (!e.cancelled) add(makeOccurrence(parser, e, e.start, length));
      continue;
    }
    if (e.rule.frequency == Frequency::kNone) {
      add(makeOccurrence(parser, e, e.start, length));
      continue;
    }
    std::vector<int64_t> excluded;
    for (const DateTime& x : e.exdates) excluded.push_back(occurrenceKey(parser, x));
    const DateTime& until = e.rule.until;
    const int64_t span_days = e.start.date_only ? length : length / kSecondsPerDay + 1;
    expandRule(e.rule, e.start, first_day - span_days - 2, last_day + 2, [&](int64_t local) {
      const DateTime occurrence = fromLocalSeconds(local, e.start.date_only, e.start.zone);
      if (until.valid()) {
        if (until.date_only || e.start.date_only) {
          const int64_t until_day = floorDiv(until.localSeconds(), kSecondsPerDay);
          if (occurrence.days() > until_day) return false;
        } else if (until.zone == kZoneUtc) {
          if (toUtc(parser, occurrence) > until.localSeconds()) return false;
        } else if (local > until.localSeconds()) {
          return false;
        }
      }
      if (occurrence.days() > last_day + 1) return false;
      const int64_t key = occurrenceKey(parser, occurrence);
      if (std::find(excluded.begin(), excluded.end(), key) != excluded.end()) return true;
      for (const Override& o : overrides) {
        if (o.uid == e.uid && o.key == key) return true;
      }
      add(makeOccurrence(parser, e, occurrence, length));
      return true;
    });
  }

  std::stable_sort(out.begin(), out.end(), earlier);
  if (truncated != nullptr) *truncated = full;
  return out;
}

std::vector<AgendaDay> buildAgenda(const Occurrences& occurrences, int64_t now_s,
                                   ui::Language language, const DisplayConfig& display) {
  std::vector<AgendaDay> agenda;
  const int64_t today = localDay(now_s);
  for (int64_t d = today; d < today + display.days; d++) {
    AgendaDay day;
    day.day = d;
    std::vector<Entry> timed;
    bool next_found = false;
    for (const Occurrence& o : occurrences) {
      if (o.last_day < d || o.first_day > d) continue;
      const bool timed_start = !o.all_day && o.first_day == d;
      if (!display.all_day && !timed_start) continue;
      const std::string_view summary(o.summary.data(), o.summary.size());
      if (excluded(display, summary)) continue;
      Entry entry;
      // Emoji become icons (or disappear): the fonts have no emoji.
      entry.title = ui::mapEmoji(summary);
      entry.location = ui::mapEmoji(std::string_view(o.location.data(), o.location.size()));
      // Over (events without length count for one minute); shown until
      // midnight unless switched off.
      entry.past = d == today && !o.all_day && std::max(o.end, o.start + 60) <= now_s;
      if (entry.past && !display.past) continue;
      if (o.all_day) {
        entry.kind = Entry::Kind::kAllDay;
        entry.continues = o.first_day < d;
        entry.suffix = o.last_day > d ? untilDay(o.last_day, d, language)
                                      : ui::text(language, ui::Text::kAllDay);
        day.entries.push_back(std::move(entry));
      } else if (!timed_start) {
        // Continues from an earlier day: shown like an all-day event.
        entry.kind = Entry::Kind::kAllDay;
        entry.continues = true;
        entry.suffix = o.last_day == d ? std::string(ui::text(language, ui::Text::kUntil)) + " " +
                                             clockText(o.end)
                                       : untilDay(o.last_day, d, language);
        day.entries.push_back(std::move(entry));
      } else {
        entry.kind = Entry::Kind::kTimed;
        entry.time = clockText(o.start);
        // Events without length (end = start) and events that end on a
        // later day have no end time of their own.
        if (o.end > o.start && localDay(o.end - 1) == d) entry.end = clockText(o.end);
        if (d == today && !entry.past) {
          if (o.start <= now_s) {
            entry.note = ui::text(language, ui::Text::kRunning);
          } else if (!next_found) {
            next_found = true;
            entry.next = true;
            entry.note = countdown(now_s, o.start, language);
          }
        }
        timed.push_back(std::move(entry));
      }
    }
    for (Entry& e : timed) day.entries.push_back(std::move(e));
    if (day.entries.empty()) {
      if (d > today + 1) continue;
      Entry none;
      none.kind = Entry::Kind::kNone;
      none.title = ui::text(language, ui::Text::kNoEvents);
      day.entries.push_back(std::move(none));
    }
    agenda.push_back(std::move(day));
  }
  return agenda;
}

}  // namespace widgets::calendar
