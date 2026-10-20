#pragma once

// From parsed events to what the tile shows: the occurrences of the coming
// days (series expanded, exceptions and moved occurrences applied, zones
// resolved) and the agenda built from them for the current time.

#include <cstdint>
#include <string>
#include <vector>

#include "ui/i18n.h"
#include "widgets/calendar/parser.h"

namespace widgets::calendar {

struct Occurrence {
  int64_t start = 0;      // UTC; all-day events: local midnight of the first day
  int64_t end = 0;        // UTC, exclusive
  int64_t first_day = 0;  // local days since 1970 (device time zone)
  int64_t last_day = 0;   // inclusive
  bool all_day = false;
  Text summary;
};

using Occurrences = std::vector<Occurrence, core::PsramAllocator<Occurrence>>;

// Most occurrences kept for the window.
inline constexpr size_t kMaxOccurrences = 200;

// Occurrences that touch the local days first_day..last_day, sorted by day,
// all-day events first, then by start time.
Occurrences expandEvents(const Parser& parser, int64_t first_day, int64_t last_day);

// --- Agenda ------------------------------------------------------------------

struct Entry {
  enum class Kind { kTimed, kAllDay, kNone };
  Kind kind = Kind::kTimed;
  std::string time;    // kTimed: "08:30"
  std::string title;   // kNone: "Keine Termine"
  std::string suffix;  // kAllDay: "ganztägig", "bis Fr", "bis 12:00"
  std::string note;    // right edge: "in 48 min", "läuft"
  bool next = false;   // the next event of today: bold
  bool past = false;   // an event of today that is over (stays until midnight)
};

struct AgendaDay {
  int64_t day = 0;  // local days since 1970
  std::vector<Entry> entries;
};

// Days shown: today and the following ones.
inline constexpr int kAgendaDays = 7;
// The countdown switches from minutes to hours from this many minutes on.
inline constexpr int kCountdownMinutes = 60;

// Agenda for the current time: today and tomorrow always (with a kNone
// entry if empty), the other days only if they have events. Events of today
// that are over stay until the day changes (marked past).
std::vector<AgendaDay> buildAgenda(const Occurrences& occurrences, int64_t now_s,
                                   ui::Language language, int days = kAgendaDays);

// Local day (since 1970) of an instant in the device's time zone.
int64_t localDay(int64_t utc);

}  // namespace widgets::calendar
