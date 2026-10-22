#pragma once

// The weather widget: a data source that fetches the forecast for one
// location from Open-Meteo every 30 minutes and a view that shows the
// current weather, the course of the day and the next three days.

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "widgets/weather/config.h"
#include "widgets/weather/forecast.h"
#include "widgets/widget.h"

namespace widgets::weather {

// Where the forecast comes from: the Open-Meteo API on the device, saved
// responses in tests.
class Source {
 public:
  virtual ~Source() = default;
  // Reads the forecast response for the configured location into doc
  // (filtered with forecastFilter()). False if the request failed.
  virtual bool fetchForecast(const Config& config, JsonDocument& doc) = 0;
};

// Data source: the forecast for the configured location.
class WeatherSource : public DataSource {
 public:
  // api must outlive the source.
  WeatherSource(Config config, Source& api);

  const char* id() const override { return "weather"; }
  SetupMessage setupMessage(ui::Language language) const override;

  bool fetches() const override { return true; }
  // Every 30 minutes (the models are updated hourly at most), retries every
  // minute.
  core::Schedule schedule() const override { return {1800, 0, 60}; }
  bool isSetUp() const override { return config_.has_location; }
  bool fetch(int64_t wall_s) override;

  const Config& config() const { return config_; }
  const Forecast& forecast() const { return forecast_; }

 private:
  Config config_;
  Source& api_;
  Forecast forecast_;
};

}  // namespace widgets::weather
