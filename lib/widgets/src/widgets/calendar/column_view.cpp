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

const ui::Font& kTimeFont = ui::fonts::kRobotoCondensedMedium17;
const ui::Font& kTitleFont = ui::fonts::kFraunces20;
const ui::Font& kWeekTitleFont = ui::fonts::kFraunces18;

// Lines of a title: wrapped once onto a second line (max_lines 2), the last
// one shortened with "…" if it is still too long.
std::vector<std::string> titleLines(const ui::Font& font, const std::string& title, int width,
                                    int max_lines) {
  if (max_lines <= 1) return {ui::fitTextEllipsis(font, title, width)};
  std::vector<std::string> lines = ui::wrapText(font, title, width, max_lines);
  if (lines.empty()) lines.push_back("");
  // A first word too long for the line ("Projektbesprechung") would be cut
  // in the first line already: then one shortened line is clearer.
  const std::string ellipsis = "\u2026";
  const std::string& first = lines.front();
  if (lines.size() > 1 && first.size() >= ellipsis.size() &&
      first.compare(first.size() - ellipsis.size(), ellipsis.size(), ellipsis) == 0) {
    return {ui::fitTextEllipsis(font, title, width)};
  }
  return lines;
}

// Height of a row of today or the next day with up to max_lines lines.
int dayRowHeight(const Entry& e, int width, int max_lines) {
  if (e.kind == Entry::Kind::kNone) return kColumnDayRowHeight;
  const size_t lines = titleLines(kTitleFont, e.title, width - kTimeColumn, max_lines).size();
  return kColumnDayRowHeight + (lines > 1 ? kColumnDayLineHeight : 0);
}

bool hasEvents(const AgendaDay& day) {
  for (const Entry& e : day.entries) {
    if (e.kind != Entry::Kind::kNone) return true;
  }
  return false;
}

// Draws one row of today or the next day with its first baseline at y + 22
// and up to max_lines lines; returns its height.
int drawDayRow(ui::Canvas& c, const Entry& e, int y, int max_lines) {
  const int baseline = y + 22;
  if (e.kind == Entry::Kind::kNone) {
    ui::drawText(c, kTitleFont, 0, baseline, e.title);
    return kColumnDayRowHeight;
  }
  if (e.kind == Entry::Kind::kAllDay) {
    ui::drawIcon(c, 2, baseline - 14, ui::kCalendarIcon);
  } else {
    ui::drawText(c, kTimeFont, 0, baseline, e.time);
  }
  const std::vector<std::string> lines =
      titleLines(kTitleFont, e.title, c.width() - kTimeColumn, max_lines);
  for (size_t i = 0; i < lines.size(); i++) {
    ui::drawText(c, kTitleFont, kTimeColumn, baseline + static_cast<int>(i) * kColumnDayLineHeight,
                 lines[i]);
  }
  return kColumnDayRowHeight + (lines.size() > 1 ? kColumnDayLineHeight : 0);
}

// Lines for a row that has `space` px left: two if they fit, else one.
int linesThatFit(const Entry& e, int width, int space) {
  return dayRowHeight(e, width, 2) <= space ? 2 : 1;
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

  // Today: as many events as fit in single lines; then long titles wrap as
  // long as all of them still fit.
  int y = ui::kColumnTitleBaseline + kRowsBelowTitle;
  const int today_rows = std::max(1, (bottom - y) / kColumnDayRowHeight);
  const std::vector<const Entry*> today = visibleToday(agenda[0], today_rows);
  int spare = (bottom - y) - static_cast<int>(today.size()) * kColumnDayRowHeight;
  for (const Entry* e : today) {
    const int extra = dayRowHeight(*e, width, 2) - kColumnDayRowHeight;
    const bool wrap = extra > 0 && extra <= spare;
    if (wrap) spare -= extra;
    y += drawDayRow(canvas, *e, y, wrap ? 2 : 1);
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
    y += drawDayRow(canvas, e, y, linesThatFit(e, width, bottom - y));
  }

  // The rest of the week, one line per event.
  bool rule = false;
  for (size_t i = next + 1; i < agenda.size(); i++) {
    for (const Entry& e : agenda[i].entries) {
      if (e.kind == Entry::Kind::kNone) continue;
      const int baseline = y + (rule ? 0 : kRuleBelowRows) + kColumnWeekRowHeight;
      // Descenders ("g", "p") stay above the bottom edge.
      if (baseline + kWeekTitleFont.descent > bottom) return;
      if (!rule) {
        canvas.dottedHLine(0, y + kRuleBelowRows, width, 2);
        y += kRuleBelowRows;
        rule = true;
      }
      y = baseline;
      ui::drawText(canvas, ui::fonts::kRobotoCondensedBold16, 0, y,
                   ui::weekdayShort(lang, weekdayFromDays(agenda[i].day)));
      const bool timed = e.kind == Entry::Kind::kTimed;
      const int title_width = width - kTimeColumn - (timed ? kWeekTimeSpace : 0);
      // A second line only if it still fits above the bottom edge.
      const bool room = y + kColumnWeekLineHeight + kWeekTitleFont.descent <= bottom;
      const std::vector<std::string> lines =
          titleLines(kWeekTitleFont, e.title, title_width, room ? 2 : 1);
      for (size_t l = 0; l < lines.size(); l++) {
        ui::drawText(canvas, kWeekTitleFont, kTimeColumn,
                     y + static_cast<int>(l) * kColumnWeekLineHeight, lines[l]);
      }
      if (timed) {
        ui::drawText(canvas, ui::fonts::kRobotoCondensedMedium15, width, y, e.time, Color::kBlack,
                     ui::Align::kRight);
      }
      if (lines.size() > 1) y += kColumnWeekLineHeight;
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
