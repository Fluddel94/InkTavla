#include "widgets/mvg/column_view.h"

#include <algorithm>
#include <cstdio>

#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/rows.h"
#include "ui/text.h"
#include "widgets/mvg/sections.h"

namespace widgets::mvg {

namespace {

using ui::Color;

// Sizes of the Briefing design (design/briefing/werkzeug, dep_block()).
constexpr int kFirstRowTop = ui::kColumnTitleBaseline + 6;
constexpr int kSymbolHeight = 22;
constexpr int kSymbolPadding = 22;  // added to the label width (measured in Bold 20)
constexpr int kDestinationGap = 8;
constexpr int kTimeRightMargin = 30;  // right edge of the time from the column edge
constexpr int kTimeGap = 10;          // between destination and time
constexpr int kDelayGap = 3;          // between time and delay
constexpr int kWarningHeight = 15;

const ui::Font& symbolFont() { return ui::fonts::kRobotoCondensedBold17; }
const ui::Font& symbolMeasureFont() { return ui::fonts::kRobotoCondensedBold20; }
const ui::Font& destinationFont() { return ui::fonts::kRobotoCondensedMedium20; }
const ui::Font& timeFont() { return ui::fonts::kRobotoCondensedBold21; }
const ui::Font& delayFont() { return ui::fonts::kRobotoCondensedBold15; }

ui::SymbolShape shapeFor(const Departure& d) {
  if (d.bus) return ui::SymbolShape::kRoundFrame;  // buses and replacement services
  switch (d.transport) {
    case TransportType::kTrain:
      return ui::SymbolShape::kFrame;
    case TransportType::kSBahn:
      return ui::SymbolShape::kPill;
    case TransportType::kUBahn:
      return ui::SymbolShape::kRect;
    default:
      return ui::SymbolShape::kBox;  // tram
  }
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
  ui::drawLineSymbol(c, symbolFont(), 0, centre - kSymbolHeight / 2, symbol_width, kSymbolHeight,
                     d.line, shapeFor(d));

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

int drawDeparturesColumn(ui::Canvas& canvas, const std::vector<Section>& sections,
                         const ColumnOptions& options) {
  const bool list = sections.size() == 1 && sections[0].name.empty();
  if (sections.empty() || list) {
    ui::drawColumnTitle(canvas, 0, canvas.width(), ui::kColumnTitleBaseline, options.station,
                        options.note, ui::TitleIcon::kPin, options.note_alert);
  }
  if (sections.empty()) return 0;

  const int n = static_cast<int>(sections.size());
  std::vector<int> available;
  for (const Section& s : sections) available.push_back(static_cast<int>(s.departures.size()));
  const std::vector<int> rows =
      ui::distributeRows(columnRowsThatFit(canvas.height(), n), available);

  // Common symbol width of all visible rows, so the destinations line up.
  int symbol_width = ui::textWidth(symbolMeasureFont(), "S8") + kSymbolPadding;
  for (int s = 0; s < n; s++) {
    const int count = std::min(rows[s], static_cast<int>(sections[s].departures.size()));
    for (int i = 0; i < count; i++) {
      symbol_width = std::max(
          symbol_width,
          ui::textWidth(symbolMeasureFont(), sections[s].departures[i].line) + kSymbolPadding);
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
  int title = ui::kColumnTitleBaseline;
  for (int s = 0; s < n; s++) {
    if (!list) {
      ui::drawColumnTitle(canvas, 0, canvas.width(), title, sections[s].name,
                          s == 0 ? options.note : "", ui::TitleIcon::kArrow,
                          s == 0 && options.note_alert);
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
  options.note = context.note;
  options.note_alert = context.note_alert;
  options.time_format = source_.config().time_format;
  options.language = context.language;
  options.now_s = context.now_s;
  drawDeparturesColumn(canvas, withoutDeparted(source_.sections(), context.now_s), options);
}

}  // namespace widgets::mvg
