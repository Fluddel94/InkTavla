#include "widgets/weather/forecast.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/time.h"

namespace widgets::weather {

Condition conditionFromWmo(int code) {
  switch (code) {
    case 0:
      return Condition::kClear;
    case 1:
      return Condition::kMainlyClear;
    case 2:
      return Condition::kPartlyCloudy;
    case 3:
      return Condition::kOvercast;
    case 45:
    case 48:
      return Condition::kFog;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
      return Condition::kDrizzle;
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
      return Condition::kRain;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      return Condition::kSnow;
    case 80:
    case 81:
    case 82:
      return Condition::kShowers;
    case 95:
    case 96:
    case 99:
      return Condition::kThunder;
    default:
      return Condition::kUnknown;
  }
}

ui::WeatherIcon conditionIcon(Condition condition, bool day) {
  switch (condition) {
    case Condition::kClear:
      return day ? ui::WeatherIcon::kSun : ui::WeatherIcon::kMoon;
    case Condition::kMainlyClear:
    case Condition::kPartlyCloudy:
      return day ? ui::WeatherIcon::kPartlyCloudy : ui::WeatherIcon::kPartlyCloudyNight;
    case Condition::kFog:
      return ui::WeatherIcon::kFog;
    case Condition::kDrizzle:
    case Condition::kRain:
    case Condition::kShowers:
      return ui::WeatherIcon::kRain;
    case Condition::kSnow:
      return ui::WeatherIcon::kSnow;
    case Condition::kThunder:
      return ui::WeatherIcon::kThunder;
    case Condition::kOvercast:
    case Condition::kUnknown:
      break;
  }
  return ui::WeatherIcon::kCloudy;
}

JsonDocument forecastFilter() {
  JsonDocument filter;
  for (const char* key : {"time", "temperature_2m", "weather_code", "is_day"}) {
    filter["current"][key] = true;
  }
  filter["minutely_15"]["time"] = true;
  filter["minutely_15"]["temperature_2m"] = true;
  for (const char* key :
       {"time", "temperature_2m", "precipitation_probability", "weather_code", "is_day"}) {
    filter["hourly"][key] = true;
  }
  for (const char* key : {"time", "weather_code", "temperature_2m_max", "temperature_2m_min"}) {
    filter["daily"][key] = true;
  }
  return filter;
}

bool parseForecast(JsonDocument& doc, Forecast& f) {
  JsonObjectConst current = doc["current"];
  JsonObjectConst hourly = doc["hourly"];
  JsonObjectConst daily = doc["daily"];
  if (current.isNull() || hourly.isNull() || daily.isNull()) return false;
  if (!current["time"].is<int64_t>() || !current["temperature_2m"].is<float>()) return false;

  Forecast result;
  result.current.time = current["time"].as<int64_t>();
  result.current.temperature = current["temperature_2m"].as<float>();
  result.current.condition = conditionFromWmo(current["weather_code"] | -1);
  result.current.day = (current["is_day"] | 1) != 0;

  JsonArrayConst times = hourly["time"];
  JsonArrayConst temps = hourly["temperature_2m"];
  JsonArrayConst rain = hourly["precipitation_probability"];
  JsonArrayConst codes = hourly["weather_code"];
  JsonArrayConst is_day = hourly["is_day"];
  if (times.isNull() || temps.size() != times.size() || codes.size() != times.size()) return false;
  for (size_t i = 0; i < times.size(); i++) {
    if (temps[i].isNull()) continue;  // no value for this hour
    Hour h;
    h.time = times[i].as<int64_t>();
    h.temperature = temps[i].as<float>();
    h.rain_probability = i < rain.size() ? (rain[i] | 0) : 0;
    h.condition = conditionFromWmo(codes[i] | -1);
    h.day = i < is_day.size() ? (is_day[i] | 1) != 0 : true;
    result.hours.push_back(h);
  }

  JsonArrayConst day_times = daily["time"];
  JsonArrayConst highs = daily["temperature_2m_max"];
  JsonArrayConst lows = daily["temperature_2m_min"];
  JsonArrayConst day_codes = daily["weather_code"];
  if (day_times.isNull() || highs.size() != day_times.size() || lows.size() != day_times.size()) {
    return false;
  }
  for (size_t i = 0; i < day_times.size(); i++) {
    if (highs[i].isNull() || lows[i].isNull()) continue;
    Day d;
    d.time = day_times[i].as<int64_t>();
    d.high = highs[i].as<float>();
    d.low = lows[i].as<float>();
    d.condition = conditionFromWmo(i < day_codes.size() ? (day_codes[i] | -1) : -1);
    result.days.push_back(d);
  }
  if (result.hours.empty() || result.days.empty()) return false;

  JsonArrayConst quarter_times = doc["minutely_15"]["time"];
  JsonArrayConst quarter_temps = doc["minutely_15"]["temperature_2m"];
  if (!quarter_times.isNull() && quarter_temps.size() == quarter_times.size() &&
      quarter_times.size() > 0) {
    Quarters q;
    q.start = quarter_times[0].as<int64_t>();
    bool even = true;
    for (size_t i = 0; i < quarter_times.size() && even; i++) {
      even = quarter_times[i].as<int64_t>() == q.start + static_cast<int64_t>(i) * kQuarterSeconds;
      q.temperatures.push_back(quarter_temps[i].is<float>() ? quarter_temps[i].as<float>() : NAN);
    }
    if (even) result.quarters = std::move(q);
  }
  f = std::move(result);
  return true;
}

int displayTemperature(float t) {
  const int r = static_cast<int>(std::lround(t));
  return r == 0 ? 0 : r;  // lround(-0.4) is 0 already; keeps "-0" out
}

}  // namespace widgets::weather
