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

// The rest of title after the given lines of wrapText() (which drops
// spaces at line breaks and may add a hyphen); false if the lines do not
// match the title.
bool restAfter(std::string_view title, const std::vector<std::string>& lines, size_t count,
               std::string_view& rest) {
  size_t pos = 0;
  for (size_t i = 0; i < count; i++) {
    while (pos < title.size() && title[pos] == ' ') pos++;
    const std::string& line = lines[i];
    if (title.compare(pos, line.size(), line) == 0) {
      pos += line.size();
    } else if (!line.empty() && line.back() == '-' &&
               title.compare(pos, line.size() - 1, line, 0, line.size() - 1) == 0) {
      pos += line.size() - 1;  // hyphen added by the hyphenation
    } else {
      return false;
    }
  }
  while (pos < title.size() && title[pos] == ' ') pos++;
  rest = title.substr(pos);
  return true;
}

bool endsWithEllipsis(const std::string& text) {
  const std::string ellipsis = "…";
  return text.size() >= ellipsis.size() &&
         text.compare(text.size() - ellipsis.size(), ellipsis.size(), ellipsis) == 0;
}

// Lines of a title: wrapped once onto a second line (max_lines 2), long
// words hyphenated (as in the disruption text of the departures). If the
// title is still too long, the last line is filled as far as it goes and
// ends with "…" ("Mittagessen mit Jo…", 2026-11-05), also in the middle of
// a word.
std::vector<std::string> titleLines(const ui::Font& font, const std::string& title, int width,
                                    int max_lines) {
  if (max_lines <= 1) return {ui::fitTextEllipsis(font, title, width)};
  std::vector<std::string> lines = ui::wrapText(font, title, width, 0, nullptr, true);
  if (lines.empty()) lines.push_back("");
  // A first word too long for the line ("Projektbesprechung") would be cut
  // in the first line already: then one shortened line is clearer.
  if (lines.size() > 1 && endsWithEllipsis(lines.front())) {
    return {ui::fitTextEllipsis(font, title, width)};
  }
  if (lines.size() <= static_cast<size_t>(max_lines)) return lines;
  const size_t keep = static_cast<size_t>(max_lines) - 1;
  std::string_view rest;
  if (!restAfter(title, lines, keep, rest)) {
    return ui::wrapText(font, title, width, max_lines, nullptr, true);
  }
  lines.resize(keep);
  lines.push_back(ui::fitTextEllipsis(font, rest, width));
  return lines;
}

const ui::Font& kLocationFont = ui::fonts::kRobotoCondensedMedium15;
const ui::Font& kMoreFont = ui::fonts::kRobotoCondensedMedium15;

// A row of today or the next day: how many lines its title gets and whether
// its place is shown below.
struct Row {
  const Entry* entry = nullptr;
  int lines = 1;
  bool location = false;
};

// The rows of a day that fit, and how many events are left out.
struct DayLayout {
  std::vector<Row> rows;
  int hidden = 0;     // events still ahead that did not fit
  bool hint = false;  // "+ 2 weitere Termine" below the rows
};

int rowHeight(const Row& r) {
  return kColumnDayRowHeight + (r.lines > 1 ? kColumnDayLineHeight : 0) +
         (r.location ? kColumnLocationLineHeight : 0);
}

int wrappedLines(const Entry& e, int width, const DisplayConfig& display) {
  if (!display.wrap || e.kind == Entry::Kind::kNone) return 1;
  return titleLines(kTitleFont, e.title, width - kTimeColumn, 2).size() > 1 ? 2 : 1;
}

bool showsLocation(const Entry& e, const DisplayConfig& display) {
  return display.location && e.kind != Entry::Kind::kNone && !e.location.empty();
}

