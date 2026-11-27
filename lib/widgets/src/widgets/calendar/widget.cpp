#include "widgets/calendar/widget.h"

#include <utility>

#include "core/syslog.h"
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
  bool cut = false;
  occurrences_ = expandEvents(parser, today, last_day, &cut);
  // A limit leaves events out: a warning in the system log, only numbers,
  // never titles (CR2 M3). Once, not with every update.
  const bool reached = parser.truncated() || cut;
  if (reached && !limit_reached_) {
    core::logWarning("calendar", "limit reached: %u of %u events kept, at most %u entries",
                     static_cast<unsigned>(parser.events().size()),
                     static_cast<unsigned>(parser.eventsRead()),
                     static_cast<unsigned>(kMaxOccurrences));
  }
  limit_reached_ = reached;
  return true;
}

}  // namespace widgets::calendar
