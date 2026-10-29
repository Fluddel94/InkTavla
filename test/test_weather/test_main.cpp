// Weather widget: configuration, reading an Open-Meteo response, the rules
// for what is shown (conditions and symbols). The pictures of the column
// are in test_briefing.

#include <unity.h>

#include <cmath>
#include <fstream>
#include <string>

#include "core/time.h"
#include "ui/icons.h"
#include "widgets/weather/api.h"
#include "widgets/weather/config.h"
#include "widgets/weather/forecast.h"

namespace {

using widgets::weather::Condition;
using widgets::weather::Forecast;

// Time of the saved response: 2026-10-20 18:30 CEST, München.
constexpr int64_t kNow = 1792513800;

Forecast load(const char* fixture = "muenchen_20261020.json") {
  std::ifstream file(std::string("test/fixtures/weather/") + fixture, std::ios::binary);
  JsonDocument doc;
  const DeserializationError error =
      deserializeJson(doc, file, DeserializationOption::Filter(widgets::weather::forecastFilter()));
  TEST_ASSERT_TRUE_MESSAGE(error == DeserializationError::Ok, error.c_str());
  Forecast forecast;
  TEST_ASSERT_TRUE(widgets::weather::parseForecast(doc, forecast));
  return forecast;
}

widgets::weather::Config config(const char* json, core::ConfigIssues& issues) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return widgets::weather::parseConfig(doc.as<JsonObjectConst>(), issues);
}

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

void test_config() {
  core::ConfigIssues issues;
  auto c = config(R"({"latitude": 48.137, "longitude": 11.575, "name": "München"})", issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_TRUE(c.has_location);
  TEST_ASSERT_TRUE(c.latitude > 48.1369 && c.latitude < 48.1371);
  TEST_ASSERT_EQUAL_STRING("München", c.name.c_str());

  issues.clear();
  c = config(R"({"latitude": 48.137})", issues);
  TEST_ASSERT_FALSE(c.has_location);
  TEST_ASSERT_EQUAL_size_t(1, issues.size());

  issues.clear();
  c = config(R"({"latitude": 91, "longitude": 200, "colour": 1})", issues);
  TEST_ASSERT_FALSE(c.has_location);
  TEST_ASSERT_TRUE(issues.size() >= 3);

  // The section of the example file.
  issues.clear();
  std::ifstream file("docs/config.example.json", std::ios::binary);
  JsonDocument example;
  TEST_ASSERT_TRUE(deserializeJson(example, file) == DeserializationError::Ok);
  c = widgets::weather::parseConfig(example["widgets"]["weather"].as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_TRUE(c.has_location);

  issues.clear();
  c = config("{}", issues);
  TEST_ASSERT_FALSE(c.has_location);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());  // not set up yet is no error
}

void test_url() {
  core::ConfigIssues issues;
  const auto c = config(R"({"latitude": 48.13743, "longitude": -11.5})", issues);
  TEST_ASSERT_EQUAL_STRING(
      "https://api.open-meteo.com/v1/forecast?latitude=48.1374&longitude=-11.5000"
      "&current=temperature_2m,weather_code,is_day"
      "&hourly=temperature_2m,precipitation_probability,precipitation,sunshine_duration,"
      "weather_code,is_day"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min"
      "&timezone=auto&timeformat=unixtime&forecast_days=4",
      widgets::weather::forecastUrl(c).c_str());
}

void test_parse() {
  const Forecast f = load();
  TEST_ASSERT_EQUAL_INT64(kNow, f.current.time);
  TEST_ASSERT_EQUAL_size_t(96, f.hours.size());
  TEST_ASSERT_EQUAL_size_t(4, f.days.size());
  TEST_ASSERT_EQUAL_INT64(1792447200, f.hours[0].time);
  TEST_ASSERT_EQUAL_INT64(1792533600, f.days[1].time);
  TEST_ASSERT_FALSE(f.current.day);
}

void test_parse_precipitation() {
  // Response of 2026-10-25 11:45 (a day of steady rain): the amount of the
  // preceding hour in mm next to the probability.
  const Forecast f = load("muenchen_20261025.json");
  TEST_ASSERT_EQUAL_size_t(96, f.hours.size());
  TEST_ASSERT_EQUAL_INT64(1792908000, f.hours[7].time);  // 07:00 (rain 06-07 h)
  TEST_ASSERT_EQUAL_FLOAT(0.9f, f.hours[7].precipitation);
  TEST_ASSERT_EQUAL_INT(100, f.hours[7].rain_probability);
  TEST_ASSERT_EQUAL_FLOAT(1.1f, f.hours[17].precipitation);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f.hours[5].precipitation);
  // Older responses without the amount: 0.
  TEST_ASSERT_EQUAL_FLOAT(0.0f, load().hours[15].precipitation);
  // Missing values, null and negative values count as 0.
  JsonDocument doc;
  deserializeJson(doc, R"({"current": {"time": 1, "temperature_2m": 5},
      "hourly": {"time": [0, 3600, 7200], "temperature_2m": [5, 5, 5], "weather_code": [61, 61, 61],
                 "precipitation": [null, -0.5]},
      "daily": {"time": [0], "temperature_2m_max": [6], "temperature_2m_min": [4]}})");
  Forecast g;
  TEST_ASSERT_TRUE(widgets::weather::parseForecast(doc, g));
  TEST_ASSERT_EQUAL_size_t(3, g.hours.size());
  for (const auto& h : g.hours) TEST_ASSERT_EQUAL_FLOAT(0.0f, h.precipitation);
}

