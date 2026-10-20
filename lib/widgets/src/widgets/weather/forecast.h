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
// Text for the current weather, e.g. "Heiter", "Bedeckt" (day/night matter
// for clear skies: "Sonnig" / "Klar").
const char* conditionText(Condition condition, bool day, ui::Language language);
ui::WeatherIcon conditionIcon(Condition condition, bool day);

struct Current {
  int64_t time = 0;  // Unix seconds
  float temperature = 0;
  float feels_like = 0;
  Condition condition = Condition::kUnknown;
  bool day = true;
};

struct Hour {
  int64_t time = 0;  // start of the hour, Unix seconds
  float temperature = 0;
  int rain_probability = 0;  // %, 0 if unknown
  Condition condition = Condition::kUnknown;
  bool day = true;
};

struct Day {
  int64_t time = 0;  // midnight at the location, Unix seconds
  float high = 0;
  float low = 0;
  Condition condition = Condition::kUnknown;
};

struct Forecast {
  Current current;
  std::vector<Hour> hours;
  std::vector<Day> days;
};

// ArduinoJson filter: only the fields read below.
JsonDocument forecastFilter();

// Reads a response requested with forecastUrl(). False if fields are
// missing or the hourly/daily lists do not fit together.
bool parseForecast(JsonDocument& doc, Forecast& forecast);

// Rain in the coming hours: the highest probability and when.
struct RainOutlook {
  bool rain = false;  // a probability of at least kRainThreshold
  int probability = 0;
  int from_hour = 0;  // local hours of the time range ("14–18 Uhr")
  int to_hour = 0;    // end of the range (hour after the last one)
};

// Probabilities below this count as "no rain" in the outlook.
inline constexpr int kRainThreshold = 20;
// Hours ahead looked at for the outlook.
inline constexpr int kOutlookHours = 12;
// Hours belong to the range around the highest value while they are at most
// this many percentage points below it.
inline constexpr int kRangeTolerance = 15;

RainOutlook rainOutlook(const Forecast& forecast, int64_t now_s);

// Temperature as shown: rounded, without "-0".
int displayTemperature(float t);

}  // namespace widgets::weather
