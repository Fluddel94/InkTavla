// Calendar widget: dates, recurrence rules, time zones, the streaming iCal
// reader, the expansion into occurrences, the agenda and the widget (the
// pictures of the column are in test_briefing). All calendar files are
// hand-written;
// the expected dates of the recurrence rules were cross-checked with
// python-dateutil.

#include <unity.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/time.h"
#include "ui/bitmap.h"
#include "ui/emoji.h"
#include "widgets/app.h"
#include "widgets/calendar/agenda.h"
#include "widgets/calendar/column_view.h"
#include "widgets/calendar/config.h"
#include "widgets/calendar/datetime.h"
#include "widgets/calendar/parser.h"
#include "widgets/calendar/rrule.h"
#include "widgets/calendar/timezone.h"
#include "widgets/calendar/widget.h"

namespace {

namespace cal = widgets::calendar;

// 2026-10-20 07:42 CEST (Tuesday), the time of the overall UI design.
constexpr int64_t kNow = 1792474920;
constexpr int64_t kDay = 86400;

std::string readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string dayText(int64_t day) {
  int y, m, d;
  cal::civilFromDays(day, y, m, d);
  char text[16];
  std::snprintf(text, sizeof(text), "%04d%02d%02d", y, m, d);
  return text;
}

std::string localText(int64_t local_s) {
  const cal::DateTime dt = cal::fromLocalSeconds(local_s);
  char text[32];
  std::snprintf(text, sizeof(text), "%04d%02d%02dT%02d%02d", dt.year, dt.month, dt.day, dt.hour,
                dt.minute);
  return text;
}

// Dates of a rule from a start date, at most n.
std::string occurrences(const char* rule_text, const char* start_text, size_t n,
                        const char* from = nullptr) {
  cal::Rule rule;
  TEST_ASSERT_TRUE_MESSAGE(cal::parseRule(rule_text, cal::kZoneFloating, rule), rule_text);
  cal::DateTime start;
  TEST_ASSERT_TRUE(cal::parseDateTime(start_text, false, cal::kZoneFloating, start));
  cal::DateTime first = start;
  if (from != nullptr) cal::parseDateTime(from, false, cal::kZoneFloating, first);
  std::string out;
  size_t count = 0;
  cal::expandRule(rule, start, first.days(), first.days() + 20000, [&](int64_t local) {
    if (local < first.localSeconds()) return true;
    if (!out.empty()) out += ' ';
    out += dayText(cal::floorDiv(local, kDay));
    return ++count < n;
  });
  return out;
}

cal::Parser parse(const std::string& text, int64_t from = kNow - 2 * kDay,
                  int64_t to = kNow + 10 * kDay) {
  cal::Parser parser(from, to);
  parser.feed(text);
  parser.finish();
  return parser;
}

// Occurrences of a file in the days around kNow as "summary@local start".
std::string expanded(const cal::Parser& parser, int64_t first_day, int64_t last_day) {
  std::string out;
  for (const cal::Occurrence& o : cal::expandEvents(parser, first_day, last_day)) {
    if (!out.empty()) out += " | ";
    out += o.summary + "@";
    out += o.all_day ? dayText(o.first_day) + "+" + std::to_string(o.last_day - o.first_day)
                     : localText(cal::deviceLocalSeconds(o.start));
  }
  return out;
}

const char kBerlin[] =
    "BEGIN:VTIMEZONE\r\nTZID:Europe/Berlin\r\n"
    "BEGIN:DAYLIGHT\r\nTZOFFSETFROM:+0100\r\nTZOFFSETTO:+0200\r\nDTSTART:19700329T020000\r\n"
    "RRULE:FREQ=YEARLY;BYMONTH=3;BYDAY=-1SU\r\nEND:DAYLIGHT\r\n"
    "BEGIN:STANDARD\r\nTZOFFSETFROM:+0200\r\nTZOFFSETTO:+0100\r\nDTSTART:19701025T030000\r\n"
    "RRULE:FREQ=YEARLY;BYMONTH=10;BYDAY=-1SU\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n";

const char kNewYork[] =
    "BEGIN:VTIMEZONE\r\nTZID:America/New_York\r\n"
    "BEGIN:DAYLIGHT\r\nTZOFFSETFROM:-0500\r\nTZOFFSETTO:-0400\r\nDTSTART:20070311T020000\r\n"
    "RRULE:FREQ=YEARLY;BYMONTH=3;BYDAY=2SU\r\nEND:DAYLIGHT\r\n"
    "BEGIN:STANDARD\r\nTZOFFSETFROM:-0400\r\nTZOFFSETTO:-0500\r\nDTSTART:20071104T020000\r\n"
    "RRULE:FREQ=YEARLY;BYMONTH=11;BYDAY=1SU\r\nEND:STANDARD\r\nEND:VTIMEZONE\r\n";

std::string calendar(const std::string& body) {
  return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n" + body + "END:VCALENDAR\r\n";
}

const cal::Zone& berlin(const cal::Parser& parser) {
  const cal::Zone* zone = parser.zone(0);
  TEST_ASSERT_NOT_NULL(zone);
  return *zone;
}

int64_t utc(int y, int m, int d, int h, int mi) {
  return cal::daysFromCivil(y, m, d) * kDay + h * 3600 + mi * 60;
}

std::vector<cal::AgendaDay> weekAgenda(int64_t now, ui::Language language = ui::Language::kGerman,
                                       const char* fixture = "week.ics",
                                       const cal::DisplayConfig& display = {}) {
  const cal::Parser parser = parse(readFile(std::string("test/fixtures/calendar/") + fixture),
                                   now - kDay, now + 15 * kDay);
  TEST_ASSERT_TRUE(parser.valid());
  const int64_t today = cal::localDay(now);
  return cal::buildAgenda(cal::expandEvents(parser, today, today + 14), now, language, display);
}

cal::Config parseCalendarConfig(const char* json, core::ConfigIssues& issues) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return cal::parseConfig(doc.as<JsonObjectConst>(), issues);
}

