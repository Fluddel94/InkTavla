#include "widgets/header/header.h"

#include <cstdio>

#include "core/time.h"
#include "ui/elements.h"
#include "ui/fonts.h"
#include "ui/text.h"

namespace widgets::header {

namespace {

// Positions from the overall UI design (2026-10-20).
constexpr int kDateX = 16;
constexpr int kDateBaseline = 29;
constexpr int kTimeRightMargin = 48;  // right edge of the time from the right
constexpr int kTimeBaseline = 30;
constexpr int kWifiSize = 22;
constexpr int kWifiRightMargin = 38;  // left edge of the symbol from the right
constexpr int kWifiTop = 8;

}  // namespace

void draw(ui::Canvas& canvas, const State& state) {
  const int width = canvas.width();
  core::LocalTime local;
  const bool valid = core::isValidUnixTime(state.now_s) && core::toLocalTime(state.now_s, local);

  char time[8] = "--:--";
  if (valid) {
    std::snprintf(time, sizeof(time), "%02d:%02d", local.hour, local.minute);
    ui::drawText(canvas, ui::fonts::kRobotoCondensedMedium26, kDateX, kDateBaseline,
                 ui::longDate(state.language, local.weekday, local.day, local.month));
  }
  ui::drawText(canvas, ui::fonts::kRobotoCondensedBold30, width - kTimeRightMargin, kTimeBaseline,
               time, ui::Color::kBlack, ui::Align::kRight);
  ui::drawWifiSymbol(canvas, width - kWifiRightMargin, kWifiTop, kWifiSize, state.wifi_connected);
}

}  // namespace widgets::header
