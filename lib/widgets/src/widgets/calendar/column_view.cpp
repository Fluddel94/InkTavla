#include "widgets/calendar/column_view.h"

#include <algorithm>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/icons.h"
#include "ui/text.h"
#include "widgets/calendar/datetime.h"

namespace widgets::calendar {

namespace {

using ui::Color;

// Sizes of the Briefing design (design/briefing/werkzeug, calendar_fill()).
constexpr int kTimeColumn = 50;  // titles start here
constexpr int kRowsBelowTitle = 8;
constexpr int kRuleBelowRows = 10;
constexpr int kTitleBelowRows = 32;
constexpr int kWeekTimeSpace = 44;  // kept free for the time on the right

const ui::Font& timeFont(bool bold) {
  return bold ? ui::fonts::kRobotoCondensedBold17 : ui::fonts::kRobotoCondensedMedium17;
}
const ui::Font& titleFont(bool bold) {
  return bold ? ui::fonts::kFraunces20Bold : ui::fonts::kFraunces20;
}

bool hasEvents(const AgendaDay& day) {
  for (const Entry& e : day.entries) {
    if (e.kind != Entry::Kind::kNone) return true;
  }
  return false;
}

// Draws one row of today or the next day with its baseline at y + 22.
void drawDayRow(ui::Canvas& c, const Entry& e, int y) {
  const int baseline = y + 22;
  if (e.kind == Entry::Kind::kNone) {
    ui::drawText(c, titleFont(false), 0, baseline, e.title);
    return;
  }
  if (e.kind == Entry::Kind::kAllDay) {
    ui::drawIcon(c, 2, baseline - 14, ui::kCalendarIcon);
  } else {
    ui::drawText(c, timeFont(e.next), 0, baseline, e.time);
  }
  const ui::Font& f = titleFont(e.next);
  ui::drawText(c, f, kTimeColumn, baseline,
               ui::fitTextEllipsis(f, e.title, c.width() - kTimeColumn));
}

// Entries of today that fit into `rows`: if there are too many, past events
// go first, so that nothing that is still ahead is hidden.
std::vector<const Entry*> visibleToday(const AgendaDay& day, int rows) {
  std::vector<const Entry*> all;
  for (const Entry& e : day.entries) all.push_back(&e);
  int surplus = static_cast<int>(all.size()) - rows;
  for (auto it = all.begin(); it != all.end() && surplus > 0;) {
    if ((*it)->past) {
      it = all.erase(it);
      surplus--;
    } else {
      ++it;
    }
  }
  if (static_cast<int>(all.size()) > rows) all.resize(std::max(0, rows));
  return all;
}

}  // namespace

std::string shortDate(int64_t day, ui::Language language) {
  int y, m, d;
  civilFromDays(day, y, m, d);
  std::string out = ui::weekdayShort(language, weekdayFromDays(day));
  out += ' ';
  out += std::to_string(d);
  if (language == ui::Language::kGerman) out += '.';
  return out;
}

void drawAgendaColumn(ui::Canvas& canvas, const std::vector<AgendaDay>& agenda,
                      const ColumnOptions& options) {
  const int width = canvas.width();
  const int bottom = canvas.height();
  const ui::Language lang = options.language;
  ui::drawColumnTitle(canvas, 0, width, ui::kColumnTitleBaseline, ui::text(lang, ui::Text::kToday),
                      options.note, ui::TitleIcon::kNone, options.note_alert);
  if (agenda.empty()) return;

  // Today: as many rows as fit.
  int y = ui::kColumnTitleBaseline + kRowsBelowTitle;
  const int today_rows = std::max(1, (bottom - y) / kColumnDayRowHeight);
  for (const Entry* e : visibleToday(agenda[0], today_rows)) {
    drawDayRow(canvas, *e, y);
    y += kColumnDayRowHeight;
  }

  // The next day with events, if its title and one row still fit.
  size_t next = 1;
  while (next < agenda.size() && !hasEvents(agenda[next])) next++;
  if (next >= agenda.size()) return;
  const int title = y + kTitleBelowRows;
  if (title + kRowsBelowTitle + kColumnDayRowHeight > bottom) return;
  canvas.dottedHLine(0, y + kRuleBelowRows, width, 2);
  const AgendaDay& day = agenda[next];
  const int weekday = weekdayFromDays(day.day);
  const std::string name = day.day == agenda[0].day + 1
                               ? std::string(ui::text(lang, ui::Text::kTomorrow))
                               : std::string(ui::weekdayName(lang, weekday));
  ui::drawColumnTitle(canvas, 0, width, title, name, shortDate(day.day, lang));
  y = title + kRowsBelowTitle;
  for (const Entry& e : day.entries) {
    if (y + kColumnDayRowHeight > bottom) return;
    drawDayRow(canvas, e, y);
    y += kColumnDayRowHeight;
  }

  // The rest of the week, one line per event.
  bool rule = false;
  for (size_t i = next + 1; i < agenda.size(); i++) {
    for (const Entry& e : agenda[i].entries) {
      if (e.kind == Entry::Kind::kNone) continue;
      const int baseline = y + (rule ? 0 : kRuleBelowRows) + kColumnWeekRowHeight;
      if (baseline > bottom) return;
      if (!rule) {
        canvas.dottedHLine(0, y + kRuleBelowRows, width, 2);
        y += kRuleBelowRows;
        rule = true;
      }
      y = baseline;
      ui::drawText(canvas, ui::fonts::kRobotoCondensedBold16, 0, y,
                   ui::weekdayShort(lang, weekdayFromDays(agenda[i].day)));
      const ui::Font& f = ui::fonts::kFraunces18;
      const bool timed = e.kind == Entry::Kind::kTimed;
      ui::drawText(
          canvas, f, kTimeColumn, y,
          ui::fitTextEllipsis(f, e.title, width - kTimeColumn - (timed ? kWeekTimeSpace : 0)));
      if (timed) {
        ui::drawText(canvas, ui::fonts::kRobotoCondensedMedium15, width, y, e.time, Color::kBlack,
                     ui::Align::kRight);
      }
    }
  }
}

std::string AgendaColumnView::title(ui::Language language) const {
  return ui::text(language, ui::Text::kTitleCalendar);
}

void AgendaColumnView::draw(ui::Canvas& canvas, const DrawContext& context) {
  ColumnOptions options;
  options.language = context.language;
  options.note_alert = context.note_alert;
  if (!context.note.empty()) {
    options.note = context.note;  // age badge
  } else if (core::isValidUnixTime(context.now_s)) {
    options.note = std::string(ui::text(context.language, ui::Text::kWeekShort)) + " " +
                   std::to_string(isoWeek(localDay(context.now_s)));
  }
  drawAgendaColumn(canvas, buildAgenda(source_.occurrences(), context.now_s, context.language),
                   options);
}

}  // namespace widgets::calendar