// "kind:time title suffix note" of the entries of one agenda day.
std::string entries(const cal::AgendaDay& day) {
  std::string out;
  for (const cal::Entry& e : day.entries) {
    if (!out.empty()) out += " | ";
    if (e.kind == cal::Entry::Kind::kAllDay) out += "[" + e.title + "] " + e.suffix;
    if (e.kind == cal::Entry::Kind::kNone) out += "(" + e.title + ")";
    if (e.kind == cal::Entry::Kind::kTimed) {
      out += e.time + " " + (e.next ? "*" : "") + e.title;
      if (!e.note.empty()) out += " (" + e.note + ")";
    }
  }
  return out;
}

// Calendar source with a saved file, fails on demand.
class FixtureSource : public cal::Source {
 public:
  bool configured() const override { return true; }
  bool fetchCalendar(cal::Parser& parser) override {
    requests++;
    if (fail) return false;
    parser.feed(text);
    return true;
  }
  std::string text = readFile("test/fixtures/calendar/week.ics");
  bool fail = false;
  int requests = 0;
};

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

// --- Dates --------------------------------------------------------------------

void test_civil_dates() {
  TEST_ASSERT_EQUAL_INT64(0, cal::daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT64(20746, cal::daysFromCivil(2026, 10, 20));
  TEST_ASSERT_EQUAL_INT64(-1, cal::daysFromCivil(1969, 12, 31));
  int y, m, d;
  cal::civilFromDays(20746, y, m, d);
  TEST_ASSERT_EQUAL_INT(2026, y);
  TEST_ASSERT_EQUAL_INT(10, m);
  TEST_ASSERT_EQUAL_INT(20, d);
  TEST_ASSERT_EQUAL_INT(2, cal::weekdayFromDays(20746));  // Tuesday
  TEST_ASSERT_EQUAL_INT(4, cal::weekdayFromDays(0));      // Thursday
  TEST_ASSERT_EQUAL_INT(43, cal::isoWeek(20746));
  TEST_ASSERT_EQUAL_INT(53, cal::isoWeek(cal::daysFromCivil(2027, 1, 1)));
  TEST_ASSERT_EQUAL_INT(1, cal::isoWeek(cal::daysFromCivil(2027, 1, 4)));
  TEST_ASSERT_EQUAL_INT(29, cal::daysInMonth(2028, 2));
  TEST_ASSERT_EQUAL_INT(28, cal::daysInMonth(2100, 2));
}

void test_parse_values() {
  cal::DateTime dt;
  TEST_ASSERT_TRUE(cal::parseDateTime("20261020T083000Z", false, 3, dt));
  TEST_ASSERT_EQUAL_UINT8(cal::kZoneUtc, dt.zone);
  TEST_ASSERT_TRUE(cal::parseDateTime("20261020T083000", false, 3, dt));
  TEST_ASSERT_EQUAL_UINT8(3, dt.zone);
  TEST_ASSERT_EQUAL_INT(8, dt.hour);
  TEST_ASSERT_TRUE(cal::parseDateTime("20261020", false, 3, dt));
  TEST_ASSERT_TRUE(dt.date_only);
  TEST_ASSERT_FALSE(cal::parseDateTime("20261020T0830", false, 3, dt));
  TEST_ASSERT_FALSE(cal::parseDateTime("20260230", false, 3, dt));
  TEST_ASSERT_FALSE(cal::parseDateTime("20261020T083000", true, 3, dt));

  int64_t s = 0;
  TEST_ASSERT_TRUE(cal::parseDuration("PT1H30M", s));
  TEST_ASSERT_EQUAL_INT64(5400, s);
  TEST_ASSERT_TRUE(cal::parseDuration("P2W", s));
  TEST_ASSERT_EQUAL_INT64(14 * kDay, s);
  TEST_ASSERT_TRUE(cal::parseDuration("-P0DT0H10M0S", s));
  TEST_ASSERT_EQUAL_INT64(-600, s);
  TEST_ASSERT_FALSE(cal::parseDuration("P", s));
  TEST_ASSERT_FALSE(cal::parseDuration("PT5D", s));

  int offset = 0;
  TEST_ASSERT_TRUE(cal::parseUtcOffset("+0200", offset));
  TEST_ASSERT_EQUAL_INT(7200, offset);
  TEST_ASSERT_TRUE(cal::parseUtcOffset("-0430", offset));
  TEST_ASSERT_EQUAL_INT(-16200, offset);
  TEST_ASSERT_FALSE(cal::parseUtcOffset("0200", offset));
}

// --- Recurrence rules -------------------------------------------------------------

void test_rrule_monthly() {
  TEST_ASSERT_EQUAL_STRING("20260113 20260210 20260310 20260414",
                           occurrences("FREQ=MONTHLY;BYDAY=2TU", "20260113T100000", 4).c_str());
  TEST_ASSERT_EQUAL_STRING("20260130 20260227 20260327",
                           occurrences("FREQ=MONTHLY;BYDAY=-1FR", "20260130T100000", 3).c_str());
  // Months without a 31st are skipped.
  TEST_ASSERT_EQUAL_STRING("20260131 20260331 20260531",
                           occurrences("FREQ=MONTHLY;BYMONTHDAY=31", "20260131T100000", 3).c_str());
  // Last working day of the month.
  TEST_ASSERT_EQUAL_STRING(
      "20260130 20260227 20260331",
      occurrences("FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1", "20260130T100000", 3).c_str());
  // Friday the 13th.
  TEST_ASSERT_EQUAL_STRING(
      "20260213 20260313 20261113",
      occurrences("FREQ=MONTHLY;BYDAY=FR;BYMONTHDAY=13", "20260213T100000", 3).c_str());
}

void test_rrule_weekly_daily_yearly() {
  TEST_ASSERT_EQUAL_STRING(
      "20261005 20261007 20261019 20261021 20261102",
      occurrences("FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE;COUNT=5", "20261005T090000", 10).c_str());
  // The week start matters for intervals (examples from RFC 5545).
  TEST_ASSERT_EQUAL_STRING(
      "19970805 19970817 19970819 19970831",
      occurrences("FREQ=WEEKLY;INTERVAL=2;WKST=SU;BYDAY=TU,SU;COUNT=4", "19970805T090000", 10)
          .c_str());
  TEST_ASSERT_EQUAL_STRING(
      "19970805 19970810 19970819 19970824",
      occurrences("FREQ=WEEKLY;INTERVAL=2;WKST=MO;BYDAY=TU,SU;COUNT=4", "19970805T090000", 10)
          .c_str());
  TEST_ASSERT_EQUAL_STRING(
      "20260923 20261021 20261118",
      occurrences("FREQ=WEEKLY;INTERVAL=4;BYDAY=WE", "20260923T000000", 3).c_str());
  TEST_ASSERT_EQUAL_STRING("20261030 20261031 20261101",
                           occurrences("FREQ=DAILY;COUNT=3", "20261030T120000", 10).c_str());
  TEST_ASSERT_EQUAL_STRING("20240229 20280229",
                           occurrences("FREQ=YEARLY", "20240229T000000", 2).c_str());
  TEST_ASSERT_EQUAL_STRING("19970519 19980518 19990517",
                           occurrences("FREQ=YEARLY;BYDAY=20MO", "19970519T090000", 3).c_str());
  // Daylight saving rule of a VTIMEZONE, jumping to 2026.
  TEST_ASSERT_EQUAL_STRING("20260329", occurrences("FREQ=YEARLY;BYMONTH=3;BYDAY=-1SU",
                                                   "19700329T020000", 1, "20260101T000000")
                                           .c_str());
}

void test_rrule_limits() {
  // Unsupported parts: only the first occurrence.
  TEST_ASSERT_EQUAL_STRING("20261020",
                           occurrences("FREQ=DAILY;BYHOUR=8,12", "20261020T080000", 5).c_str());
  TEST_ASSERT_EQUAL_STRING("20261020", occurrences("FREQ=HOURLY", "20261020T080000", 5).c_str());
  cal::Rule rule;
  TEST_ASSERT_FALSE(cal::parseRule("INTERVAL=2", cal::kZoneFloating, rule));
  TEST_ASSERT_FALSE(cal::parseRule("FREQ=WEEKLY;BYDAY=XX", cal::kZoneFloating, rule));
  TEST_ASSERT_TRUE(cal::parseRule("FREQ=WEEKLY;X-NAME=1", cal::kZoneFloating, rule));
  TEST_ASSERT_TRUE(cal::parseRule("FREQ=WEEKLY;UNTIL=20271230T180000Z", 5, rule));
  TEST_ASSERT_EQUAL_UINT8(cal::kZoneUtc, rule.until.zone);

  // An old daily series is not walked from its start: the first visit after
  // the start is close to the requested window.
  TEST_ASSERT_TRUE(cal::parseRule("FREQ=DAILY", cal::kZoneFloating, rule));
  cal::DateTime start;
  cal::parseDateTime("20100101T070000", false, cal::kZoneFloating, start);
  int visits = 0;
  int64_t second = 0;
  cal::expandRule(rule, start, 20746, 20750, [&](int64_t local) {
    if (++visits == 2) second = cal::floorDiv(local, kDay);
    return true;
  });
  TEST_ASSERT_TRUE(second >= 20746 - 2 && second <= 20746);
  TEST_ASSERT_TRUE(visits < 10);
}

// --- Time zones ----------------------------------------------------------------------

void test_time_zones() {
  const cal::Parser parser = parse(calendar(kBerlin));
  const cal::Zone& zone = berlin(parser);
  TEST_ASSERT_EQUAL_INT64(utc(2026, 7, 1, 10, 0), cal::zoneToUtc(zone, utc(2026, 7, 1, 12, 0)));
  TEST_ASSERT_EQUAL_INT64(utc(2026, 1, 15, 11, 0), cal::zoneToUtc(zone, utc(2026, 1, 15, 12, 0)));
  // 02:30 exists twice on 2026-10-25: the first (summer time).
  TEST_ASSERT_EQUAL_INT64(utc(2026, 10, 25, 0, 30), cal::zoneToUtc(zone, utc(2026, 10, 25, 2, 30)));
  TEST_ASSERT_EQUAL_INT64(utc(2026, 10, 25, 2, 0), cal::zoneToUtc(zone, utc(2026, 10, 25, 3, 0)));
  // 02:30 does not exist on 2026-03-29: the offset before the change.
  TEST_ASSERT_EQUAL_INT64(utc(2026, 3, 29, 1, 30), cal::zoneToUtc(zone, utc(2026, 3, 29, 2, 30)));
  TEST_ASSERT_EQUAL_INT(7200, cal::zoneOffsetAt(zone, utc(2026, 10, 25, 0, 59)));
  TEST_ASSERT_EQUAL_INT(3600, cal::zoneOffsetAt(zone, utc(2026, 10, 25, 1, 0)));
  // Before the first onset: the offset before it.
  TEST_ASSERT_EQUAL_INT(3600, cal::zoneOffsetAt(zone, utc(1960, 6, 1, 0, 0)));

  // The device zone (TZ of the process).
  TEST_ASSERT_EQUAL_INT64(utc(2026, 10, 20, 5, 42), cal::deviceToUtc(utc(2026, 10, 20, 7, 42)));
  TEST_ASSERT_EQUAL_INT64(utc(2026, 10, 20, 7, 42), cal::deviceLocalSeconds(kNow));
}

// --- Reading files -------------------------------------------------------------------

void test_parser_lines() {
  // CRLF and LF, folding (also inside a UTF-8 character), escapes, lower-case
  // names, quoted parameters, an alarm with its own SUMMARY, unknown
  // components and long values that are skipped.
  const std::string text =
      "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n" + std::string(kBerlin) +
      "BEGIN:VEVENT\r\nUID:a@example.invalid\r\n"
      "SUMMARY:Gr\xC3\r\n \xBC\xC3\x9F"
      "e\\, Treffen\\; Raum \\\\1\\nzweite Zeile\r\n"
      "DTSTART;TZID=\"Europe/Berlin\":20261021T090000\r\n"
      "dtend;tzid=Europe/Berlin:20261021T100000\r\n"
      "DESCRIPTION:" +
      std::string(5000, 'x') +
      "\r\n"
      "ATTACH;FMTTYPE=text/plain:https://example.invalid/" +
      std::string(3000, 'y') +
      "\r\n"
      "BEGIN:VALARM\r\nSUMMARY:Alarm\r\nTRIGGER:-PT10M\r\nEND:VALARM\r\n"
      "END:VEVENT\n"
      "BEGIN:VTODO\nSUMMARY:Keine Aufgabe\nDTSTART:20261021T090000\nEND:VTODO\n"
      "BEGIN:VEVENT\nUID:b\nSUMMARY:Ohne Zeitangabe\nEND:VEVENT\n"
      "END:VCALENDAR";  // no final line break
  // Fed in small pieces, as from the network.
  cal::Parser parser(kNow - 2 * kDay, kNow + 10 * kDay);
  for (size_t i = 0; i < text.size(); i += 7) parser.feed(text.substr(i, 7));
  parser.finish();
  TEST_ASSERT_TRUE(parser.valid());
  TEST_ASSERT_EQUAL_size_t(2, parser.eventsRead());
  TEST_ASSERT_EQUAL_size_t(1, parser.events().size());
  const cal::Event& e = parser.events()[0];
  TEST_ASSERT_EQUAL_STRING("Grüße, Treffen; Raum \\1 zweite Zeile", e.summary.c_str());
  TEST_ASSERT_EQUAL_UINT64(cal::uidHash("a@example.invalid"), e.uid);
  TEST_ASSERT_EQUAL_INT(9, e.start.hour);
  TEST_ASSERT_EQUAL_UINT8(0, e.start.zone);
  TEST_ASSERT_TRUE(e.end.valid());
  TEST_ASSERT_EQUAL_STRING("Grüße, Treffen; Raum \\1 zweite Zeile@20261021T0900",
                           expanded(parser, 20746, 20752).c_str());

  // An incomplete file is not valid.
  const cal::Parser cut = parse(text.substr(0, text.size() / 2));
  TEST_ASSERT_FALSE(cut.valid());

  // Long titles are cut without splitting a character.
  const std::string long_title(cal::kMaxSummaryBytes - 1, 'a');
  const cal::Parser titled = parse(calendar("BEGIN:VEVENT\r\nSUMMARY:" + long_title +
                                            "\xC3\xA4\xC3\xA4\r\nDTSTART:20261021T090000Z\r\n"
                                            "END:VEVENT\r\n"));
  TEST_ASSERT_EQUAL_size_t(cal::kMaxSummaryBytes - 1, titled.events()[0].summary.size());
}

void test_parser_window() {
  // week.ics: old single events and ended series are dropped while reading,
  // cancelled events are never kept.
  const cal::Parser parser = parse(readFile("test/fixtures/calendar/week.ics"));
  TEST_ASSERT_TRUE(parser.valid());
  TEST_ASSERT_EQUAL_size_t(10, parser.eventsRead());
  TEST_ASSERT_EQUAL_size_t(7, parser.events().size());
  for (const cal::Event& e : parser.events()) {
    TEST_ASSERT_TRUE(e.summary.find("Alte") == std::string::npos);
    TEST_ASSERT_TRUE(e.summary.find("Abgesagt") == std::string::npos);
    // The alarm's SUMMARY does not replace the event's.
    TEST_ASSERT_TRUE(e.summary.find("Erinnerung") == std::string::npos);
  }

  // More events than kMaxEvents in the window: the rest is dropped.
  std::string body;
  for (size_t i = 0; i < cal::kMaxEvents + 5; i++) {
    body += "BEGIN:VEVENT\r\nSUMMARY:E" + std::to_string(i) +
            "\r\nDTSTART:20261021T090000Z\r\nEND:VEVENT\r\n";
  }
  const cal::Parser many = parse(calendar(body));
  TEST_ASSERT_TRUE(many.truncated());
  TEST_ASSERT_EQUAL_size_t(cal::kMaxEvents, many.events().size());
}

// --- Occurrences ----------------------------------------------------------------------

void test_expand_week() {
  const cal::Parser parser = parse(readFile("test/fixtures/calendar/week.ics"));
  TEST_ASSERT_EQUAL_STRING(
      "Team-Standup@20261020T0830 | Mittagessen mit Jonas@20261020T1230 | "
      "Zahnarzt@20261020T1715 | Papiertonne@20261021+0 | Kino mit Lena@20261021T1830 | "
      "Chorprobe@20261022T1900 | Geburtstag Oma@20261024+0 | Team-Standup@20261027T0830",
      expanded(parser, 20746, 20753).c_str());
  // The weekly rehearsal keeps its local time across the end of summer time
  // (2026-10-25): 17:00 UTC before, 18:00 UTC after.
  const auto list = cal::expandEvents(parser, 20748, 20755);
  int found = 0;
  for (const cal::Occurrence& o : list) {
    if (o.summary != "Chorprobe") continue;
    TEST_ASSERT_EQUAL_INT64(found == 0 ? utc(2026, 10, 22, 17, 0) : utc(2026, 10, 29, 18, 0),
                            o.start);
    TEST_ASSERT_EQUAL_INT64(o.start + 2 * 3600, o.end);
    found++;
  }
  TEST_ASSERT_EQUAL_INT(2, found);
}

void test_expand_exceptions() {
  // A weekly series with an excluded date, a moved occurrence (it may come
  // before its series in the file), a cancelled occurrence and an UNTIL.
  const std::string text = calendar(
      std::string(kBerlin) +
      "BEGIN:VEVENT\r\nUID:s1\r\nSUMMARY:Verschoben\r\n"
      "RECURRENCE-ID;TZID=Europe/Berlin:20261021T100000\r\n"
      "DTSTART;TZID=Europe/Berlin:20261023T140000\r\nDTEND;TZID=Europe/Berlin:20261023T150000\r\n"
      "END:VEVENT\r\n"
      "BEGIN:VEVENT\r\nUID:s1\r\nSUMMARY:Serie\r\n"
      "DTSTART;TZID=Europe/Berlin:20261007T100000\r\nDURATION:PT1H\r\n"
      "RRULE:FREQ=WEEKLY;UNTIL=20261111T090000Z\r\n"
      "EXDATE;TZID=Europe/Berlin:20261014T100000,20261028T100000\r\n"
      "END:VEVENT\r\n"
      "BEGIN:VEVENT\r\nUID:s1\r\nSUMMARY:Serie\r\nSTATUS:CANCELLED\r\n"
      "RECURRENCE-ID;TZID=Europe/Berlin:20261104T100000\r\n"
      "DTSTART;TZID=Europe/Berlin:20261104T100000\r\nEND:VEVENT\r\n"
      // Same UID scheme, other series: not affected by s1's exceptions.
      "BEGIN:VEVENT\r\nUID:s2\r\nSUMMARY:Andere\r\nDTSTART;VALUE=DATE:20261021\r\n"
      "RRULE:FREQ=DAILY;COUNT=2\r\nEND:VEVENT\r\n");
  const cal::Parser parser = parse(text, utc(2026, 10, 10, 0, 0), utc(2026, 11, 20, 0, 0));
  TEST_ASSERT_EQUAL_STRING(
      "Serie@20261007T1000 | Andere@20261021+0 | Andere@20261022+0 | Verschoben@20261023T1400 | "
      "Serie@20261111T1000",
      expanded(parser, cal::daysFromCivil(2026, 10, 1), cal::daysFromCivil(2026, 11, 30)).c_str());
}

void test_expand_zones_and_lengths() {
  const std::string text = calendar(
      std::string(kNewYork) +
      // New York: 09:00 EDT = 14:00 CET (after 25.10.), 09:00 EST (after
      // 1.11.) = 15:00 CET.
      "BEGIN:VEVENT\r\nSUMMARY:New York\r\nDTSTART;TZID=America/New_York:20261030T090000\r\n"
      "RRULE:FREQ=DAILY;INTERVAL=3;COUNT=2\r\nEND:VEVENT\r\n"
      // Floating time and a TZID without VTIMEZONE: the device's zone.
      "BEGIN:VEVENT\r\nSUMMARY:Ohne Zone\r\nDTSTART:20261021T090000\r\nEND:VEVENT\r\n"
      "BEGIN:VEVENT\r\nSUMMARY:Unbekannte Zone\r\nDTSTART;TZID=Mars/Olympus:20261021T100000\r\n"
      "END:VEVENT\r\n"
      // Multi-day: all-day (three days) and timed over two nights.
      "BEGIN:VEVENT\r\nSUMMARY:Urlaub\r\nDTSTART;VALUE=DATE:20261022\r\n"
      "DTEND;VALUE=DATE:20261025\r\nEND:VEVENT\r\n"
      "BEGIN:VEVENT\r\nSUMMARY:Messe\r\nDTSTART:20261023T160000Z\r\n"
      "DTEND:20261025T110000Z\r\nEND:VEVENT\r\n");
  const cal::Parser parser = parse(text, utc(2026, 10, 19, 0, 0), utc(2026, 11, 10, 0, 0));
  TEST_ASSERT_EQUAL_STRING(
      "Ohne Zone@20261021T0900 | Unbekannte Zone@20261021T1000 | Urlaub@20261022+2 | "
      "Messe@20261023T1800 | New York@20261030T1400 | New York@20261102T1500",
      expanded(parser, cal::daysFromCivil(2026, 10, 19), cal::daysFromCivil(2026, 11, 9)).c_str());
  for (const cal::Occurrence& o : cal::expandEvents(parser, 20746, 20760)) {
    if (o.summary == "Messe") {
      TEST_ASSERT_EQUAL_INT64(cal::daysFromCivil(2026, 10, 23), o.first_day);
      TEST_ASSERT_EQUAL_INT64(cal::daysFromCivil(2026, 10, 25), o.last_day);
    }
  }
}

// --- Agenda ----------------------------------------------------------------------------

void test_agenda_morning() {
  const auto agenda = weekAgenda(kNow);
  TEST_ASSERT_EQUAL_size_t(4, agenda.size());
  TEST_ASSERT_EQUAL_STRING(
      "08:30 *Team-Standup (in 48 min) | 12:30 Mittagessen mit Jonas | 17:15 Zahnarzt",
      entries(agenda[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("[Papiertonne] ganztägig | 18:30 Kino mit Lena",
                           entries(agenda[1]).c_str());
  TEST_ASSERT_EQUAL_STRING("19:00 Chorprobe", entries(agenda[2]).c_str());
  TEST_ASSERT_EQUAL_STRING("[Geburtstag Oma] ganztägig", entries(agenda[3]).c_str());
  TEST_ASSERT_EQUAL_INT64(cal::daysFromCivil(2026, 10, 24), agenda[3].day);
  // End times (shown on step L, 2026-11-16); none for all-day events.
  TEST_ASSERT_EQUAL_STRING("08:45", agenda[0].entries[0].end.c_str());
  TEST_ASSERT_EQUAL_STRING("18:00", agenda[0].entries[2].end.c_str());
  TEST_ASSERT_EQUAL_STRING("", agenda[1].entries[0].end.c_str());
  TEST_ASSERT_FALSE(agenda[1].entries[0].continues);
}

void test_agenda_during_the_day() {
  // 12:50: the stand-up is over (it stays until midnight), lunch is running,
  // the dentist is next.
  auto agenda = weekAgenda(kNow + 308 * 60);
  TEST_ASSERT_EQUAL_STRING(
      "08:30 Team-Standup | 12:30 Mittagessen mit Jonas (läuft) | 17:15 *Zahnarzt (in 4 h)",
      entries(agenda[0]).c_str());
  TEST_ASSERT_TRUE(agenda[0].entries[0].past);
  TEST_ASSERT_FALSE(agenda[0].entries[1].past);
  // 18:30: everything of today is over but still shown, without notes.
  agenda = weekAgenda(kNow + 648 * 60);
  TEST_ASSERT_EQUAL_STRING("08:30 Team-Standup | 12:30 Mittagessen mit Jonas | 17:15 Zahnarzt",
                           entries(agenda[0]).c_str());
  // After midnight they are gone; Wednesday is today.
  agenda = weekAgenda(kNow + 17 * 3600);
  TEST_ASSERT_EQUAL_STRING("[Papiertonne] ganztägig | 18:30 *Kino mit Lena (in 18 h)",
                           entries(agenda[0]).c_str());
  // English.
  agenda = weekAgenda(kNow, ui::Language::kEnglish);
  TEST_ASSERT_EQUAL_STRING("[Papiertonne] all day | 18:30 Kino mit Lena",
                           entries(agenda[1]).c_str());
}

void test_agenda_multi_day() {
  const std::string text = calendar(
      "BEGIN:VEVENT\r\nSUMMARY:Urlaub\r\nDTSTART;VALUE=DATE:20261019\r\n"
      "DTEND;VALUE=DATE:20261031\r\nEND:VEVENT\r\n"
      "BEGIN:VEVENT\r\nSUMMARY:Messe\r\nDTSTART:20261020T160000Z\r\n"
      "DTEND:20261022T100000Z\r\nEND:VEVENT\r\n");
  const cal::Parser parser = parse(text);
  const int64_t today = cal::localDay(kNow);
  const auto agenda =
      cal::buildAgenda(cal::expandEvents(parser, today, today + 7), kNow, ui::Language::kGerman);
  TEST_ASSERT_EQUAL_STRING("[Urlaub] bis 30.10. | 18:00 *Messe (in 10 h)",
                           entries(agenda[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("[Urlaub] bis 30.10. | [Messe] bis Do", entries(agenda[1]).c_str());
  TEST_ASSERT_EQUAL_STRING("[Urlaub] bis 30.10. | [Messe] bis 12:00", entries(agenda[2]).c_str());
  TEST_ASSERT_EQUAL_STRING("[Urlaub] bis Fr", entries(agenda[6]).c_str());
}

// --- Widget ------------------------------------------------------------------------

void test_config() {
  core::ConfigIssues issues;
  JsonDocument doc;
  deserializeJson(doc, R"({"url": "https://example.invalid/secret.ics", "colour": 1})");
  cal::parseConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(2, issues.size());
  for (const core::ConfigIssue& i : issues) {
    // The value is never repeated in a message.
    TEST_ASSERT_TRUE(i.message.find("secret.ics") == std::string::npos);
  }
  TEST_ASSERT_EQUAL_STRING("widgets.calendar.url", issues[0].path.c_str());
  TEST_ASSERT_TRUE(issues[0].message.find("inktavla_config.py calendar-url") != std::string::npos);
  TEST_ASSERT_EQUAL_STRING("widgets.calendar.colour", issues[1].path.c_str());

  // Through the validation of the whole file; an empty section is fine.
  issues.clear();
  deserializeJson(doc, R"({"widgets": {"calendar": {}}})");
  widgets::validateConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
}

void test_config_settings() {
  // Defaults: the column as before the settings.
  core::ConfigIssues issues;
  cal::Config c = parseCalendarConfig("{}", issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_INT(7, c.display.days);
  TEST_ASSERT_TRUE(c.display.all_day && c.display.past && c.display.wrap);
  TEST_ASSERT_FALSE(c.display.more || c.display.location);
  TEST_ASSERT_TRUE(c.display.exclude.empty());
  TEST_ASSERT_EQUAL_INT(15, c.calendar.refresh_minutes);

  c = parseCalendarConfig(R"({"days": 3, "all_day": false, "past": false, "more": true,
      "location": true, "wrap": false, "exclude": [" Geburtstag ", "müll", ""],
      "refresh_minutes": 60})",
                          issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_INT(3, c.display.days);
  TEST_ASSERT_FALSE(c.display.all_day || c.display.past || c.display.wrap);
  TEST_ASSERT_TRUE(c.display.more && c.display.location);
  TEST_ASSERT_EQUAL_size_t(2, c.display.exclude.size());
  TEST_ASSERT_EQUAL_INT(60, c.calendar.refresh_minutes);
  TEST_ASSERT_TRUE(cal::excluded(c.display, "Geburtstag Oma"));
  TEST_ASSERT_TRUE(cal::excluded(c.display, "MÜLLABFUHR"));
  TEST_ASSERT_FALSE(cal::excluded(c.display, "Zahnarzt"));

  // Invalid values: a message each, the default instead.
  c = parseCalendarConfig(R"({"days": 0, "more": "ja", "refresh_minutes": 2,
      "exclude": "Geburtstag"})",
                          issues);
  TEST_ASSERT_EQUAL_size_t(4, issues.size());
  TEST_ASSERT_EQUAL_STRING("widgets.calendar.days", issues[0].path.c_str());
  TEST_ASSERT_EQUAL_STRING("must be 1, 2, 3 or 7", issues[0].message.c_str());
  TEST_ASSERT_EQUAL_STRING("widgets.calendar.more", issues[1].path.c_str());
  TEST_ASSERT_EQUAL_STRING("widgets.calendar.exclude", issues[2].path.c_str());
  TEST_ASSERT_EQUAL_STRING("widgets.calendar.refresh_minutes", issues[3].path.c_str());
  TEST_ASSERT_EQUAL_STRING("must be between 5 and 360", issues[3].message.c_str());
  TEST_ASSERT_EQUAL_INT(7, c.display.days);
  TEST_ASSERT_FALSE(c.display.more);
  TEST_ASSERT_TRUE(c.display.exclude.empty());
  TEST_ASSERT_EQUAL_INT(15, c.calendar.refresh_minutes);

  // Too many or too long words, entries that are no words.
  issues.clear();
  c = parseCalendarConfig(R"({"exclude": ["a","b","c","d","e","f","g","h","i","j","k",
      "abcdefghijklmnopqrstuvwxyzabcdefghij", 3]})",
                          issues);
  TEST_ASSERT_EQUAL_size_t(10, c.display.exclude.size());
  TEST_ASSERT_EQUAL_size_t(1, issues.size());
  TEST_ASSERT_TRUE(issues[0].message.find("at most 10") != std::string::npos);
  issues.clear();
  c = parseCalendarConfig(R"({"exclude": ["abcdefghijklmnopqrstuvwxyzabcdefghij", 3]})", issues);
  TEST_ASSERT_EQUAL_size_t(2, issues.size());
  TEST_ASSERT_TRUE(c.display.exclude.empty());
}

// UTF-8 of the codepoint of an emoji icon (private use area, three bytes).
std::string icon(ui::EmojiIcon icon) {
  const uint32_t cp = ui::emojiCodepoint(icon);
  std::string out;
  out += static_cast<char>(0xE0 | (cp >> 12));
  out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
  out += static_cast<char>(0x80 | (cp & 0x3F));
  return out;
}

// Emoji in titles and locations: icons for the known ones, nothing for the
// others (no "?" from the fonts), no double spaces.
void test_emoji_in_titles() {
  const std::vector<cal::AgendaDay> agenda = weekAgenda(kNow, ui::Language::kGerman, "emoji.ics");
  TEST_ASSERT_FALSE(agenda.empty());
  const std::vector<cal::Entry>& e = agenda[0].entries;
  TEST_ASSERT_EQUAL(7, e.size());
  TEST_ASSERT_EQUAL_STRING(("Date night " + icon(ui::EmojiIcon::kHeart)).c_str(),
                           e[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING(
      ("Geburtstag Oma " + icon(ui::EmojiIcon::kCake) + icon(ui::EmojiIcon::kParty)).c_str(),
      e[1].title.c_str());
  TEST_ASSERT_EQUAL_STRING(
      ("Caf\xC3\xA9 " + icon(ui::EmojiIcon::kCoffee) + " am Markt " + icon(ui::EmojiIcon::kHouse))
          .c_str(),
      e[1].location.c_str());
  TEST_ASSERT_EQUAL_STRING(("Flug nach Rom " + icon(ui::EmojiIcon::kAirplane)).c_str(),
                           e[2].title.c_str());
  // The skin tone is dropped, the thumb stays.
  TEST_ASSERT_EQUAL_STRING(
      ("Zahnarzt " + icon(ui::EmojiIcon::kTooth) + " " + icon(ui::EmojiIcon::kThumbsUp)).c_str(),
      e[3].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Unbekannt", e[4].title.c_str());
  TEST_ASSERT_EQUAL_STRING(
      ("Lecker essen " + icon(ui::EmojiIcon::kDining) + icon(ui::EmojiIcon::kDining) +
       " mit Freunden " + icon(ui::EmojiIcon::kLaughingFace))
          .c_str(),
      e[5].title.c_str());
  // Christmas tree and evergreen tree; the deciduous tree is unknown.
  TEST_ASSERT_EQUAL_STRING(
      ("Weihnachtsmarkt " + icon(ui::EmojiIcon::kTree) + " " + icon(ui::EmojiIcon::kTree)).c_str(),
      e[6].title.c_str());
}

void test_agenda_settings() {
  cal::DisplayConfig d;
  // Two days: today and tomorrow only.
  d.days = 2;
  auto agenda = weekAgenda(kNow, ui::Language::kGerman, "week.ics", d);
  TEST_ASSERT_EQUAL_size_t(2, agenda.size());
  // Three days: up to Thursday (choir), without the birthday on Saturday.
  d.days = 3;
  agenda = weekAgenda(kNow, ui::Language::kGerman, "week.ics", d);
  TEST_ASSERT_EQUAL_size_t(3, agenda.size());
  TEST_ASSERT_EQUAL_STRING("19:00 Chorprobe", entries(agenda[2]).c_str());
  // Only 1, 2, 3 and 7 are allowed.
  core::ConfigIssues day_issues;
  TEST_ASSERT_EQUAL_INT(7, parseCalendarConfig(R"({"days": 5})", day_issues).display.days);
  TEST_ASSERT_EQUAL_size_t(1, day_issues.size());
  TEST_ASSERT_EQUAL_STRING("must be 1, 2, 3 or 7", day_issues[0].message.c_str());

  // Without all-day events (the paper bin, the birthday disappear).
  d = {};
  d.all_day = false;
  agenda = weekAgenda(kNow, ui::Language::kGerman, "week.ics", d);
  TEST_ASSERT_EQUAL_STRING("18:30 Kino mit Lena", entries(agenda[1]).c_str());
  TEST_ASSERT_EQUAL_size_t(3, agenda.size());

  // Without past events: at 12:50 the stand-up is gone; in the evening
  // "Keine Termine".
  d = {};
  d.past = false;
  agenda = weekAgenda(kNow + 308 * 60, ui::Language::kGerman, "week.ics", d);
  TEST_ASSERT_EQUAL_STRING("12:30 Mittagessen mit Jonas (läuft) | 17:15 *Zahnarzt (in 4 h)",
                           entries(agenda[0]).c_str());
  agenda = weekAgenda(kNow + 648 * 60, ui::Language::kGerman, "week.ics", d);
  TEST_ASSERT_EQUAL_STRING("(Keine Termine)", entries(agenda[0]).c_str());

  // Words left out, ignoring the case.
  d = {};
  d.exclude = {"KINO", "TEAM"};
  agenda = weekAgenda(kNow, ui::Language::kGerman, "week.ics", d);
  TEST_ASSERT_EQUAL_STRING("12:30 *Mittagessen mit Jonas (in 5 h) | 17:15 Zahnarzt",
                           entries(agenda[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("[Papiertonne] ganztägig", entries(agenda[1]).c_str());

  // Places, in one line.
  agenda = weekAgenda(kNow);
  TEST_ASSERT_EQUAL_STRING("Besprechungsraum 2", agenda[0].entries[0].location.c_str());
  TEST_ASSERT_EQUAL_STRING("Praxis am Markt, Hauptstraße 4, 80331 München",
                           agenda[0].entries[2].location.c_str());
  TEST_ASSERT_EQUAL_STRING("", agenda[1].entries[0].location.c_str());
  TEST_ASSERT_EQUAL_STRING("Kino am Park, Parkstraße 1", agenda[1].entries[1].location.c_str());
}

void test_not_a_calendar() {
  const int64_t from = kNow - 86400, to = kNow + 86400 * 9;
  {
    cal::Parser p(from, to);
    p.feed("\xEF\xBB\xBF\r\n  begin:vcalendar\r\nEND:VCALENDAR\r\n");
    p.finish();
    TEST_ASSERT_FALSE(p.notCalendar());
    TEST_ASSERT_TRUE(p.valid());
  }
  {
    cal::Parser p(from, to);
    p.feed("BEGIN:VCAL");  // not decided yet
    TEST_ASSERT_FALSE(p.notCalendar());
  }
  {
    cal::Parser p(from, to);
    p.feed(readFile("test/fixtures/calendar/not_ical.html"));
    TEST_ASSERT_TRUE(p.notCalendar());
    TEST_ASSERT_FALSE(p.valid());
  }
  {
    cal::Parser p(from, to);
    p.feed("BEGIN:VEVENT\r\n");
    TEST_ASSERT_TRUE(p.notCalendar());
  }
  TEST_ASSERT_TRUE(cal::errorForStatus(404) == cal::FetchError::kAddress);
  TEST_ASSERT_TRUE(cal::errorForStatus(403) == cal::FetchError::kAddress);
  TEST_ASSERT_TRUE(cal::errorForStatus(401) == cal::FetchError::kAddress);
  TEST_ASSERT_TRUE(cal::errorForStatus(410) == cal::FetchError::kAddress);
  TEST_ASSERT_TRUE(cal::errorForStatus(500) == cal::FetchError::kNetwork);
  TEST_ASSERT_TRUE(cal::errorForStatus(429) == cal::FetchError::kNetwork);
  TEST_ASSERT_TRUE(cal::errorForStatus(-1) == cal::FetchError::kNetwork);
}

// Source that reports why a request failed, like the one on the device.
class FailingSource : public FixtureSource {
 public:
  bool fetchCalendar(cal::Parser& parser) override {
    if (status != 200) {
      requests++;
      return false;
    }
    return FixtureSource::fetchCalendar(parser);
  }
  cal::FetchError lastError() const override { return cal::errorForStatus(status); }
  int status = 200;
};

void test_fetch_errors() {
  FailingSource source;
  cal::CalendarSource data(source);
  const auto de = ui::Language::kGerman;
  TEST_ASSERT_TRUE(data.problem(de).message.empty());
  // Address rejected by the server.
  source.status = 404;
  TEST_ASSERT_FALSE(data.fetch(kNow));
  widgets::Problem p = data.problem(de);
  TEST_ASSERT_EQUAL_STRING("Kalender-Adresse prüfen", p.message.c_str());
  TEST_ASSERT_EQUAL_STRING("Auf der Einstellungsseite prüfen (Taste 3)", p.detail.c_str());
  TEST_ASSERT_TRUE(p.settings);
  // Server error or no connection.
  source.status = 503;
  TEST_ASSERT_FALSE(data.fetch(kNow));
  p = data.problem(de);
  TEST_ASSERT_EQUAL_STRING("Kalender nicht erreichbar", p.message.c_str());
  TEST_ASSERT_FALSE(p.settings);
  // A sign-in page instead of the iCal file.
  source.status = 200;
  source.text = readFile("test/fixtures/calendar/not_ical.html");
  TEST_ASSERT_FALSE(data.fetch(kNow));
  TEST_ASSERT_TRUE(data.problem(de).settings);
  // An iCal file cut off: a network problem.
  source.text = readFile("test/fixtures/calendar/week.ics");
  source.text.resize(source.text.size() / 2);
  TEST_ASSERT_FALSE(data.fetch(kNow));
  TEST_ASSERT_TRUE(data.lastError() == cal::FetchError::kNetwork);
  // Success clears the problem.
  source.text = readFile("test/fixtures/calendar/week.ics");
  TEST_ASSERT_TRUE(data.fetch(kNow));
  TEST_ASSERT_TRUE(data.problem(de).message.empty());
}

void test_widget() {
  FixtureSource source;
  cal::CalendarSource data(source);
  cal::AgendaColumnView view(data);
  TEST_ASSERT_TRUE(data.isSetUp());
  TEST_ASSERT_EQUAL_STRING("calendar", data.id());
  TEST_ASSERT_EQUAL_STRING("Kalender", view.title(ui::Language::kGerman).c_str());
  TEST_ASSERT_EQUAL_STRING("Kalender-Adresse fehlt",
                           data.setupMessage(ui::Language::kGerman).message.c_str());

  // Before NTP has answered the window is unknown: no request.
  TEST_ASSERT_FALSE(data.fetch(1000));
  TEST_ASSERT_EQUAL_INT(0, source.requests);

  TEST_ASSERT_TRUE(data.fetch(kNow));
  ui::Bitmap bitmap(214, 260);
  ui::BitmapCanvas canvas(bitmap);
  widgets::DrawContext context;
  context.now_s = kNow;
  view.draw(canvas, context);
  int black = 0;
  for (int y = 0; y < bitmap.height(); y++) {
    for (int x = 0; x < bitmap.width(); x++) black += bitmap.get(x, y) == ui::Color::kBlack;
  }
  TEST_ASSERT_GREATER_THAN(1000, black);

  // Default interval 15 minutes; refresh_minutes changes it.
  TEST_ASSERT_EQUAL_UINT32(900, data.schedule().interval_s);
  cal::Config config;
  config.calendar.refresh_minutes = 60;
  config.display.days = 14;
  cal::CalendarSource hourly(source, config);
  TEST_ASSERT_EQUAL_UINT32(3600, hourly.schedule().interval_s);
  TEST_ASSERT_TRUE(hourly.fetch(kNow));
  TEST_ASSERT_TRUE(hourly.occurrences().size() > data.occurrences().size());

  // A failed request or an incomplete file keeps the last data.
  source.fail = true;
  TEST_ASSERT_FALSE(data.fetch(kNow + 900));
  source.fail = false;
  source.text = source.text.substr(0, source.text.size() / 2);
  TEST_ASSERT_FALSE(data.fetch(kNow + 1800));
  ui::Bitmap again(214, 260);
  ui::BitmapCanvas canvas2(again);
  view.draw(canvas2, context);
  TEST_ASSERT_TRUE(std::equal(bitmap.data(), bitmap.data() + bitmap.size(), again.data()));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_civil_dates);
  RUN_TEST(test_parse_values);
  RUN_TEST(test_rrule_monthly);
  RUN_TEST(test_rrule_weekly_daily_yearly);
  RUN_TEST(test_rrule_limits);
  RUN_TEST(test_time_zones);
  RUN_TEST(test_parser_lines);
  RUN_TEST(test_parser_window);
  RUN_TEST(test_expand_week);
  RUN_TEST(test_expand_exceptions);
  RUN_TEST(test_expand_zones_and_lengths);
  RUN_TEST(test_agenda_morning);
  RUN_TEST(test_emoji_in_titles);
  RUN_TEST(test_agenda_during_the_day);
  RUN_TEST(test_agenda_multi_day);
  RUN_TEST(test_config);
  RUN_TEST(test_config_settings);
  RUN_TEST(test_agenda_settings);
  RUN_TEST(test_widget);
  RUN_TEST(test_not_a_calendar);
  RUN_TEST(test_fetch_errors);
  return UNITY_END();
}
