#include "widgets/calendar/parser.h"

#include <utility>

namespace widgets::calendar {

namespace {

// True if an occurrence of the series with COUNT ends at or after from_local
// (local seconds) and starts before to_local. Visits at most COUNT
// occurrences, only up to that point.
bool countReachesWindow(const Event& e, int64_t length_s, int64_t from_local, int64_t to_local) {
  bool reaches = false;
  expandRule(e.rule, e.start, floorDiv(from_local, kSecondsPerDay),
             floorDiv(to_local, kSecondsPerDay), [&](int64_t local) {
               if (local + length_s >= from_local) {
                 reaches = true;
                 return false;
               }
               return true;
             });
  return reaches;
}

// Name with parameters, e.g. "DTSTART;TZID=Europe/Berlin"; longer ones are
// skipped.
constexpr size_t kMaxName = 192;
// Events are kept if they may touch the window with this margin: times are
// compared before their zone is known (up to +-14 h), and all-day events
// depend on the local date.
constexpr int64_t kMargin = 2 * kSecondsPerDay;
// More TZIDs in one file are treated as floating.
constexpr size_t kMaxZones = 32;

bool equalsUpper(std::string_view a, std::string_view upper) {
  if (a.size() != upper.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    char c = a[i];
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c != upper[i]) return false;
  }
  return true;
}

// Cuts UTF-8 text to at most max bytes without splitting a character.
void cutUtf8(Text& text, size_t max) {
  if (text.size() <= max) return;
  size_t end = max;
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) end--;
  text.resize(end);
}

// Calls f for each item of a comma separated list.
template <typename F>
void forEachValue(std::string_view list, F f) {
  while (!list.empty()) {
    const size_t comma = list.find(',');
    f(list.substr(0, comma));
    if (comma == std::string_view::npos) break;
    list.remove_prefix(comma + 1);
  }
}

}  // namespace

uint64_t uidHash(std::string_view uid) {
  uint64_t h = 14695981039346656037ull;
  for (char c : uid) {
    h ^= static_cast<unsigned char>(c);
    h *= 1099511628211ull;
  }
  return h;
}

std::string unescapeText(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); i++) {
    const char c = text[i];
    if (c == '\\' && i + 1 < text.size()) {
      const char n = text[++i];
      out += (n == 'n' || n == 'N') ? ' ' : n;
    } else {
      out += c;
    }
  }
  return out;
}

Parser::Parser(int64_t from_utc, int64_t to_utc)
    : from_local_(from_utc - kMargin), to_local_(to_utc + kMargin) {}

const Zone* Parser::zone(uint8_t index) const {
  if (index >= zones_.size() || zones_[index].observances.empty()) return nullptr;
  return &zones_[index];
}

void Parser::feed(const char* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    // A byte order mark and blank lines before BEGIN:VCALENDAR are skipped.
    if (start_ == Start::kUnknown && !checkStart(data[i])) continue;
    feedByte(data[i]);
  }
}

bool Parser::checkStart(char c) {
  static constexpr std::string_view kStart = "BEGIN:VCALENDAR";
  const auto u = static_cast<unsigned char>(c);
  if (start_matched_ == 0 &&
      (u == 0xEF || u == 0xBB || u == 0xBF || c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
    return false;  // byte order mark, blank lines
  }
  const char upper = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
  if (upper != kStart[start_matched_]) {
    start_ = Start::kBad;
    return true;
  }
  if (++start_matched_ == kStart.size()) start_ = Start::kGood;
  return true;
}

void Parser::finish() {
  if (pending_newline_ || !name_.empty() || state_ != LineState::kName) {
    pending_newline_ = false;
    endLine();
  }
}

void Parser::feedByte(char c) {
  if (pending_newline_) {
    pending_newline_ = false;
    if (c == ' ' || c == '\t') return;  // folded line: continues
    endLine();
  }
  if (c == '\r') return;
  if (c == '\n') {
    pending_newline_ = true;
    return;
  }
  switch (state_) {
    case LineState::kName:
      if (c == '"') in_quotes_ = !in_quotes_;
      if (c == ':' && !in_quotes_) {
        state_ = startValue() ? LineState::kValue : LineState::kSkip;
        return;
      }
      if (name_.size() >= kMaxName) {
        state_ = LineState::kSkip;
        return;
      }
      name_ += c;
      return;
    case LineState::kValue:
      if (value_.size() < value_cap_) value_ += c;
      return;
    case LineState::kSkip:
      return;
  }
}

bool Parser::startValue() {
  const std::string_view full(name_);
  const std::string_view name = full.substr(0, full.find(';'));
  value_.clear();
  value_cap_ = 0;
  if (equalsUpper(name, "BEGIN") || equalsUpper(name, "END")) {
    value_cap_ = 32;
  } else if (!stack_.empty()) {
    switch (stack_.back()) {
      case Component::kEvent:
        if (equalsUpper(name, "SUMMARY") || equalsUpper(name, "UID") ||
            equalsUpper(name, "LOCATION")) {
          value_cap_ = 512;
        } else if (equalsUpper(name, "EXDATE")) {
          value_cap_ = 2048;
        } else if (equalsUpper(name, "DTSTART") || equalsUpper(name, "DTEND") ||
                   equalsUpper(name, "DURATION") || equalsUpper(name, "RECURRENCE-ID") ||
                   equalsUpper(name, "STATUS") || equalsUpper(name, "RRULE")) {
          value_cap_ = 512;
        }
        break;
      case Component::kTimezone:
        if (equalsUpper(name, "TZID")) value_cap_ = 128;
        break;
      case Component::kObservance:
        if (equalsUpper(name, "DTSTART") || equalsUpper(name, "TZOFFSETFROM") ||
            equalsUpper(name, "TZOFFSETTO") || equalsUpper(name, "RRULE") ||
            equalsUpper(name, "RDATE")) {
          value_cap_ = 512;
        }
        break;
      default:
        break;
    }
  }
  return value_cap_ > 0;
}

void Parser::endLine() {
  if (state_ == LineState::kValue) handleLine();
  state_ = LineState::kName;
  name_.clear();
  value_.clear();
  in_quotes_ = false;
}

std::string_view Parser::param(std::string_view key) const {
  std::string_view rest(name_);
  const size_t first = rest.find(';');
  if (first == std::string_view::npos) return {};
  rest.remove_prefix(first + 1);
  while (!rest.empty()) {
    // Split at ';' outside quotes.
    size_t end = 0;
    bool quoted = false;
    while (end < rest.size() && (quoted || rest[end] != ';')) {
      if (rest[end] == '"') quoted = !quoted;
      end++;
    }
    const std::string_view item = rest.substr(0, end);
    const size_t eq = item.find('=');
    if (eq != std::string_view::npos && equalsUpper(item.substr(0, eq), key)) {
      std::string_view value = item.substr(eq + 1);
      if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
      }
      return value;
    }
    rest.remove_prefix(end < rest.size() ? end + 1 : end);
  }
  return {};
}

