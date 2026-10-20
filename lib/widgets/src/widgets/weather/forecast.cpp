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

const char* conditionText(Condition condition, bool day, ui::Language language) {
  const bool de = language == ui::Language::kGerman;
  switch (condition) {
    case Condition::kClear:
      return day ? (de ? "Sonnig" : "Sunny") : (de ? "Klar" : "Clear");
    case Condition::kMainlyClear:
      return day ? (de ? "Heiter" : "Mostly sunny") : (de ? "Leicht bewölkt" : "Mostly clear");
    case Condition::kPartlyCloudy:
      return de ? "Wolkig" : "Partly cloudy";
    case Condition::kOvercast:
      return de ? "Bedeckt" : "Overcast";
    case Condition::kFog:
      return de ? "Nebel" : "Fog";
    case Condition::kDrizzle:
      return de ? "Nieselregen" : "Drizzle";
    case Condition::kRain:
      return de ? "Regen" : "Rain";
    case Condition::kSnow:
      return de ? "Schnee" : "Snow";
    case Condition::kShowers:
      return de ? "Schauer" : "Showers";
    case Condition::kThunder:
      return de ? "Gewitter" : "Thunderstorm";
    case Condition::kUnknown:
      break;
  }
  return "";
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
  for (const char* key :
       {"time", "temperature_2m", "apparent_temperature", "weather_code", "is_day"}) {
    filter["current"][key] = true;
  }
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
  result.current.feels_like = current["apparent_temperature"] | result.current.temperature;
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
  f = std::move(result);
  return true;
}

RainOutlook rainOutlook(const Forecast& f, int64_t now_s) {
  RainOutlook o;
  // Hours from the current one on, kOutlookHours ahead.
  std::vector<const Hour*> ahead;
  for (const Hour& h : f.hours) {
    if (h.time + 3600 > now_s && h.time < now_s + kOutlookHours * 3600) ahead.push_back(&h);
  }
  size_t peak = 0;
  for (size_t i = 0; i < ahead.size(); i++) {
    if (ahead[i]->rain_probability > ahead[peak]->rain_probability) peak = i;
  }
  if (ahead.empty() || ahead[peak]->rain_probability < kRainThreshold) return o;
  const int max = ahead[peak]->rain_probability;
  const int limit = std::max(kRainThreshold, max - kRangeTolerance);
  size_t first = peak;
  size_t last = peak;
  while (first > 0 && ahead[first - 1]->rain_probability >= limit) first--;
  while (last + 1 < ahead.size() && ahead[last + 1]->rain_probability >= limit) last++;
  core::LocalTime from, to;
  core::toLocalTime(ahead[first]->time, from);
  core::toLocalTime(ahead[last]->time + 3600, to);
  o.rain = true;
  o.probability = max;
  o.from_hour = from.hour;
  // A range up to midnight ends at "24", not "0" ("21–24 Uhr").
  o.to_hour = to.hour == 0 && from.hour > 0 ? 24 : to.hour;
  return o;
}

int displayTemperature(float t) {
  const int r = static_cast<int>(std::lround(t));
  return r == 0 ? 0 : r;  // lround(-0.4) is 0 already; keeps "-0" out
}

}  // namespace widgets::weather
