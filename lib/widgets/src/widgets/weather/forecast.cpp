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
  for (const char* key : {"time", "temperature_2m", "precipitation_probability", "precipitation",
                          "sunshine_duration", "weather_code", "is_day"}) {
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
  JsonArrayConst amount = hourly["precipitation"];        // optional
  JsonArrayConst sunshine = hourly["sunshine_duration"];  // optional
  JsonArrayConst codes = hourly["weather_code"];
  JsonArrayConst is_day = hourly["is_day"];
  if (times.isNull() || temps.size() != times.size() || codes.size() != times.size()) return false;
  for (size_t i = 0; i < times.size(); i++) {
    if (temps[i].isNull()) continue;  // no value for this hour
    Hour h;
    h.time = times[i].as<int64_t>();
    h.temperature = temps[i].as<float>();
    h.rain_probability = i < rain.size() ? (rain[i] | 0) : 0;
    // Missing or null counts as dry; the models never give less than 0.
    h.precipitation = i < amount.size() ? std::max(0.0f, amount[i] | 0.0f) : 0.0f;
    h.sunshine = i < sunshine.size() && sunshine[i].is<float>() ? sunshine[i].as<float>() : -1;
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

Condition spanCondition(const std::vector<const Hour*>& hours) {
  if (hours.empty()) return Condition::kUnknown;
  int thunder = 0, fog = 0, wet = 0, snow = 0, clear = 0, partly = 0, overcast = 0;
  float total = 0, sun = 0, sun_known = 0;
  for (const Hour* h : hours) {
    total += h->precipitation;
    if (h->precipitation >= kRainHourMm - 0.001f) {
      wet++;
      if (h->condition == Condition::kSnow) snow++;
    }
    if (h->sunshine >= 0) {
      sun += h->sunshine;
      sun_known++;
    }
    switch (h->condition) {
      case Condition::kThunder:
        thunder++;
        break;
      case Condition::kFog:
        fog++;
        break;
      case Condition::kClear:
        clear++;
        break;
      case Condition::kMainlyClear:
      case Condition::kPartlyCloudy:
        partly++;
        break;
      default:
        overcast++;  // overcast, light precipitation, unknown
        break;
    }
  }
  const int n = static_cast<int>(hours.size());
  if (thunder > 0) return Condition::kThunder;
  if (total >= kSymbolRainMm - 0.001f || wet >= kSymbolRainHours) {
    return snow * 2 > wet ? Condition::kSnow : Condition::kRain;
  }
  if (fog * 2 >= n) return Condition::kFog;
  if (overcast >= partly && overcast >= clear) return Condition::kOvercast;
  if (partly >= clear) {
    const bool sunny = sun_known > 0 && sun >= kSunnyShare * 3600.0 * sun_known;
    return sunny ? Condition::kClear : Condition::kPartlyCloudy;
  }
  return Condition::kClear;
}

std::vector<const Hour*> hoursOfDay(const Forecast& forecast, int64_t day_time, int first_hour,
                                    int last_hour) {
  std::vector<const Hour*> list;
  core::LocalTime want;
  if (!core::toLocalTime(day_time, want)) return list;
  for (const Hour& h : forecast.hours) {
    core::LocalTime t;
    if (!core::toLocalTime(h.time, t)) continue;
    if (t.year != want.year || t.month != want.month || t.day != want.day) continue;
    if (t.hour >= first_hour && t.hour <= last_hour) list.push_back(&h);
  }
  return list;
}

std::vector<const Hour*> daylightHours(const Forecast& forecast, int64_t day_time, int first_hour) {
  const std::vector<const Hour*> all = hoursOfDay(forecast, day_time, 0, 23);
  std::vector<const Hour*> list;
  bool any_day = false;
  for (const Hour* h : all) {
    any_day = any_day || h->day;
    core::LocalTime t;
    if (h->day && core::toLocalTime(h->time, t) && t.hour >= first_hour) list.push_back(h);
  }
  // Polar night: no day hour at all, so a fixed window.
  if (!all.empty() && !any_day) {
    return hoursOfDay(forecast, day_time, std::max(first_hour, kDayFirstHour), kDayLastHour);
  }
  return list;
}

Condition dayCondition(const Forecast& forecast, const Day& day) {
  // Daily values start at midnight at the location; noon is safely inside.
  const std::vector<const Hour*> hours = daylightHours(forecast, day.time + 12 * 3600);
  if (static_cast<int>(hours.size()) < kMinDayHours) return day.condition;
  return spanCondition(hours);
}

}  // namespace widgets::weather