void test_parse_rejects_broken() {
  JsonDocument doc;
  deserializeJson(doc, R"({"current": {"time": 1}, "hourly": {"time": [1, 2],
      "temperature_2m": [1]}})");
  Forecast f;
  TEST_ASSERT_FALSE(widgets::weather::parseForecast(doc, f));
  deserializeJson(doc, R"({"error": true, "reason": "Latitude must be in range"})");
  TEST_ASSERT_FALSE(widgets::weather::parseForecast(doc, f));
}

void test_conditions() {
  using widgets::weather::conditionFromWmo;
  TEST_ASSERT_TRUE(conditionFromWmo(0) == Condition::kClear);
  TEST_ASSERT_TRUE(conditionFromWmo(3) == Condition::kOvercast);
  TEST_ASSERT_TRUE(conditionFromWmo(48) == Condition::kFog);
  TEST_ASSERT_TRUE(conditionFromWmo(55) == Condition::kDrizzle);
  TEST_ASSERT_TRUE(conditionFromWmo(65) == Condition::kRain);
  TEST_ASSERT_TRUE(conditionFromWmo(86) == Condition::kSnow);
  TEST_ASSERT_TRUE(conditionFromWmo(81) == Condition::kShowers);
  TEST_ASSERT_TRUE(conditionFromWmo(96) == Condition::kThunder);
  TEST_ASSERT_TRUE(conditionFromWmo(42) == Condition::kUnknown);
  TEST_ASSERT_TRUE(widgets::weather::conditionIcon(Condition::kClear, false) ==
                   ui::WeatherIcon::kMoon);
}

void test_day_icons_by_code() {
  // Forecast days (2026-10-29): rain by intensity, showers with the sun,
  // freezing drizzle and rain; other codes as the condition by day.
  using widgets::weather::dayIcon;
  using W = ui::WeatherIcon;
  const struct {
    int code;
    W icon;
  } cases[] = {{51, W::kRainLight}, {55, W::kRainLight},    {61, W::kRainLight},
               {63, W::kRain},      {65, W::kRainHeavy},    {80, W::kShowers},
               {82, W::kShowers},   {56, W::kFreezingRain}, {67, W::kFreezingRain},
               {0, W::kSun},        {3, W::kCloudy},        {73, W::kSnow},
               {95, W::kThunder},   {45, W::kFog}};
  for (const auto& c : cases) {
    TEST_ASSERT_TRUE_MESSAGE(dayIcon(c.code, Condition::kUnknown) == c.icon,
                             std::to_string(c.code).c_str());
  }
  // Without a code the condition decides (by day).
  TEST_ASSERT_TRUE(dayIcon(-1, Condition::kClear) == W::kSun);
  TEST_ASSERT_TRUE(dayIcon(-1, Condition::kRain) == W::kRain);
  // The code is kept from the response.
  const Forecast f = load();
  for (const auto& d : f.days) TEST_ASSERT_TRUE(d.code >= 0);
  // The band keeps the umbrella for every rain symbol of the days.
  for (W w : {W::kRainLight, W::kRainHeavy, W::kShowers, W::kFreezingRain}) {
    TEST_ASSERT_EQUAL_PTR(ui::bandIcon(W::kRain).bits, ui::bandIcon(w).bits);
  }
}

void test_temperature() {
  TEST_ASSERT_EQUAL_INT(0, widgets::weather::displayTemperature(-0.4f));
  TEST_ASSERT_EQUAL_INT(-1, widgets::weather::displayTemperature(-0.6f));
  TEST_ASSERT_EQUAL_INT(13, widgets::weather::displayTemperature(12.5f));
}