uint8_t Parser::zoneIndex(std::string_view tzid) {
  if (tzid.empty()) return kZoneFloating;
  for (size_t i = 0; i < zones_.size(); i++) {
    if (zones_[i].name == tzid) return static_cast<uint8_t>(i);
  }
  if (zones_.size() >= kMaxZones) return kZoneFloating;
  Zone z;
  z.name = std::string(tzid);
  zones_.push_back(std::move(z));
  return static_cast<uint8_t>(zones_.size() - 1);
}

bool Parser::parseDateParam(std::string_view value, DateTime& out) {
  const bool date_only = equalsUpper(param("VALUE"), "DATE");
  return parseDateTime(value, date_only, zoneIndex(param("TZID")), out);
}

void Parser::handleLine() {
  const std::string_view full(name_);
  const std::string_view name = full.substr(0, full.find(';'));
  const std::string_view value(value_);
  if (equalsUpper(name, "BEGIN")) {
    beginComponent(value);
  } else if (equalsUpper(name, "END")) {
    endComponent();
  } else if (!stack_.empty() && stack_.back() == Component::kEvent) {
    handleEvent(name, value);
  } else if (!stack_.empty() && stack_.back() == Component::kTimezone) {
    if (equalsUpper(name, "TZID")) timezone_.name = std::string(value);
  } else if (!stack_.empty() && stack_.back() == Component::kObservance) {
    handleObservance(name, value);
  }
}

void Parser::beginComponent(std::string_view type) {
  Component c = Component::kOther;
  const bool top = stack_.empty();
  const Component parent = top ? Component::kOther : stack_.back();
  if (top && equalsUpper(type, "VCALENDAR")) {
    c = Component::kCalendar;
  } else if (parent == Component::kCalendar && equalsUpper(type, "VEVENT")) {
    c = Component::kEvent;
    event_ = Event();
    rule_text_.clear();
  } else if (parent == Component::kCalendar && equalsUpper(type, "VTIMEZONE")) {
    c = Component::kTimezone;
    timezone_ = Zone();
  } else if (parent == Component::kTimezone &&
             (equalsUpper(type, "STANDARD") || equalsUpper(type, "DAYLIGHT"))) {
    c = Component::kObservance;
    observance_ = Observance();
    observance_rule_.clear();
  }
  if (stack_.size() < 16) stack_.push_back(c);
}

void Parser::endComponent() {
  if (stack_.empty()) return;
  const Component c = stack_.back();
  stack_.pop_back();
  switch (c) {
    case Component::kCalendar:
      calendar_done_ = true;
      break;
    case Component::kEvent:
      finishEvent();
      break;
    case Component::kTimezone:
      if (!timezone_.name.empty() && !timezone_.observances.empty()) {
        const uint8_t index = zoneIndex(timezone_.name);
        if (index != kZoneFloating) zones_[index].observances = std::move(timezone_.observances);
      }
      break;
    case Component::kObservance:
      if (observance_.start.valid()) {
        if (!observance_rule_.empty()) {
          parseRule(observance_rule_, kZoneFloating, observance_.rule);
        }
        timezone_.observances.push_back(std::move(observance_));
      }
      break;
    case Component::kOther:
      break;
  }
}

