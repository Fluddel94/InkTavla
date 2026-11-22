#include "widgets/calendar/widget.h"

#include <utility>

#include "core/time.h"
#include "widgets/calendar/datetime.h"
#include "widgets/calendar/timezone.h"

namespace widgets::calendar {

CalendarSource::CalendarSource(Source& api, Config config)
    : api_(api), config_(std::move(config)) {}

SetupMessage CalendarSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kCalendarAddressMissing),
          ui::text(language, ui::Text::kSetUpOnSettingsPage)};
}

FetchError errorForStatus(int status) {
  switch (status) {
    case 400:
    case 401:
    case 403:
    case 404:
    case 410:
      return FetchError::kAddress;
    default:
      return FetchError::kNetwork;
  }
}

Problem CalendarSource::problem(ui::Language language) const {
  switch (error_) {
    case FetchError::kAddress:
      return {ui::text(language, ui::Text::kCalendarCheckAddress),
              ui::text(language, ui::Text::kCheckOnSettingsPage), true};
    case FetchError::kNetwork:
    case FetchError::kTimeout:
      return {ui::text(language, ui::Text::kCalendarUnreachable), "", false};
    case FetchError::kNone:
      break;
  }
  return {};
}

bool CalendarSource::fetch(int64_t wall_s) {
  if (!core::isValidUnixTime(wall_s)) return false;
  const int64_t today = localDay(wall_s);
  const int64_t last_day = today + config_.display.days - 1 + kExtraDays;
  Parser parser(deviceToUtc(today * kSecondsPerDay), deviceToUtc((last_day + 1) * kSecondsPerDay));
  const bool fetched = api_.fetchCalendar(parser);
  if (fetched) parser.finish();
  if (parser.notCalendar()) {
    error_ = FetchError::kAddress;
    return false;
  }
  if (!fetched) {
    error_ = api_.lastError();
    return false;
  }
  if (!parser.valid()) {
    // Started as iCal but ended early (connection cut off).
    error_ = FetchError::kNetwork;
    return false;
  }
  error_ = FetchError::kNone;
  occurrences_ = expandEvents(parser, today, last_day);
  return true;
}

}  // namespace widgets::calendar
