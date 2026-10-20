#pragma once

// View of the MVG widget: the departure list in a column slot.

#include <cstdint>
#include <string>
#include <vector>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/mvg/departure.h"
#include "widgets/mvg/time_display.h"

namespace widgets::mvg {

struct ViewOptions {
  std::string title;  // station name
  TimeFormat time_format = TimeFormat::kMinutes;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;  // current Unix time, 0 if unknown
};

// Row height of the departure list (48 px: about 8 departures, readable from
// 2-3 m).
inline constexpr int kRowHeight = 48;

// Number of departures that fit into a canvas of this height.
int rowsThatFit(int canvas_height);

// Draws the title and as many departures as fit. Returns the number of
// departures drawn.
int drawDepartures(ui::Canvas& canvas, const std::vector<Departure>& departures,
                   const ViewOptions& options);

}  // namespace widgets::mvg
