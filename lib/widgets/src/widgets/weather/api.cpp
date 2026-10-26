#include "widgets/weather/api.h"

#include <cstdio>

#if defined(ARDUINO)
#include "core/http.h"
#endif

namespace widgets::weather {

std::string forecastUrl(const Config& config) {
  char coordinates[64];
  std::snprintf(coordinates, sizeof(coordinates), "latitude=%.4f&longitude=%.4f", config.latitude,
                config.longitude);
  std::string url = "https://api.open-meteo.com/v1/forecast?";
  url += coordinates;
  url +=
      "&current=temperature_2m,weather_code,is_day"
      "&hourly=temperature_2m,precipitation_probability,precipitation,sunshine_duration,"
      "weather_code,is_day"
      "&minutely_15=temperature_2m&past_minutely_15=96&forecast_minutely_15=96"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min"
      "&timezone=auto&timeformat=unixtime&forecast_days=4";
  return url;
}

#if defined(ARDUINO)
bool ApiSource::fetchForecast(const Config& config, JsonDocument& doc) {
  return core::fetchJsonStream(forecastUrl(config), doc, forecastFilter());
}
#endif

}  // namespace widgets::weather
