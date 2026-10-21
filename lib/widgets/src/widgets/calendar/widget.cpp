#include "widgets/calendar/widget.h"

#include "core/time.h"
#include "widgets/calendar/datetime.h"
#include "widgets/calendar/timezone.h"
#include "widgets/calendar/view.h"

namespace widgets::calendar {

CalendarSource::CalendarSource(Source& api) : api_(api) {}

SetupMessage CalendarSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kCalendarAddressMissing),
          "USB: tools/inktavla_config.py calendar-url"};
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

std::string AgendaView::title(ui::Language language) const {
  return ui::text(language, ui::Text::kTitleCalendar);
}

void AgendaView::draw(ui::Canvas& canvas, const DrawContext& context) {
  ViewOptions options;
  options.title = title(context.language);
  options.language = context.language;
  options.note_alert = context.note_alert;
  if (!context.note.empty()) {
    options.note = context.note;  // age badge
  } else if (core::isValidUnixTime(context.now_s)) {
    options.note = std::string(ui::text(context.language, ui::Text::kWeekShort)) + " " +
                   std::to_string(isoWeek(localDay(context.now_s)));
  }
  drawCalendar(canvas, buildAgenda(source_.occurrences(), context.now_s, context.language),
               options);
}

}  // namespace widgets::calendar
