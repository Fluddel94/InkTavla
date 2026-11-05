#pragma once

// Streaming reader for iCalendar files (RFC 5545), as served by the private
// iCal address of Google Calendar, Outlook, Nextcloud or iCloud.
//
// The file is fed in pieces as it arrives and never held as a whole: it can
// be several megabytes with years of history. Long lines are unfolded,
// values that are not needed (descriptions, attachments) are skipped, and
// events that cannot touch the time window are dropped as soon as they end.
// What remains is a compact list of events and the time zones they use.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/psram.h"
#include "widgets/calendar/datetime.h"
#include "widgets/calendar/rrule.h"
#include "widgets/calendar/timezone.h"

namespace widgets::calendar {

// Longest title kept (bytes of UTF-8); longer ones are cut. The column shows
// far less.
inline constexpr size_t kMaxSummaryBytes = 96;  // also for LOCATION
// Most events kept from one file; more are dropped (truncated()).
inline constexpr size_t kMaxEvents = 400;

// Text kept in PSRAM on the device (titles of many events).
using Text = std::basic_string<char, std::char_traits<char>, core::PsramAllocator<char>>;

struct Event {
  uint64_t uid = 0;  // hash of the UID (overrides refer to their series)
  Text summary;
  Text location;  // LOCATION, one line; empty if missing
  DateTime start;
  DateTime end;           // invalid if missing
  int64_t duration = -1;  // DURATION in seconds, -1 if missing
  Rule rule;              // frequency kNone: a single event
  std::vector<DateTime> exdates;
  DateTime recurrence_id;  // valid: replaces this occurrence of its series
  bool cancelled = false;  // STATUS:CANCELLED
};

using Events = std::vector<Event, core::PsramAllocator<Event>>;

class Parser {
 public:
  // Keeps events that may touch [from_utc, to_utc).
  Parser(int64_t from_utc, int64_t to_utc);

  void feed(const char* data, size_t length);
  void feed(std::string_view text) { feed(text.data(), text.size()); }
  // Ends the input (a last line without line break).
  void finish();

  // True if a complete VCALENDAR was read.
  bool valid() const { return calendar_done_; }
  // True as soon as the input is known not to be an iCal file: it does not
  // start with BEGIN:VCALENDAR (after a byte order mark and blank lines),
  // e.g. the HTML sign-in page of a calendar that is no longer shared.
  bool notCalendar() const { return start_ == Start::kBad; }
  bool truncated() const { return truncated_; }
  size_t eventsRead() const { return events_read_; }

  const Events& events() const { return events_; }
  // Zone of a DateTime::zone index; nullptr for UTC, floating times and
  // TZIDs without a VTIMEZONE (those use the device's time zone).
  const Zone* zone(uint8_t index) const;

 private:
  enum class Component : uint8_t { kCalendar, kEvent, kTimezone, kObservance, kOther };
  enum class LineState : uint8_t { kName, kValue, kSkip };
  enum class Start : uint8_t { kUnknown, kGood, kBad };

  // Follows the start of the input; false for a byte to skip.
  bool checkStart(char c);
  void feedByte(char c);
  void endLine();
  bool startValue();
  void handleLine();
  void handleEvent(std::string_view name, std::string_view value);
  void handleObservance(std::string_view name, std::string_view value);
  void beginComponent(std::string_view type);
  void endComponent();
  void finishEvent();
  std::string_view param(std::string_view key) const;
  uint8_t zoneIndex(std::string_view tzid);
  bool parseDateParam(std::string_view value, DateTime& out);

  int64_t from_local_;  // window with a margin, compared with local seconds
  int64_t to_local_;

  // Current line.
  LineState state_ = LineState::kName;
  std::string name_;  // name with parameters
  std::string value_;
  size_t value_cap_ = 0;
  bool in_quotes_ = false;
  bool pending_newline_ = false;

  Start start_ = Start::kUnknown;
  size_t start_matched_ = 0;  // bytes of "BEGIN:VCALENDAR" seen so far

  std::vector<Component> stack_;
  bool calendar_done_ = false;
  bool truncated_ = false;
  size_t events_read_ = 0;

  Event event_;
  std::string rule_text_;
  Observance observance_;
  std::string observance_rule_;
  Zone timezone_;

  Events events_;
  std::vector<Zone> zones_;  // index = DateTime::zone
};

// Hash of a UID (FNV-1a, 64 bit).
uint64_t uidHash(std::string_view uid);

// Text value without the escapes of RFC 5545 ("\," "\;" "\\" "\n"); line
// breaks become spaces.
std::string unescapeText(std::string_view text);

}  // namespace widgets::calendar
