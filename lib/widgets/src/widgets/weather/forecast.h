#pragma once

// Weather forecast as the widget shows it, read from an Open-Meteo response
// (https://open-meteo.com/en/docs), plus the rules that turn it into what is
// displayed. Weather data by Open-Meteo.com (CC BY 4.0).

#include <ArduinoJson.h>

#include <cstdint>
#include <vector>

#include "ui/i18n.h"
#include "ui/icons.h"

namespace widgets::weather {

// Weather in a few kinds, from the WMO weather codes the API delivers.
enum class Condition {
  kClear,         // 0
  kMainlyClear,   // 1
  kPartlyCloudy,  // 2
  kOvercast,      // 3
  kFog,           // 45, 48
  kDrizzle,       // 51-57
  kRain,          // 61-67
  kSnow,          // 71-77, 85, 86
  kShowers,       // 80-82
  kThunder,       // 95-99
  kUnknown,
};

Condition conditionFromWmo(int code);
ui::WeatherIcon conditionIcon(Condition condition, bool day);

struct Current {
  int64_t time = 0;  // Unix seconds
  float temperature = 0;
  Condition condition = Condition::kUnknown;
  bool day = true;
};

struct Hour {
  int64_t time = 0;  // start of the hour, Unix seconds
  float temperature = 0;
  int rain_probability = 0;  // %, 0 if unknown
  // Precipitation (rain and snow as water) of the preceding hour in mm, as
  // Open-Meteo gives it: the value of 17:00 fell from 16:00 to 17:00. 0 if
  // unknown.
  float precipitation = 0;
  Condition condition = Condition::kUnknown;
  bool day = true;
};

struct Day {
  int64_t time = 0;  // midnight at the location, Unix seconds
  float high = 0;
  float low = 0;
  Condition condition = Condition::kUnknown;
};

// Temperatures every 15 minutes ("minutely_15"; computed by models with 15
// minute steps in Central Europe and North America, interpolated from hourly
// values elsewhere), for the curve of the day.
inline constexpr int64_t kQuarterSeconds = 900;
struct Quarters {
  int64_t start = 0;                // time of the first value, Unix seconds
  std::vector<float> temperatures;  // one every kQuarterSeconds, NaN if missing
};

struct Forecast {
  Current current;
  std::vector<Hour> hours;
  std::vector<Day> days;
  Quarters quarters;  // empty if the response has none
};

// ArduinoJson filter: only the fields read below.
JsonDocument forecastFilter();

// Reads a response requested with forecastUrl(). False if fields are
// missing or the hourly/daily lists do not fit together. The 15 minute values
// are optional: without them, or if their times are not evenly spaced,
// quarters stays empty.
bool parseForecast(JsonDocument& doc, Forecast& forecast);

// Temperature as shown: rounded, without "-0".
int displayTemperature(float t);

}  // namespace widgets::weather
