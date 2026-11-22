#include "widgets/calendar/address_test.h"

#include <ArduinoJson.h>

#include "core/time.h"
#include "widgets/calendar/agenda.h"
#include "widgets/calendar/datetime.h"
#include "widgets/calendar/timezone.h"

namespace widgets::calendar {

AddressTest testAddress(Source& source, int64_t wall_s) {
  AddressTest test;
  if (!core::isValidUnixTime(wall_s)) {
    test.outcome = TestOutcome::kNoTime;
    return test;
  }
  const int64_t today = localDay(wall_s);
  const int64_t last_day = today + kTestDays - 1;
  Parser parser(deviceToUtc(today * kSecondsPerDay), deviceToUtc((last_day + 1) * kSecondsPerDay));
  const bool fetched = source.fetchCalendar(parser);
  if (fetched) parser.finish();
  if (parser.notCalendar()) {
    test.outcome = TestOutcome::kNotIcal;
  } else if (!fetched) {
    const FetchError error = source.lastError();
    test.outcome = error == FetchError::kAddress   ? TestOutcome::kAddress
                   : error == FetchError::kTimeout ? TestOutcome::kTimeout
                                                   : TestOutcome::kNetwork;
  } else if (!parser.valid()) {
    test.outcome = TestOutcome::kNetwork;  // started as iCal, ended early
  } else {
    test.outcome = TestOutcome::kOk;
    test.events = expandEvents(parser, today, last_day).size();
  }
  return test;
}

const char* testOutcomeName(TestOutcome outcome) {
  switch (outcome) {
    case TestOutcome::kOk:
      return "ok";
    case TestOutcome::kAddress:
      return "address";
    case TestOutcome::kNotIcal:
      return "not_ical";
    case TestOutcome::kNetwork:
      return "network";
    case TestOutcome::kTimeout:
      return "timeout";
    case TestOutcome::kNoTime:
      return "no_time";
  }
  return "network";
}

std::string addressTestJson(const AddressTest& test) {
  JsonDocument doc;
  const bool ok = test.outcome == TestOutcome::kOk;
  doc["ok"] = ok;
  if (ok) {
    doc["events"] = test.events;
    doc["days"] = kTestDays;
  } else {
    doc["error"] = testOutcomeName(test.outcome);
  }
  std::string json;
  serializeJson(doc, json);
  return json;
}

}  // namespace widgets::calendar
