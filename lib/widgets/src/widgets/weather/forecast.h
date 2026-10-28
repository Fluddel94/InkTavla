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
  // Sunshine of the preceding hour in seconds (0-3600), -1 if unknown.
  float sunshine = -1;
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

// --- Weather of a span of hours ------------------------------------------------
// The daily code of Open-Meteo is the "worst" weather of the whole day (fog
// at 22 h makes a sunny day foggy). The symbol of a day is made from its
// daylight hours instead (the hours Open-Meteo marks with is_day, so from
// sunrise to sunset), by these rules:
//  1. a thunderstorm in at least one hour: thunderstorm;
//  2. precipitation of kSymbolRainMm or more in all, or in at least
//     kSymbolRainHours hours (kRainHourMm or more each): rain, or snow if
//     most of those hours have snow;
//  3. fog in at least half of the hours: fog;
//  4. otherwise the most frequent of clear, partly cloudy (codes 1 and 2) and
//     overcast (the cloudier one on a tie);
//  5. partly cloudy, but sunshine in at least kSunnyShare of the time: clear.
// The night does not count, so fog at 22 h or at 5 h never shows in the symbol.
inline constexpr int kDayFirstHour = 8;  // used if a day has no daylight hours
inline constexpr int kDayLastHour = 17;  // (polar night): the hour from 17 to 18 h
inline constexpr int kMinDayHours = 3;   // fewer hours: the daily code is used
inline constexpr float kSymbolRainMm = 1.0f;
inline constexpr int kSymbolRainHours = 3;
inline constexpr float kRainHourMm = 0.1f;
inline constexpr double kSunnyShare = 0.7;

// The rules above for any hours (Condition::kUnknown if there are none).
Condition spanCondition(const std::vector<const Hour*>& hours);

// The hours of the forecast with local times on the day of `day_time` (any
// time of that day) from first_hour to last_hour.
std::vector<const Hour*> hoursOfDay(const Forecast& forecast, int64_t day_time, int first_hour,
                                    int last_hour);

// The daylight hours (Hour::day) of the local day of `day_time` that start at
// first_hour or later. For a day without any daylight hour (polar night):
// the hours kDayFirstHour to kDayLastHour.
std::vector<const Hour*> daylightHours(const Forecast& forecast, int64_t day_time,
                                       int first_hour = 0);

// The symbol of a day by the rules above; the daily code if fewer than
// kMinDayHours hours are in the forecast.
Condition dayCondition(const Forecast& forecast, const Day& day);

}  // namespace widgets::weather
