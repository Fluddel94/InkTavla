#include "widgets/mvg/column_view.h"

#include <algorithm>
#include <cstdio>

#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/rows.h"
#include "ui/text.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/line_icons.h"
#include "widgets/mvg/sections.h"

namespace widgets::mvg {

namespace {

using ui::Color;

// Sizes of the Briefing design (design/briefing/werkzeug, dep_block()).
constexpr int kFirstRowTop = ui::kColumnTitleBaseline + 6;
constexpr int kSymbolHeight = kLineIconHeight;  // MVV symbols, see line_icons.h
constexpr int kDestinationGap = 8;
constexpr int kTimeRightMargin = 30;  // right edge of the time from the column edge
constexpr int kTimeGap = 10;          // between destination and time
constexpr int kDelayGap = 3;          // between time and delay
constexpr int kWarningHeight = 15;
constexpr int kSignGap = 6;  // between the warning sign and the time column

// Disruption messages (2026-10-26, variant M1): the first line 24 px below
// the title above, the text indented after the warning sign.
constexpr int kIncidentSignHeight = 18;
constexpr int kIncidentTextX = 24;
constexpr int kIncidentFirstStep = 24;  // title above to the first line
constexpr int kIncidentLineStep = 20;
constexpr int kIncidentTitleGap = 28;  // last text line to the next title
// One list has no title below the messages: the rows start right below.
constexpr int kIncidentListGap = 2;

const ui::Font& destinationFont() { return ui::fonts::kRobotoCondensedMedium20; }
const ui::Font& timeFont() { return ui::fonts::kRobotoCondensedBold21; }
const ui::Font& delayFont() { return ui::fonts::kRobotoCondensedBold15; }
const ui::Font& incidentTextFont() { return ui::fonts::kRobotoCondensedMedium17; }

std::string joinLines(const std::vector<std::string>& lines) {
  std::string text;
  for (const std::string& l : lines) {
    if (!text.empty()) text += ", ";
    text += l;
  }
  return text;
}

std::string withFullStop(const std::string& message) {
  const char last = message.empty() ? '.' : message.back();
  return last == '.' || last == '!' || last == '?' ? message : message + ".";
}

// Draws the disruption messages with the first line kIncidentFirstStep below
// the baseline `above` (the station title; -kIncidentFirstStep + title
// baseline if there is none); returns the baseline of the next title (for
// one list: the baseline the rows start below, like below a title).
int drawIncidents(ui::Canvas& c, const std::vector<Incident>& incidents, int above, int sections,
                  bool list) {
  const int first = above + kIncidentFirstStep;
  const int gap = list ? kIncidentListGap : kIncidentTitleGap;
  // As many lines as leave every section kIncidentMinRows rows, but at least
  // one; with sections at most kIncidentMaxLinesSections.
  int max_lines = sections > 1 ? kIncidentMaxLinesSections : kIncidentMaxLines;
  const auto used = [&](int lines) {
    return first - ui::kColumnTitleBaseline + kIncidentLineStep * (lines - 1) + gap;
  };
  while (max_lines > 1 &&
         columnRowsThatFit(c.height() - used(max_lines), sections) < kIncidentMinRows * sections) {
    max_lines--;
  }
  const std::vector<std::string> lines = ui::wrapText(incidentTextFont(), incidentText(incidents),
                                                      c.width() - kIncidentTextX, max_lines);
  ui::drawWarningSign(c, 0, first, kIncidentSignHeight);
  int y = first;
  for (size_t i = 0; i < lines.size(); i++) {
    if (i > 0) y += kIncidentLineStep;
    ui::drawText(c, incidentTextFont(), kIncidentTextX, y, lines[i]);
  }
  return y + gap;
}

std::string timeText(const TimeDisplay& t, ui::Language language) {
  char buffer[16];
  switch (t.kind) {
    case TimeDisplay::Kind::kNow:
      return ui::text(language, ui::Text::kNow);
    case TimeDisplay::Kind::kMinutes:
      std::snprintf(buffer, sizeof(buffer), "%d %s", t.minutes,
                    ui::text(language, ui::Text::kMinutesUnit));
      return buffer;
    case TimeDisplay::Kind::kClock:
      break;
  }
  std::snprintf(buffer, sizeof(buffer), "%02d:%02d", t.hour, t.minute);
  return buffer;
}

void drawRow(ui::Canvas& c, const Departure& d, int symbol_width, int time_width, int top,
             const ColumnOptions& options) {
  const int centre = top + kColumnRowHeight / 2;
  const int baseline = centre + 7;
  const TimeDisplay t = timeDisplay(d, options.time_format, options.now_s);
  drawLineSymbol(c, 0, centre - kSymbolHeight / 2, d.line, d.bus);

  const int time_right = c.width() - kTimeRightMargin;
  const int dest_x = symbol_width + kDestinationGap;
  int dest_width = time_right - time_width - kTimeGap - dest_x;
  // The warning sign stands right before the time column, in the same
  // place in every row.
  const int sign_width = ui::warningSignWidth(kWarningHeight);
  if (t.warning_before_time) dest_width -= sign_width + kSignGap;
  const std::string dest = d.multi_destination
                               ? ui::fitEvenly(destinationFont(), d.destination, dest_width)
                               : ui::fitText(destinationFont(), d.destination, dest_width);
  ui::drawText(c, destinationFont(), dest_x, baseline, dest);
  if (t.warning_before_time) {
    ui::drawWarningSign(c, time_right - time_width - kSignGap - sign_width, centre + 8,
                        kWarningHeight);
  }

  const std::string time = timeText(t, options.language);
  ui::drawText(c, timeFont(), time_right, baseline, time, Color::kBlack, ui::Align::kRight);
  if (t.struck) {
    const int w = ui::textWidth(timeFont(), time);
    const int y = baseline - timeFont().cap_height / 2 - 1;
    c.fillRect(time_right - w - 2, y, w + 4, 2, Color::kBlack);
  }
  if (t.warning_in_time_column) {
    ui::drawWarningSign(c, time_right + 8, centre + 8, kWarningHeight);
  } else if (t.show_delay) {
    char delay[8];
    std::snprintf(delay, sizeof(delay), "%+d", t.delay);
    ui::drawText(c, delayFont(), time_right + kDelayGap, baseline, delay);
  }
}

}  // namespace

int columnRowsThatFit(int canvas_height, int sections) {
  return ui::rowsThatFit(canvas_height - kFirstRowTop, kColumnRowHeight, std::max(1, sections),
                         kColumnSectionGap + 6);
}

std::string incidentText(const std::vector<Incident>& incidents) {
  bool same_lines = true;
  for (const Incident& i : incidents) same_lines = same_lines && i.lines == incidents[0].lines;
  std::string text;
  if (same_lines && !incidents.empty() && !incidents[0].lines.empty()) {
    text = joinLines(incidentLines({incidents[0]})) + ":";
  }
  for (const Incident& i : incidents) {
    if (!text.empty()) text += " ";
    if (!same_lines && !i.lines.empty()) text += joinLines(incidentLines({i})) + ": ";
    text += withFullStop(i.message);
  }
  return text;
}

int drawDeparturesColumn(ui::Canvas& canvas, const std::vector<Section>& sections,
                         const ColumnOptions& options) {
  const bool list = sections.size() == 1 && sections[0].name.empty();
  const int n = std::max(1, static_cast<int>(sections.size()));
  bool own_stations = false;
  for (const Section& s : sections) own_stations = own_stations || s.own_station;
  // Two directions get the station title above their section titles.
  const bool station_above = !sections.empty() && !list && !own_stations;
  const bool age = !options.note.empty();
  // The station title stays at the top; the disruption messages follow
  // below it (two stations: at the top, their sections have the pins).
  int first_title = ui::kColumnTitleBaseline;
  const bool station_title = sections.empty() || list || station_above;
  if (station_title) {
    ui::drawColumnTitle(canvas, 0, canvas.width(), first_title, options.station,
                        age ? options.note : options.station_note, ui::TitleIcon::kPin,
                        age && options.note_alert);
  }
  if (!options.incidents.empty() && !sections.empty()) {
    first_title =
        drawIncidents(canvas, options.incidents,
                      station_title ? first_title : first_title - kIncidentFirstStep, n, list);
  } else if (station_above) {
    first_title += kStationTitleStep;
  }
  if (sections.empty()) return 0;

  std::vector<int> available;
  for (const Section& s : sections) available.push_back(static_cast<int>(s.departures.size()));
  const std::vector<int> rows = ui::distributeRows(
      columnRowsThatFit(canvas.height() - (first_title - ui::kColumnTitleBaseline), n), available);

  // The destinations line up after the widest visible symbol.
  int symbol_width = kLineIconWidth;
  for (int s = 0; s < n; s++) {
    const int count = std::min(rows[s], static_cast<int>(sections[s].departures.size()));
    for (int i = 0; i < count; i++) {
      const Departure& d = sections[s].departures[i];
      symbol_width = std::max(symbol_width, lineSymbolWidth(d.line, d.bus));
    }
  }
  int time_width = ui::textWidth(timeFont(), "00:00");
  if (options.time_format == TimeFormat::kMinutes) {
    char sample[16];
    std::snprintf(sample, sizeof(sample), "59 %s",
                  ui::text(options.language, ui::Text::kMinutesUnit));
    time_width = std::max(time_width, ui::textWidth(timeFont(), sample));
  }

  int drawn = 0;
  int title = first_title;
  for (int s = 0; s < n; s++) {
    if (!list) {
      // Without the station title (two stations) the age badge goes into
      // the first section title.
      const bool badge = s == 0 && age && !station_above;
      ui::drawColumnTitle(canvas, 0, canvas.width(), title, sections[s].name,
                          badge ? options.note : sections[s].note,
                          sections[s].own_station ? ui::TitleIcon::kPin : ui::TitleIcon::kArrow,
                          badge && options.note_alert);
    }
    int top = title + 6;
    const std::vector<Departure>& deps = sections[s].departures;
    if (deps.empty() && rows[s] > 0) {
      ui::drawText(canvas, destinationFont(), 0, top + kColumnRowHeight / 2 + 7,
                   ui::text(options.language, ui::Text::kNoDepartures));
      top += kColumnRowHeight;
    }
    const int count = std::min(rows[s], static_cast<int>(deps.size()));
    for (int i = 0; i < count; i++) {
      drawRow(canvas, deps[i], symbol_width, time_width, top, options);
      top += kColumnRowHeight;
      drawn++;
    }
    title = top + kColumnSectionGap;
  }
  return drawn;
}

std::string DeparturesColumnView::title(ui::Language language) const {
  const std::string& name = source_.stationName();
  return name.empty() ? std::string(ui::text(language, ui::Text::kTitleDepartures)) : name;
}

void DeparturesColumnView::drawTitle(ui::Canvas& canvas, ui::Language language) {
  ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline, title(language), "",
                      ui::TitleIcon::kPin);
}

void DeparturesColumnView::draw(ui::Canvas& canvas, const DrawContext& context) {
  ColumnOptions options;
  options.station = title(context.language);
  options.station_note = source_.config().note;
  options.note = context.note;
  options.note_alert = context.note_alert;
  options.time_format = source_.config().time_format;
  options.language = context.language;
  options.now_s = context.now_s;
  options.incidents = source_.incidents();
  drawDeparturesColumn(canvas, withoutDeparted(source_.sections(), context.now_s), options);
}

}  // namespace widgets::mvg
