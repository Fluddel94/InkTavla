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

// Messages of the departures that are really drawn: the first `rows[s]`
// departures of every section, in the order of the sections.
std::vector<Incident> visibleIncidents(const std::vector<Section>& sections,
                                       const std::vector<int>& rows) {
  std::vector<Incident> result;
  for (size_t s = 0; s < sections.size() && s < rows.size(); s++) {
    const std::vector<Departure>& deps = sections[s].departures;
    const size_t count = std::min(static_cast<size_t>(std::max(0, rows[s])), deps.size());
    for (size_t i = 0; i < count; i++) {
      for (const DepartureIncident& m : deps[i].incidents) {
        mergeIncidents(result, {{m.message, {m.line}}});
      }
    }
  }
  return result;
}

// The text lines of the disruption messages and the rows the sections get
// below them. The text and the rows depend on each other (the rows decide
// which departures, and so which messages, are shown), so the text gets the
// fewest lines it needs, at most the lines allowed; with more
// lines every section would get fewer than kIncidentMinRows rows, the text
// ends with "..." instead.
// Wraps the text (hyphenating long words) and puts `more` at the end of the
// last line, on a line of its own if there is a line left, or by shortening
// the last line. A text that has to be cut ends with "…" after the last
// letter that fits (not after a whole word, 2026-11-23). *complete is false
// if the text had to be cut.
std::vector<std::string> wrapIncident(const IncidentLabel& label, int width, int max_lines,
                                      bool* complete) {
  std::vector<std::string> lines =
      ui::wrapText(incidentTextFont(), label.text, width, max_lines, complete, true, true);
  if (label.more.empty() || lines.empty()) return lines;
  const int more_width = ui::textWidth(incidentTextFont(), " " + label.more);
  if (ui::textWidth(incidentTextFont(), lines.back()) + more_width <= width) {
    lines.back() += " " + label.more;
    return lines;
  }
  if (static_cast<int>(lines.size()) < max_lines) {
    lines.push_back(label.more);
    return lines;
  }
  // No line left: the last line is shortened to make room (ending with "…").
  constexpr char kEllipsis[] = "\u2026";
  std::string last = lines.back();
  if (last.size() >= 3 && last.compare(last.size() - 3, 3, kEllipsis) == 0) {
    last.resize(last.size() - 3);
  }
  while (!last.empty() &&
         ui::textWidth(incidentTextFont(), last + kEllipsis) + more_width > width) {
    last.pop_back();
    while (!last.empty() && (static_cast<unsigned char>(last.back()) & 0xC0) == 0x80) {
      last.pop_back();
    }
    while (!last.empty() && (last.back() == ' ' || last.back() == ',')) last.pop_back();
  }
  lines.back() = last + kEllipsis + " " + label.more;
  if (complete != nullptr) *complete = false;
  return lines;
}

// True if the departures drawn belong to more than one line.
bool showsSeveralLines(const std::vector<Section>& sections, const std::vector<int>& rows) {
  std::string first;
  for (size_t s = 0; s < sections.size() && s < rows.size(); s++) {
    const std::vector<Departure>& deps = sections[s].departures;
    const size_t count = std::min(static_cast<size_t>(std::max(0, rows[s])), deps.size());
    for (size_t i = 0; i < count; i++) {
      if (first.empty()) first = deps[i].line;
      if (deps[i].line != first) return true;
    }
  }
  return false;
}

struct IncidentPlan {
  std::vector<std::string> lines;  // empty: no message to show
  std::vector<int> rows;           // rows of the sections below the text
};

// `first`: baseline of the first line; `gap`: from the last line to the next
// title (one list: to the rows).
IncidentPlan planIncidents(int canvas_height, int text_width, const std::vector<Section>& sections,
                           int first, int gap, int max_lines, ui::Language language) {
  const int n = std::max(1, static_cast<int>(sections.size()));
  std::vector<int> available;
  for (const Section& s : sections) available.push_back(static_cast<int>(s.departures.size()));
  IncidentPlan plan;
  for (int lines = 1; lines <= max_lines; lines++) {
    const int height = first - ui::kColumnTitleBaseline + kIncidentLineStep * (lines - 1) + gap;
    const int fit = columnRowsThatFit(canvas_height - height, n);
    if (lines > 1 && fit < kIncidentMinRows * n) break;
    const std::vector<int> rows = ui::distributeRows(fit, available);
    const std::vector<Incident> incidents = visibleIncidents(sections, rows);
    if (incidents.empty()) {
      if (plan.lines.empty()) return IncidentPlan();
      break;
    }
    bool complete = true;
    plan.lines = wrapIncident(incidentLabel(incidents, showsSeveralLines(sections, rows), language),
                              text_width, lines, &complete);
    plan.rows = rows;
    if (complete) break;
  }
  return plan;
}

