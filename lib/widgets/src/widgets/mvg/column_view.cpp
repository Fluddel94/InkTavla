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

// Disruption messages (design D, stoerung.py).
constexpr int kIncidentSignHeight = 18;
constexpr int kIncidentHeadingX = 24;
constexpr int kIncidentLineStep = 20;
constexpr int kIncidentTitleGap = 28;  // last text line to the first title

const ui::Font& destinationFont() { return ui::fonts::kRobotoCondensedMedium20; }
const ui::Font& timeFont() { return ui::fonts::kRobotoCondensedBold21; }
const ui::Font& delayFont() { return ui::fonts::kRobotoCondensedBold15; }
const ui::Font& incidentHeadingFont() { return ui::fonts::kRobotoCondensedBold17; }
const ui::Font& incidentTextFont() { return ui::fonts::kRobotoCondensedMedium17; }

std::string joinLines(const std::vector<std::string>& lines) {
  std::string text;
  for (const std::string& l : lines) {
    if (!text.empty()) text += ", ";
    text += l;
  }
  return text;
}

// Draws the disruption messages from the top of the column; returns the
// baseline of the first title below them. `below` is the space the station
// title takes between them and the sections.
int drawIncidents(ui::Canvas& c, const std::vector<Incident>& incidents, int sections, int below,
                  ui::Language language) {
  const int baseline = ui::kColumnTitleBaseline;
  ui::drawWarningSign(c, 0, baseline, kIncidentSignHeight);
  ui::drawText(c, incidentHeadingFont(), kIncidentHeadingX, baseline,
               ui::fitTextEllipsis(incidentHeadingFont(), incidentHeading(incidents, language),
                                   c.width() - kIncidentHeadingX));
  // As many lines as leave every section kIncidentMinRows rows, if need be
  // only the heading; with sections at most kIncidentMaxLinesSections.
  int max_lines = sections > 1 ? kIncidentMaxLinesSections : kIncidentMaxLines;
  while (max_lines > 0 &&
         columnRowsThatFit(c.height() - below - kIncidentLineStep * max_lines - kIncidentTitleGap,
                           sections) < kIncidentMinRows * sections) {
    max_lines--;
  }
  std::vector<std::string> lines;
  if (max_lines > 0) {
    lines = ui::wrapText(incidentTextFont(), incidentText(incidents), c.width(), max_lines);
  }
  int y = baseline;
  for (const std::string& line : lines) {
    y += kIncidentLineStep;
    ui::drawText(c, incidentTextFont(), 0, y, line);
  }
  return y + kIncidentTitleGap;
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
  if (t.warning_after_destination) dest_width -= ui::warningSignWidth(kWarningHeight) + 4;
  const std::string dest = d.multi_destination
                               ? ui::fitEvenly(destinationFont(), d.destination, dest_width)
                               : ui::fitText(destinationFont(), d.destination, dest_width);
  const int dest_end = ui::drawText(c, destinationFont(), dest_x, baseline, dest);
  if (t.warning_after_destination) {
    ui::drawWarningSign(c, dest_end + 4, centre + 8, kWarningHeight);
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

std::string incidentHeading(const std::vector<Incident>& incidents, ui::Language language) {
  std::string heading = ui::text(language, ui::Text::kDisruption);
  const std::string lines = joinLines(incidentLines(incidents));
  if (!lines.empty()) heading += " " + lines;
  return heading;
}

std::string incidentText(const std::vector<Incident>& incidents) {
  std::string text;
  for (const Incident& i : incidents) {
    if (!text.empty()) text += " ";
    if (incidents.size() > 1 && !i.lines.empty()) text += joinLines(incidentLines({i})) + ": ";
    text += i.message;
    const char last = i.message.empty() ? '.' : i.message.back();
    if (last != '.' && last != '!' && last != '?') text += ".";
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
  int first_title = ui::kColumnTitleBaseline;
  if (!options.incidents.empty()) {
    first_title = drawIncidents(canvas, options.incidents, n, station_above ? kStationTitleStep : 0,
                                options.language);
  }
  if (sections.empty() || list || station_above) {
    ui::drawColumnTitle(canvas, 0, canvas.width(), first_title, options.station,
                        age ? options.note : options.station_note, ui::TitleIcon::kPin,
                        age && options.note_alert);
    if (station_above) first_title += kStationTitleStep;
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
