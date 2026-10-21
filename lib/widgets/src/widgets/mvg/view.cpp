#include "widgets/mvg/view.h"

#include <algorithm>
#include <cstdio>

#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/rows.h"
#include "ui/text.h"
#include "widgets/mvg/line_icons.h"

namespace widgets::mvg {

namespace {

using ui::Color;

// Sizes from the overall UI design (2026-10-20), provisional until checked
// on the panel from 2-3 m.
constexpr int kPad = ui::kTilePadding;
constexpr int kListTop = 40;     // first row of the list
constexpr int kSectionTop = 38;  // first section title
constexpr int kSectionGap = 4;   // space above the following section titles
constexpr int kSymbolHeight = 26;
constexpr int kSymbolPadding = 22;  // added to the label width
constexpr int kDestinationGap = 10;
constexpr int kTimeGap = 12;        // between destination and time
constexpr int kDelayGap = 6;        // between time and delay column
constexpr int kWarningHeight = 17;  // after the destination
constexpr int kWarningGap = 6;
constexpr int kCancelWarningHeight = 19;  // in the delay column
constexpr int kIconScale = 2;             // 16 px MVV symbols drawn 32 px high
constexpr int kRuleStep = 3;              // dotted line between rows

const ui::Font& destinationFont() { return ui::fonts::kRobotoCondensedMedium24; }
const ui::Font& timeFont() { return ui::fonts::kRobotoCondensedBold26; }
const ui::Font& delayFont() { return ui::fonts::kRobotoCondensedBold21; }
const ui::Font& symbolFont() { return ui::fonts::kRobotoCondensedBold20; }

std::string minutesText(int minutes, ui::Language language) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%d %s", minutes,
                ui::text(language, ui::Text::kMinutesUnit));
  return buffer;
}

std::string clockText(int hour, int minute) {
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
  return buffer;
}

std::string delayText(int delay) {
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "%+d", delay);
  return buffer;
}

// MVV symbol of a departure (S-Bahn, U-Bahn, tram), nullptr for a generated
// one. Buses never use a symbol, even if a tram has the same number.
const LineIcon* iconFor(const Departure& d, const ViewOptions& options) {
  return d.bus || !options.line_icons ? nullptr : findLineIcon(d.line);
}

ui::SymbolShape shapeFor(const Departure& d) {
  if (d.bus) return ui::SymbolShape::kRoundFrame;  // buses and replacement services
  switch (d.transport) {
    case TransportType::kTrain:
      return ui::SymbolShape::kFrame;
    case TransportType::kSBahn:
      return ui::SymbolShape::kPill;
    default:
      return ui::SymbolShape::kBox;
  }
}

int symbolWidth(const Departure& d, const ViewOptions& options) {
  const LineIcon* icon = iconFor(d, options);
  if (icon != nullptr) return icon->width * kIconScale;
  return ui::textWidth(symbolFont(), d.line) + kSymbolPadding;
}

// Columns shared by all rows, so that destinations and times line up.
struct Columns {
  int symbol_width = 0;
  int destination_x = 0;
  int time_right = 0;  // right edge of the time
  int time_width = 0;  // widest time text
  int right = 0;       // right edge of the delay column
};

Columns columns(int width, int symbol_width, const ViewOptions& options) {
  Columns c;
  c.symbol_width = symbol_width;
  c.destination_x = kPad + symbol_width + kDestinationGap;
  c.right = width - kPad;
  // Room for "+99" or the warning sign of a cancellation, also in minutes
  // mode (which shows clock times from 60 minutes on).
  const int extra =
      std::max(ui::textWidth(delayFont(), "+99"), ui::warningSignWidth(kCancelWarningHeight));
  c.time_right = c.right - extra - kDelayGap;
  c.time_width = ui::textWidth(timeFont(), "00:00");
  if (options.time_format == TimeFormat::kMinutes) {
    c.time_width =
        std::max(c.time_width, ui::textWidth(timeFont(), minutesText(59, options.language)));
  }
  return c;
}

