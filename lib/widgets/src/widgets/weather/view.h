#pragma once

// View of the weather widget in a wide tile (440 x 218) after the overall UI
// design: current weather on the left, the course of the day as a chart
// (symbols, temperature line, hourly rain probability) on the right, the
// next three days at the bottom.

#include <cstdint>
#include <string>

#include "ui/canvas.h"
#include "ui/i18n.h"
#include "widgets/weather/forecast.h"

namespace widgets::weather {

// Source named in the tile title ("WETTER · OPEN-METEO"): the terms of
// Open-Meteo ask for it wherever their data is shown. It stays while the age
// badge replaces the place.
inline constexpr char kAttribution[] = "Open-Meteo";

struct ViewOptions {
  std::string title;  // tile title, e.g. "Wetter"; kAttribution is added
  std::string note;   // right part of the title, e.g. the place
  bool note_alert = false;
  ui::Language language = ui::Language::kGerman;
  int64_t now_s = 0;  // current Unix time
};

// Hours on the time axis of the chart (local time of the current day).
inline constexpr int kFirstHour = 5;
inline constexpr int kLastHour = 23;
inline constexpr int kMarkStep = 3;  // a symbol and a temperature every 3 hours

void drawWeather(ui::Canvas& canvas, const Forecast& forecast, const ViewOptions& options);

}  // namespace widgets::weather