// Draws the warning sign and the lines, the first with its baseline at
// `first`; returns the baseline of the next title (for one list: the baseline
// the rows start below, like below a title).
int drawIncidentLines(ui::Canvas& c, const std::vector<std::string>& lines, int first, int gap) {
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

  // With the unit ("+8 min", step L) the delay needs more room on the right.
  const int right_margin =
      options.delay_unit
          ? kDelayGap +
                ui::textWidth(delayFont(), std::string("+88 ") +
                                               ui::text(options.language, ui::Text::kMinutesUnit))
          : kTimeRightMargin;
  const int time_right = c.width() - right_margin;
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
  if (t.show_delay) {
    char delay[8];
    std::snprintf(delay, sizeof(delay), "%+d", t.delay);
    std::string text = delay;
    if (options.delay_unit)
      text += std::string(" ") + ui::text(options.language, ui::Text::kMinutesUnit);
    ui::drawText(c, delayFont(), time_right + kDelayGap, baseline, text);
  }
}

}  // namespace

int columnRowsThatFit(int canvas_height, int sections) {
  return ui::rowsThatFit(canvas_height - kFirstRowTop, kColumnRowHeight, std::max(1, sections),
                         kColumnSectionGap + 6);
}

IncidentLabel incidentLabel(const std::vector<Incident>& incidents, bool with_lines,
                            ui::Language /*language*/) {
  IncidentLabel label;
  if (incidents.empty()) return label;
  const Incident& first = incidents[0];
  if (with_lines && !first.lines.empty()) label.text = joinLines(incidentLines({first})) + ": ";
  label.text += withFullStop(first.message);
  if (incidents.size() > 1) {
    label.more = "+" + std::to_string(incidents.size() - 1);  // all languages (2026-11-24)
  }
  return label;
}

int drawDeparturesColumn(ui::Canvas& canvas, const std::vector<Section>& sections,
                         const ColumnOptions& options, std::vector<std::string>* incident_lines) {
  if (incident_lines != nullptr) incident_lines->clear();
  const bool list = sections.size() == 1 && sections[0].name.empty();
  const int n = std::max(1, static_cast<int>(sections.size()));
  bool own_stations = false;
  for (const Section& s : sections) own_stations = own_stations || s.own_station;
  // Two directions get the station title above their section titles.
  const bool station_above = !sections.empty() && !list && !own_stations;
  const bool age = !options.note.empty();
  // The station title stays at the top; the disruption messages follow
  // below it (two stations: below the pin title of the first section, the
  // rows of that section follow the text).
  int first_title = ui::kColumnTitleBaseline;
  const bool station_title = sections.empty() || list || station_above;
  if (station_title) {
    ui::drawColumnTitle(canvas, 0, canvas.width(), first_title, options.station,
                        age ? options.note : "", ui::TitleIcon::kPin, age && options.note_alert);
  }
  std::vector<int> available;
  for (const Section& s : sections) available.push_back(static_cast<int>(s.departures.size()));
  std::vector<int> rows;
  IncidentPlan plan;
  // Two stations: the text follows the title of the first section and the
  // rows come right after it (like below the station title of one list).
  const bool text_in_section = own_stations && !list;
  const int text_first = ui::kColumnTitleBaseline + kIncidentFirstStep;
  const int text_gap = list || text_in_section ? kIncidentListGap : kIncidentTitleGap;
  int text_end = 0;  // baseline the rows of the first section start below
  if (options.incident_lines > 0 && !sections.empty()) {
    plan = planIncidents(canvas.height(), canvas.width() - kIncidentTextX, sections, text_first,
                         text_gap, options.incident_lines, options.language);
  }
  if (!plan.lines.empty()) {
    if (incident_lines != nullptr) *incident_lines = plan.lines;
    rows = plan.rows;
    if (!text_in_section) first_title = drawIncidentLines(canvas, plan.lines, text_first, text_gap);
  } else {
    if (station_above) first_title += kStationTitleStep;
    if (sections.empty()) return 0;
    rows = ui::distributeRows(
        columnRowsThatFit(canvas.height() - (first_title - ui::kColumnTitleBaseline), n),
        available);
  }
  if (sections.empty()) return 0;

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
                          badge ? options.note : "",
                          sections[s].own_station ? ui::TitleIcon::kPin : ui::TitleIcon::kArrow,
                          badge && options.note_alert);
    }
    int top = title + 6;
    if (s == 0 && text_in_section && !plan.lines.empty()) {
      text_end = drawIncidentLines(canvas, plan.lines, text_first, text_gap);
      top = text_end + 6;
    }
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
  options.note = context.note;
  options.note_alert = context.note_alert;
  options.time_format = source_.config().time_format;
  options.language = context.language;
  options.now_s = context.now_s;
  options.incident_lines = source_.config().incident_lines;
  options.delay_unit = step_ == ui::SizeStep::kL;
  drawDeparturesColumn(canvas, withoutDeparted(source_.sections(), context.now_s), options);
}

}  // namespace widgets::mvg