void drawRow(ui::Canvas& canvas, const Departure& d, const Columns& col, int row_top, int row_h,
             const ViewOptions& options) {
  const int centre = row_top + row_h / 2;
  const int baseline = centre + (destinationFont().cap_height + 1) / 2;
  const TimeDisplay t = timeDisplay(d, options.time_format, options.now_s);

  // Line symbol.
  const LineIcon* icon = iconFor(d, options);
  if (icon != nullptr) {
    const int icon_height = kLineIconHeight * kIconScale;
    ui::drawXbm(canvas, kPad, centre - icon_height / 2, icon->width, kLineIconHeight, icon->bits,
                kIconScale);
  } else {
    ui::drawLineSymbol(canvas, symbolFont(), kPad, centre - kSymbolHeight / 2, col.symbol_width,
                       kSymbolHeight, d.line, shapeFor(d));
  }

  // Destination, shortened; warning sign directly after it.
  int dest_width = col.time_right - col.time_width - kTimeGap - col.destination_x;
  if (t.warning_after_destination) {
    dest_width -= ui::warningSignWidth(kWarningHeight) + kWarningGap;
  }
  const std::string dest = d.multi_destination
                               ? ui::fitEvenly(destinationFont(), d.destination, dest_width)
                               : ui::fitText(destinationFont(), d.destination, dest_width);
  const int dest_end = ui::drawText(canvas, destinationFont(), col.destination_x, baseline, dest);
  if (t.warning_after_destination) {
    ui::drawWarningSign(canvas, dest_end + kWarningGap, baseline, kWarningHeight);
  }

  // Time, right-aligned.
  std::string time;
  switch (t.kind) {
    case TimeDisplay::Kind::kNow:
      time = ui::text(options.language, ui::Text::kNow);
      break;
    case TimeDisplay::Kind::kMinutes:
      time = minutesText(t.minutes, options.language);
      break;
    case TimeDisplay::Kind::kClock:
      time = clockText(t.hour, t.minute);
      break;
  }
  ui::drawText(canvas, timeFont(), col.time_right, baseline, time, Color::kBlack,
               ui::Align::kRight);
  if (t.struck) {
    const int w = ui::textWidth(timeFont(), time);
    canvas.fillRect(col.time_right - w - 2, centre - 1, w + 4, 2, Color::kBlack);
  }

  // Delay column: warning sign of a cancellation or the delay.
  if (t.warning_in_time_column) {
    ui::drawWarningSign(canvas, col.right - ui::warningSignWidth(kCancelWarningHeight), baseline,
                        kCancelWarningHeight);
  } else if (t.show_delay) {
    ui::drawText(canvas, delayFont(), col.right, baseline, delayText(t.delay), Color::kBlack,
                 ui::Align::kRight);
  }
}

// Rows of one block of departures from top; returns the y below them.
int drawRows(ui::Canvas& canvas, const std::vector<Departure>& departures, int count,
             const Columns& col, int top, int row_h, const ViewOptions& options) {
  if (count == 0) return top;
  if (departures.empty()) {
    const int baseline = top + row_h / 2 + (destinationFont().cap_height + 1) / 2;
    ui::drawText(canvas, destinationFont(), canvas.width() / 2, baseline,
                 ui::text(options.language, ui::Text::kNoDepartures), Color::kBlack,
                 ui::Align::kCenter);
    return top + row_h;
  }
  for (int i = 0; i < count; i++) {
    const int row_top = top + i * row_h;
    if (i > 0) canvas.dottedHLine(kPad, row_top, canvas.width() - 2 * kPad + 1, kRuleStep);
    drawRow(canvas, departures[i], col, row_top, row_h, options);
  }
  return top + count * row_h;
}

}  // namespace

int listRowsThatFit(int canvas_height) {
  return std::max(0, (canvas_height - kListTop) / kListRowHeight);
}

int sectionRowsThatFit(int canvas_height, int sections) {
  if (sections <= 0) return 0;
  const int titles = sections * ui::kSectionTitleHeight + (sections - 1) * kSectionGap;
  return std::max(0, (canvas_height - kSectionTop - titles) / kSectionRowHeight);
}

std::vector<int> splitRows(int total, const std::vector<int>& available) {
  return ui::distributeRows(total, available);
}

int drawDepartures(ui::Canvas& canvas, const std::vector<Section>& sections,
                   const ViewOptions& options) {
  ui::drawTileTitle(canvas, options.title, options.note, options.note_alert);
  if (sections.empty()) return 0;

  const bool list = sections.size() == 1 && sections[0].name.empty();
  std::vector<int> rows;
  if (list) {
    rows.push_back(std::min(static_cast<int>(sections[0].departures.size()),
                            listRowsThatFit(canvas.height())));
    if (rows[0] == 0) rows[0] = 1;  // room for "no departures"
  } else {
    std::vector<int> available;
    for (const Section& s : sections) available.push_back(static_cast<int>(s.departures.size()));
    rows = splitRows(sectionRowsThatFit(canvas.height(), static_cast<int>(sections.size())),
                     available);
  }

  // Common symbol width of all visible rows, so the destinations line up.
  int symbol_width = ui::textWidth(symbolFont(), "S8") + kSymbolPadding;
  for (size_t s = 0; s < sections.size(); s++) {
    const int n = std::min(rows[s], static_cast<int>(sections[s].departures.size()));
    for (int i = 0; i < n; i++) {
      symbol_width = std::max(symbol_width, symbolWidth(sections[s].departures[i], options));
    }
  }
  const Columns col = columns(canvas.width(), symbol_width, options);

  int drawn = 0;
  if (list) {
    drawn = std::min(rows[0], static_cast<int>(sections[0].departures.size()));
    drawRows(canvas, sections[0].departures, rows[0], col, kListTop, kListRowHeight, options);
    return drawn;
  }
  int y = kSectionTop;
  for (size_t s = 0; s < sections.size(); s++) {
    if (s > 0) y += kSectionGap;
    ui::drawSectionTitle(canvas, kPad, canvas.width() - kPad, y, sections[s].name);
    y += ui::kSectionTitleHeight;
    const int n = std::min(rows[s], static_cast<int>(sections[s].departures.size()));
    y = drawRows(canvas, sections[s].departures, sections[s].departures.empty() ? rows[s] : n, col,
                 y, kSectionRowHeight, options);
    drawn += n;
  }
  return drawn;
}

}  // namespace widgets::mvg