void Parser::handleEvent(std::string_view name, std::string_view value) {
  if (equalsUpper(name, "UID")) {
    event_.uid = uidHash(value);
  } else if (equalsUpper(name, "SUMMARY")) {
    const std::string summary = unescapeText(value);
    event_.summary.assign(summary.data(), summary.size());
    cutUtf8(event_.summary, kMaxSummaryBytes);
  } else if (equalsUpper(name, "LOCATION")) {
    // Addresses come with line breaks ("\n", Outlook): one line, parts
    // separated by ", " (unescapeText() would only put a space).
    std::string location;
    for (size_t i = 0; i < value.size(); i++) {
      char c = value[i];
      if (c == '\\' && i + 1 < value.size()) {
        c = value[++i];
        if (c == 'n' || c == 'N') {
          while (!location.empty() && location.back() == ' ') location.pop_back();
          if (!location.empty() && location.back() != ',') location += ',';
          location += ' ';
          continue;
        }
      }
      location += c;
    }
    while (!location.empty() && (location.back() == ' ' || location.back() == ',')) {
      location.pop_back();
    }
    event_.location.assign(location.data(), location.size());
    cutUtf8(event_.location, kMaxSummaryBytes);
  } else if (equalsUpper(name, "DTSTART")) {
    parseDateParam(value, event_.start);
  } else if (equalsUpper(name, "DTEND")) {
    parseDateParam(value, event_.end);
  } else if (equalsUpper(name, "RECURRENCE-ID")) {
    parseDateParam(value, event_.recurrence_id);
  } else if (equalsUpper(name, "DURATION")) {
    int64_t seconds = 0;
    if (parseDuration(value, seconds) && seconds >= 0) event_.duration = seconds;
  } else if (equalsUpper(name, "RRULE")) {
    rule_text_ = std::string(value);
  } else if (equalsUpper(name, "STATUS")) {
    event_.cancelled = equalsUpper(value, "CANCELLED");
  } else if (equalsUpper(name, "EXDATE")) {
    forEachValue(value, [&](std::string_view item) {
      DateTime dt;
      if (!parseDateParam(item, dt)) return;
      const int64_t t = dt.localSeconds();
      if (t >= from_local_ && t <= to_local_) event_.exdates.push_back(dt);
    });
  }
}

void Parser::handleObservance(std::string_view name, std::string_view value) {
  int offset = 0;
  if (equalsUpper(name, "DTSTART")) {
    parseDateTime(value, false, kZoneFloating, observance_.start);
  } else if (equalsUpper(name, "TZOFFSETFROM")) {
    if (parseUtcOffset(value, offset)) observance_.offset_from = offset;
  } else if (equalsUpper(name, "TZOFFSETTO")) {
    if (parseUtcOffset(value, offset)) observance_.offset_to = offset;
  } else if (equalsUpper(name, "RRULE")) {
    observance_rule_ = std::string(value);
  } else if (equalsUpper(name, "RDATE")) {
    forEachValue(value, [&](std::string_view item) {
      DateTime dt;
      if (parseDateTime(item, false, kZoneFloating, dt) && observance_.rdates.size() < 64) {
        observance_.rdates.push_back(dt);
      }
    });
  }
}

void Parser::finishEvent() {
  events_read_++;
  Event& e = event_;
  if (!e.start.valid()) return;
  if (!rule_text_.empty() && !e.recurrence_id.valid()) {
    if (!parseRule(rule_text_, e.start.zone, e.rule)) e.rule = Rule();
  }
  const bool recurring = e.rule.frequency != Frequency::kNone;
  if (e.cancelled && !e.recurrence_id.valid()) return;  // also whole cancelled series

  const int64_t start = e.start.localSeconds();
  int64_t end = start + (e.start.date_only ? kSecondsPerDay : 0);
  if (e.end.valid()) {
    end = e.end.localSeconds();
  } else if (e.duration >= 0) {
    end = start + e.duration;
  }
  const bool overlaps = start <= to_local_ && end >= from_local_;
  bool keep = false;
  if (recurring) {
    keep =
        start <= to_local_ && (!e.rule.until.valid() || e.rule.until.localSeconds() >= from_local_);
    // A series with COUNT may have ended long ago ("10 times from 2019");
    // such old series would only fill the kMaxEvents places (CR2 M3).
    if (keep && e.rule.count > 0) keep = countReachesWindow(e, end - start, from_local_, to_local_);
  } else if (e.recurrence_id.valid()) {
    const int64_t original = e.recurrence_id.localSeconds();
    keep = overlaps || (original >= from_local_ && original <= to_local_);
  } else {
    keep = overlaps;
  }
  if (!keep) return;
  if (events_.size() >= kMaxEvents) {
    truncated_ = true;
    return;
  }
  events_.push_back(std::move(e));
}

}  // namespace widgets::calendar