// Layout of today (and of the next day with the hint switched on): first as
// many events as fit in single lines (past events go first, then the last
// ones; with display.more a line for the hint is kept free), then places and
// second title lines in the space that is left, from the top.
DayLayout fitDay(const AgendaDay& day, int width, int space, const DisplayConfig& display) {
  std::vector<const Entry*> all;
  for (const Entry& e : day.entries) all.push_back(&e);
  DayLayout out;
  const auto needed = [&] {
    return static_cast<int>(all.size()) * kColumnDayRowHeight +
           (display.more && out.hidden > 0 ? kColumnMoreLineHeight : 0);
  };
  while (all.size() > 1 && needed() > space) {
    auto past = std::find_if(all.begin(), all.end(), [](const Entry* e) { return e->past; });
    if (past != all.end()) {
      all.erase(past);
    } else {
      all.pop_back();
      out.hidden++;
    }
  }
  int spare = space - static_cast<int>(all.size()) * kColumnDayRowHeight;
  if (display.more && out.hidden > 0 && spare >= kColumnMoreLineHeight) {
    out.hint = true;
    spare -= kColumnMoreLineHeight;
  }
  for (const Entry* e : all) out.rows.push_back({e, 1, false});
  for (Row& r : out.rows) {
    if (showsLocation(*r.entry, display) && kColumnLocationLineHeight <= spare) {
      r.location = true;
      spare -= kColumnLocationLineHeight;
    }
  }
  for (Row& r : out.rows) {
    if (wrappedLines(*r.entry, width, display) > 1 && kColumnDayLineHeight <= spare) {
      r.lines = 2;
      spare -= kColumnDayLineHeight;
    }
  }
  return out;
}

// Layout of the next day without the hint (as before the settings): row by
// row from the top, each with its place and a second line if they still fit,
// until the space is used up.
DayLayout fillDay(const AgendaDay& day, int width, int space, const DisplayConfig& display) {
  DayLayout out;
  int used = 0;
  for (const Entry& e : day.entries) {
    if (used + kColumnDayRowHeight > space) break;
    Row r{&e, 1, false};
    r.location = showsLocation(e, display) && used + rowHeight({&e, 1, true}) <= space;
    if (wrappedLines(e, width, display) > 1 && used + rowHeight({&e, 2, r.location}) <= space) {
      r.lines = 2;
    }
    used += rowHeight(r);
    out.rows.push_back(r);
  }
  return out;
}

bool hasEvents(const AgendaDay& day) {
  for (const Entry& e : day.entries) {
    if (e.kind != Entry::Kind::kNone) return true;
  }
  return false;
}

// Draws one row of today or the next day with its first baseline at y + 22;
// returns its height.
int drawDayRow(ui::Canvas& c, const Row& r, int y) {
  const Entry& e = *r.entry;
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
  const int title_width = c.width() - kTimeColumn;
  const std::vector<std::string> lines = titleLines(kTitleFont, e.title, title_width, r.lines);
  for (size_t i = 0; i < lines.size(); i++) {
    ui::drawText(c, kTitleFont, kTimeColumn, baseline + static_cast<int>(i) * kColumnDayLineHeight,
                 lines[i]);
  }
  int height = kColumnDayRowHeight + (lines.size() > 1 ? kColumnDayLineHeight : 0);
  if (r.location) {
    ui::drawText(c, kLocationFont, kTimeColumn, y + height + 12,
                 ui::fitTextEllipsis(kLocationFont, e.location, title_width));
    height += kColumnLocationLineHeight;
  }
  return height;
}

// Draws the rows of a day from y and the hint below them; returns the new y.
int drawDay(ui::Canvas& c, const DayLayout& layout, int y, ui::Language language) {
  for (const Row& r : layout.rows) y += drawDayRow(c, r, y);
  if (layout.hint) {
    ui::drawText(c, kMoreFont, c.width(), y + 16, ui::moreEvents(language, layout.hidden),
                 Color::kBlack, ui::Align::kRight);
    y += kColumnMoreLineHeight;
  }
  return y;
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

  // Today: as many events as fit in single lines; then places and long
  // titles as long as all of them still fit.
  const DisplayConfig& display = options.display;
  int y = ui::kColumnTitleBaseline + kRowsBelowTitle;
  y = drawDay(canvas, fitDay(agenda[0], width, bottom - y, display), y, lang);

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
  const DayLayout layout = display.more ? fitDay(day, width, bottom - y, display)
                                        : fillDay(day, width, bottom - y, display);
  y = drawDay(canvas, layout, y, lang);
  if (layout.rows.size() < day.entries.size()) return;

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
      const bool room =
          display.wrap && y + kColumnWeekLineHeight + kWeekTitleFont.descent <= bottom;
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
  options.display = source_.config().display;
  drawAgendaColumn(
      canvas, buildAgenda(source_.occurrences(), context.now_s, context.language, options.display),
      options);
}

}  // namespace widgets::calendar
