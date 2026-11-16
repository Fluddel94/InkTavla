#pragma once

// Start screen after power-on and every restart, until the first data has
// arrived (see core::DisplayController::enableStartScreen()): the logo
// (grid, word mark "InkTavla", rule) in the middle, at the bottom the firmware version and what the
// device is waiting for (the data, or the saved Wi-Fi).

#include <string>

#include "ui/canvas.h"
#include "ui/font.h"
#include "ui/i18n.h"

namespace widgets::briefing {

struct StartInfo {
  std::string version;  // e.g. "0.1.0"; empty = not shown
  std::string network;  // name of the saved Wi-Fi; empty = not shown
};

// The logo: mark (band on top, two columns below, the lower
// left one black), word mark in `font`, rule below;
// centred on center_x, the word mark on `baseline`.
void drawLogo(ui::Canvas& canvas, const ui::Font& font, int center_x, int baseline);

// Into the whole screen (800 x 480). waiting_for_wifi: not connected yet.
void drawStartScreen(ui::Canvas& canvas, ui::Language language, const StartInfo& info,
                     bool waiting_for_wifi);

}  // namespace widgets::briefing
