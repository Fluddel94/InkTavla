#pragma once

// Requests to the Open-Meteo forecast API (https://open-meteo.com/en/docs).
// Free for non-commercial use; weather data by Open-Meteo.com (CC BY 4.0).
// The URL is built here; the HTTPS request itself is the core's
// (core/http.h).

#include <ArduinoJson.h>

#include <string>

#include "widgets/weather/config.h"
#include "widgets/weather/widget.h"

namespace widgets::weather {

// https://api.open-meteo.com/v1/forecast?latitude=..&longitude=..
//   &current=...&hourly=...&daily=...&timezone=auto&timeformat=unixtime
//   &forecast_days=4
// Coordinates with 4 decimals (about 10 m), the precision of the models is
// far coarser.
std::string forecastUrl(const Config& config);

#if defined(ARDUINO)
// The Open-Meteo API over HTTPS.
class ApiSource : public Source {
 public:
  bool fetchForecast(const Config& config, JsonDocument& doc) override;
};
#endif

}  // namespace widgets::weather
