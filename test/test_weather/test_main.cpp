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
      "&hourly=temperature_2m,precipitation_probability,precipitation,weather_code,is_day"
      "&minutely_15=temperature_2m&past_minutely_15=96&forecast_minutely_15=96"
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
  TEST_ASSERT_TRUE(f.quarters.temperatures.empty());  // saved before they were requested
}

void test_parse_quarters() {
  // Response of 2026-10-22 07:30 with 15 minute values from 24 hours back
  // (here 20) to ahead.
  const Forecast f = load("muenchen_20261022.json");
  TEST_ASSERT_EQUAL_INT64(1792575000, f.quarters.start);
  TEST_ASSERT_EQUAL_size_t(160, f.quarters.temperatures.size());
  // On the full hour the same value as the hourly list: 07:00 = 6.7 degrees.
  const size_t i = (f.hours[7].time - f.quarters.start) / widgets::weather::kQuarterSeconds;
  TEST_ASSERT_EQUAL_FLOAT(f.hours[7].temperature, f.quarters.temperatures[i]);
  TEST_ASSERT_EQUAL_FLOAT(6.7f, f.quarters.temperatures[i]);

  // Gaps become NaN; times that are not evenly spaced are not used.
  JsonDocument doc;
  deserializeJson(doc, R"({"current": {"time": 1, "temperature_2m": 5},
      "hourly": {"time": [0], "temperature_2m": [5], "weather_code": [0]},
      "daily": {"time": [0], "temperature_2m_max": [6], "temperature_2m_min": [4]},
      "minutely_15": {"time": [0, 900, 1800], "temperature_2m": [5, null, 6]}})");
  Forecast g;
  TEST_ASSERT_TRUE(widgets::weather::parseForecast(doc, g));
  TEST_ASSERT_EQUAL_size_t(3, g.quarters.temperatures.size());
  TEST_ASSERT_TRUE(std::isnan(g.quarters.temperatures[1]));
  doc["minutely_15"]["time"][2] = 2000;
  TEST_ASSERT_TRUE(widgets::weather::parseForecast(doc, g));
  TEST_ASSERT_TRUE(g.quarters.temperatures.empty());
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

void test_temperature() {
  TEST_ASSERT_EQUAL_INT(0, widgets::weather::displayTemperature(-0.4f));
  TEST_ASSERT_EQUAL_INT(-1, widgets::weather::displayTemperature(-0.6f));
  TEST_ASSERT_EQUAL_INT(13, widgets::weather::displayTemperature(12.5f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_config);
  RUN_TEST(test_url);
  RUN_TEST(test_parse);
  RUN_TEST(test_parse_quarters);
  RUN_TEST(test_parse_precipitation);
  RUN_TEST(test_parse_rejects_broken);
  RUN_TEST(test_conditions);
  RUN_TEST(test_temperature);
  return UNITY_END();
}
