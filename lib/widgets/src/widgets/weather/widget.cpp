#include "widgets/weather/widget.h"

#include <utility>

#include "core/psram.h"

namespace widgets::weather {

WeatherSource::WeatherSource(Config config, Source& api) : config_(std::move(config)), api_(api) {}

SetupMessage WeatherSource::setupMessage(ui::Language language) const {
  return {ui::text(language, ui::Text::kLocationMissing),
          "config.json: widgets.weather.latitude/longitude"};
}

bool WeatherSource::fetch(int64_t wall_s) {
  (void)wall_s;
  JsonDocument doc(core::jsonAllocator());
  if (!api_.fetchForecast(config_, doc)) return false;
  Forecast forecast;
  if (!parseForecast(doc, forecast)) return false;
  forecast_ = std::move(forecast);
  return true;
}

}  // namespace widgets::weather