// --- The symbol of a day from its daylight hours (2026-10-28) ------------------

// 2026-10-27 00:00 CET, the day of the saved response muenchen_20261027.json.
constexpr int64_t kMidnight = 1793055600;

// One hour of the day at `hour` o'clock.
widgets::weather::Hour makeHour(int hour, Condition condition, bool day, float precipitation = 0,
                                float sunshine = -1) {
  widgets::weather::Hour h;
  h.time = kMidnight + hour * 3600;
  h.condition = condition;
  h.day = day;
  h.precipitation = precipitation;
  h.sunshine = sunshine;
  return h;
}

// A forecast of one day with the given condition in every hour; daylight
// from `sunrise` to `sunset` (the hours sunrise..sunset-1 have is_day).
Forecast oneDay(Condition fill, int sunrise = 8, int sunset = 18,
                Condition daily = Condition::kRain) {
  Forecast f;
  for (int h = 0; h < 24; h++) f.hours.push_back(makeHour(h, fill, h >= sunrise && h < sunset));
  widgets::weather::Day d;
  d.time = kMidnight;
  d.condition = daily;
  f.days.push_back(d);
  return f;
}

Condition symbolOf(const Forecast& f) { return widgets::weather::dayCondition(f, f.days[0]); }

void test_day_symbol_case_of_the_27th() {
  // Sun from 10 to 18 h, grey before, grey again from 21 h: the daily code of
  // Open-Meteo is "overcast", the symbol is the sun.
  const Forecast f = load("muenchen_20261027.json");
  TEST_ASSERT_TRUE(f.days[0].condition == Condition::kOvercast);
  const auto day = widgets::weather::daylightHours(f, f.days[0].time + 12 * 3600);
  TEST_ASSERT_EQUAL_size_t(11, day.size());  // 7 to 17 h
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kClear);
  TEST_ASSERT_TRUE(widgets::weather::conditionIcon(symbolOf(f), true) == ui::WeatherIcon::kSun);
}

void test_day_symbol_ignores_the_night() {
  // Fog in the night and in the late evening never counts.
  Forecast f = oneDay(Condition::kClear);
  for (int h : {0, 1, 2, 3, 4, 5, 6, 22, 23}) f.hours[h] = makeHour(h, Condition::kFog, false);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kClear);
  // Neither do rain and a thunderstorm at night.
  for (int h : {0, 1, 2, 3}) f.hours[h] = makeHour(h, Condition::kThunder, false, 2.0f);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kClear);
  // Fog in the day does: half of the day hours.
  for (int h = 8; h < 13; h++) f.hours[h] = makeHour(h, Condition::kFog, true);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kFog);
  // Four of ten day hours are not half.
  f.hours[12] = makeHour(12, Condition::kClear, true);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kClear);
}

void test_day_symbol_follows_the_daylight() {
  // In winter the day is short: the hours before sunrise and after sunset
  // do not count, whatever the hour is.
  Forecast f = oneDay(Condition::kOvercast, 8, 16);
  for (int h = 8; h < 16; h++) f.hours[h] = makeHour(h, Condition::kClear, true);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kClear);
  // In summer the day is long: the early and the late hours count.
  Forecast s = oneDay(Condition::kOvercast, 5, 21);
  for (int h = 8; h < 16; h++) s.hours[h] = makeHour(h, Condition::kClear, true);
  TEST_ASSERT_TRUE(symbolOf(s) == Condition::kOvercast);  // 8 clear, 8 overcast: the cloudier one
  // Counting is blunt: the most frequent weather wins.
  s.hours[16] = makeHour(16, Condition::kClear, true);
  TEST_ASSERT_TRUE(symbolOf(s) == Condition::kClear);
}

