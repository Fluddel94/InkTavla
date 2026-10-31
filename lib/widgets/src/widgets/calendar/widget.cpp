#include "widgets/calendar/widget.h"

#include "core/time.h"
#include "widgets/calendar/datetime.h"
#include "widgets/calendar/timezone.h"

namespace widgets::calendar {

CalendarSource::CalendarSource(Source& api) : api_(api) {}

SetupMessage CalendarSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kCalendarAddressMissing),
          ui::text(language, ui::Text::kSetUpOnSettingsPage)};
}

bool CalendarSource::fetch(int64_t wall_s) {
  if (!core::isValidUnixTime(wall_s)) return false;
  const int64_t today = localDay(wall_s);
  const int64_t last_day = today + kAgendaDays - 1 + kExtraDays;
  Parser parser(deviceToUtc(today * kSecondsPerDay), deviceToUtc((last_day + 1) * kSecondsPerDay));
  if (!api_.fetchCalendar(parser)) return false;
  parser.finish();
  if (!parser.valid()) return false;
  occurrences_ = expandEvents(parser, today, last_day);
  return true;
}

}  // namespace widgets::calendar
