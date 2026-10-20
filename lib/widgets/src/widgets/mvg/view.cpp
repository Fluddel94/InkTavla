#include "widgets/mvg/view.h"

#include <algorithm>
#include <cstdio>

#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/text.h"
#include "widgets/mvg/line_icons.h"

namespace widgets::mvg {

namespace {

using ui::Color;

// Sizes for 48 px rows (see the UI proposal from 2026-10-20).
constexpr int kTitleHeight = 44;
constexpr int kTitleLine = 2;
constexpr int kMargin = 4;
constexpr int kSymbolHeight = 33;   // 0.68 x row height
constexpr int kSymbolPadding = 13;  // added to the widest label
constexpr int kDestinationGap = 11;
constexpr int kWarningHeight = 19;  // 0.7 x font size
constexpr int kWarningGap = 5;
constexpr int kIconScale = 2;  // 16 px MVV symbols drawn 32 px high

const ui::Font& textFont() { return ui::fonts::kRobotoCondensedMedium27; }
const ui::Font& boldFont() { return ui::fonts::kRobotoCondensedBold27; }
const ui::Font& symbolFont() { return ui::fonts::kRobotoCondensedBold23; }

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
  std::snprintf(buffer, sizeof(buffer), " %+d", delay);
  return buffer;
}

// Width of the line symbol of a departure: the MVV symbol (S-Bahn, U-Bahn,
// tram) or a generated box/frame with the label (buses, regional trains).
// Buses never use a symbol, even if a tram has the same number.
int symbolWidth(const Departure& d) {
  const LineIcon* icon = d.bus ? nullptr : findLineIcon(d.line);
  if (icon != nullptr) return icon->width * kIconScale;
  return ui::textWidth(symbolFont(), d.line) + kSymbolPadding;
}

}  // namespace

int rowsThatFit(int canvas_height) {
  return std::max(0, (canvas_height - kTitleHeight - kTitleLine) / kRowHeight);
}

int drawDepartures(ui::Canvas& canvas, const std::vector<Departure>& departures,
                   const ViewOptions& options) {
  const int width = canvas.width();
  const ui::Font& title_font = boldFont();

  // Title: station name.
  const int title_baseline = (kTitleHeight + title_font.cap_height) / 2;
  ui::drawText(canvas, title_font, kMargin, title_baseline,
               ui::fitText(title_font, options.title, width - 2 * kMargin));
  canvas.hLine(0, kTitleHeight, width, Color::kBlack, kTitleLine);

  const int top = kTitleHeight + kTitleLine;
  const int rows = std::min(static_cast<int>(departures.size()), rowsThatFit(canvas.height()));
  if (rows == 0) {
    const int baseline = top + (kRowHeight + textFont().cap_height) / 2;
    ui::drawText(canvas, textFont(), width / 2, baseline,
                 ui::text(options.language, ui::Text::kNoDepartures), Color::kBlack,
                 ui::Align::kCenter);
    return 0;
  }

  // Fixed columns for all rows: the destination always starts at the same
  // x, independent of the width of the line symbol.
  int symbol_width = ui::textWidth(symbolFont(), "S8") + kSymbolPadding;
  for (int i = 0; i < rows; i++) {
    symbol_width = std::max(symbol_width, symbolWidth(departures[i]));
  }
  const int dest_x = kMargin + symbol_width + kDestinationGap;

  // Time column; right of it room for "+99" or the warning sign of a
  // cancellation (also in minutes mode, which shows clock times from 60
  // minutes on).
  const bool minutes_mode = options.time_format == TimeFormat::kMinutes;
  const int time_width =
      minutes_mode ? std::max(ui::textWidth(boldFont(), minutesText(59, options.language)),
                              ui::textWidth(boldFont(), "00:00"))
                   : ui::textWidth(textFont(), "00:00");
  const int extra_width =
      std::max(ui::textWidth(boldFont(), delayText(99)), ui::warningSignWidth(kWarningHeight) + 8);
  const int time_x = width - kMargin - extra_width - time_width;

  for (int i = 0; i < rows; i++) {
    const Departure& d = departures[i];
    const TimeDisplay t = timeDisplay(d, options.time_format, options.now_s);
    const int row_top = top + i * kRowHeight;
    const int baseline = row_top + (kRowHeight + textFont().cap_height) / 2;

    const LineIcon* icon = d.bus ? nullptr : findLineIcon(d.line);
    if (icon != nullptr) {
      const int icon_height = kLineIconHeight * kIconScale;
      ui::drawXbm(canvas, kMargin, row_top + (kRowHeight - icon_height) / 2, icon->width,
                  kLineIconHeight, icon->bits, kIconScale);
    } else {
      ui::drawLineSymbol(canvas, symbolFont(), kMargin, row_top + (kRowHeight - kSymbolHeight) / 2,
                         symbol_width, kSymbolHeight, d.line, d.bus);
    }

    // Destination, shortened; warning sign after it.
    int dest_width = time_x - dest_x - 6;
    if (t.warning_after_destination) {
      dest_width -= ui::warningSignWidth(kWarningHeight) + kWarningGap;
    }
    const std::string dest = d.multi_destination
                                 ? ui::fitEvenly(textFont(), d.destination, dest_width)
                                 : ui::fitText(textFont(), d.destination, dest_width);
    const int dest_end = ui::drawText(canvas, textFont(), dest_x, baseline, dest);
    if (t.warning_after_destination) {
      ui::drawWarningSign(canvas, dest_end + kWarningGap, baseline, kWarningHeight);
    }

    // Time column.
    std::string time;
    const ui::Font* font = &boldFont();
    switch (t.kind) {
      case TimeDisplay::Kind::kNow:
        time = ui::text(options.language, ui::Text::kNow);
        break;
      case TimeDisplay::Kind::kMinutes:
        time = minutesText(t.minutes, options.language);
        break;
      case TimeDisplay::Kind::kClock:
        time = clockText(t.hour, t.minute);
        if (!minutes_mode) font = &textFont();
        break;
    }
    const int time_end = ui::drawText(canvas, *font, time_x, baseline, time);
    if (t.struck) {
      const int y = baseline - textFont().cap_height / 2 - 1;
      canvas.hLine(time_x - 2, y, time_end - time_x + 4, Color::kBlack, 2);
    }
    if (t.warning_in_time_column) {
      ui::drawWarningSign(canvas, time_end + 6, baseline, kWarningHeight);
    } else if (t.show_delay) {
      ui::drawText(canvas, boldFont(), time_end, baseline, delayText(t.delay));
    }

    if (i < rows - 1) canvas.hLine(0, row_top + kRowHeight - 1, width, Color::kBlack);
  }
  return rows;
}

}  // namespace widgets::mvg