void test_day_symbol_rain_thunder_snow() {
  // A thunderstorm for one hour of the day is shown.
  Forecast f = oneDay(Condition::kClear);
  f.hours[12] = makeHour(12, Condition::kThunder, true, 0.3f);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kThunder);
  // Rain: three hours with 0.1 mm (rain although it is mostly sunny) ...
  Forecast r = oneDay(Condition::kClear);
  for (int h : {9, 12, 15}) r.hours[h] = makeHour(h, Condition::kDrizzle, true, 0.1f);
  TEST_ASSERT_TRUE(symbolOf(r) == Condition::kRain);
  // ... but not two.
  r.hours[15] = makeHour(15, Condition::kClear, true);
  TEST_ASSERT_TRUE(symbolOf(r) == Condition::kClear);
  // 1 mm in a single hour is enough, 0.9 mm is not.
  r.hours[12] = makeHour(12, Condition::kShowers, true, 1.0f);
  TEST_ASSERT_TRUE(symbolOf(r) == Condition::kRain);
  r.hours[12] = makeHour(12, Condition::kShowers, true, 0.9f);
  r.hours[9] = makeHour(9, Condition::kClear, true);
  TEST_ASSERT_TRUE(symbolOf(r) == Condition::kClear);
  // Rain at night does not count.
  Forecast n = oneDay(Condition::kClear);
  for (int h = 0; h < 5; h++) n.hours[h] = makeHour(h, Condition::kRain, false, 1.0f);
  TEST_ASSERT_TRUE(symbolOf(n) == Condition::kClear);
  // Snow when most of the wet hours have snow.
  Forecast s = oneDay(Condition::kOvercast);
  for (int h = 10; h < 14; h++) s.hours[h] = makeHour(h, Condition::kSnow, true, 0.4f);
  s.hours[14] = makeHour(14, Condition::kRain, true, 0.4f);
  TEST_ASSERT_TRUE(symbolOf(s) == Condition::kSnow);
}

void test_day_symbol_ties_and_sunshine() {
  // A tie between partly cloudy and overcast: overcast.
  Forecast f = oneDay(Condition::kOvercast);
  for (int h = 8; h < 13; h++) f.hours[h] = makeHour(h, Condition::kPartlyCloudy, true);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kOvercast);
  // Partly cloudy wins with one more hour; clear only with 70 % sunshine.
  f.hours[13] = makeHour(13, Condition::kPartlyCloudy, true);
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kPartlyCloudy);
  for (int h = 8; h < 14; h++) f.hours[h].sunshine = 2700;  // 75 %
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kClear);
  for (int h = 8; h < 14; h++) f.hours[h].sunshine = 2000;  // 56 %
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kPartlyCloudy);
  // Unknown sunshine (-1) never makes it clear.
  for (int h = 8; h < 14; h++) f.hours[h].sunshine = -1;
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kPartlyCloudy);
}

void test_day_symbol_missing_hours() {
  // Fewer than three day hours in the forecast: the daily code.
  Forecast f = oneDay(Condition::kClear, 8, 18, Condition::kSnow);
  f.hours.erase(f.hours.begin() + 10, f.hours.end());  // only 8 and 9 h are day hours
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kSnow);
  // No hours at all: the daily code.
  f.hours.clear();
  TEST_ASSERT_TRUE(symbolOf(f) == Condition::kSnow);
  // Polar night (no day hour): the hours from 8 to 18 o'clock.
  Forecast p = oneDay(Condition::kClear, 30, 30, Condition::kSnow);
  for (int h : {0, 1, 2, 3, 4, 5, 6, 7, 19, 20, 21, 22, 23}) {
    p.hours[h] = makeHour(h, Condition::kFog, false);
  }
  TEST_ASSERT_TRUE(symbolOf(p) == Condition::kClear);
  // Midnight sun (every hour is a day hour): all hours count.
  TEST_ASSERT_TRUE(symbolOf(oneDay(Condition::kOvercast, 0, 24)) == Condition::kOvercast);
}

void test_daylight_hours_from_an_hour() {
  const Forecast f = oneDay(Condition::kClear, 8, 17);
  const auto all = widgets::weather::daylightHours(f, kMidnight + 12 * 3600);
  TEST_ASSERT_EQUAL_size_t(9, all.size());
  const auto rest = widgets::weather::daylightHours(f, kMidnight + 12 * 3600, 13);
  TEST_ASSERT_EQUAL_size_t(4, rest.size());  // 13 to 16 h
  TEST_ASSERT_EQUAL_size_t(0, widgets::weather::daylightHours(f, kMidnight + 12 * 3600, 17).size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_config);
  RUN_TEST(test_url);
  RUN_TEST(test_parse);
  RUN_TEST(test_parse_precipitation);
  RUN_TEST(test_parse_rejects_broken);
  RUN_TEST(test_conditions);
  RUN_TEST(test_day_icons_by_code);
  RUN_TEST(test_temperature);
  RUN_TEST(test_day_symbol_case_of_the_27th);
  RUN_TEST(test_day_symbol_ignores_the_night);
  RUN_TEST(test_day_symbol_follows_the_daylight);
  RUN_TEST(test_day_symbol_rain_thunder_snow);
  RUN_TEST(test_day_symbol_ties_and_sunshine);
  RUN_TEST(test_day_symbol_missing_hours);
  RUN_TEST(test_daylight_hours_from_an_hour);
  return UNITY_END();
}
