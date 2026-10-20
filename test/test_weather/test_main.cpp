// Weather widget: configuration, reading an Open-Meteo response, the rules
// for what is shown (rain outlook, weather texts and symbols) and preview
// images of the tile (compared with the references in test/fixtures/ui/,
// update them with INKTAVLA_UPDATE_PREVIEWS=1).

#include <unity.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/time.h"
#include "hal/png.h"
#include "ui/bitmap.h"
#include "ui/elements.h"
#include "ui/icons.h"
#include "ui/layout.h"
#include "widgets/weather/api.h"
#include "widgets/weather/config.h"
#include "widgets/weather/forecast.h"
#include "widgets/weather/view.h"

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

std::string readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void writeFile(const std::string& path, const std::string& data) {
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream(path, std::ios::binary) << data;
}

void check(const ui::Bitmap& bitmap, const char* name) {
  const std::string png =
      hal::png::encode(bitmap.data(), bitmap.width(), bitmap.height(), bitmap.stride());
  writeFile(std::string(".pio/preview/") + name + ".png", png);
  const std::string reference = std::string("test/fixtures/ui/") + name + ".png";
  if (std::getenv("INKTAVLA_UPDATE_PREVIEWS") != nullptr) {
    writeFile(reference, png);
    return;
  }
  const std::string expected = readFile(reference);
  TEST_ASSERT_TRUE_MESSAGE(!expected.empty(), ("missing reference " + reference).c_str());
  TEST_ASSERT_TRUE_MESSAGE(hal::png::criticalChunks(expected) == png,
                           ("differs from " + reference + ", see .pio/preview/").c_str());
}

ui::Bitmap tile(const Forecast& forecast, int64_t now, const char* note = "München",
                bool alert = false) {
  ui::Bitmap bitmap(440, 218);
  ui::BitmapCanvas canvas(bitmap);
  widgets::weather::ViewOptions options;
  options.title = "Wetter";
  options.note = note;
  options.note_alert = alert;
  options.now_s = now;
  widgets::weather::drawWeather(canvas, forecast, options);
  return bitmap;
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
      "&current=temperature_2m,apparent_temperature,weather_code,is_day"
      "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
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
  const ui::Language de = ui::Language::kGerman;
  TEST_ASSERT_EQUAL_STRING("Sonnig", widgets::weather::conditionText(Condition::kClear, true, de));
  TEST_ASSERT_EQUAL_STRING("Klar", widgets::weather::conditionText(Condition::kClear, false, de));
  TEST_ASSERT_TRUE(widgets::weather::conditionIcon(Condition::kClear, false) ==
                   ui::WeatherIcon::kMoon);
}

void test_temperature() {
  TEST_ASSERT_EQUAL_INT(0, widgets::weather::displayTemperature(-0.4f));
  TEST_ASSERT_EQUAL_INT(-1, widgets::weather::displayTemperature(-0.6f));
  TEST_ASSERT_EQUAL_INT(13, widgets::weather::displayTemperature(12.5f));
}

void test_rain_outlook() {
  Forecast f;
  // Hourly from 2026-10-21 00:00 CEST with 0 % except 40/65/60/55/30 from
  // 14:00 on.
  const int64_t midnight = 1792533600;
  for (int h = 0; h < 24; h++) {
    widgets::weather::Hour hour;
    hour.time = midnight + h * 3600;
    const int p[] = {40, 65, 60, 55, 30};
    hour.rain_probability = h >= 14 && h < 19 ? p[h - 14] : 0;
    f.hours.push_back(hour);
  }
  auto r = widgets::weather::rainOutlook(f, midnight + 9 * 3600 + 1800);  // 09:30
  TEST_ASSERT_TRUE(r.rain);
  TEST_ASSERT_EQUAL_INT(65, r.probability);
  TEST_ASSERT_EQUAL_INT(15, r.from_hour);  // 65, 60, 55 within 15 points
  TEST_ASSERT_EQUAL_INT(18, r.to_hour);
  // A range up to midnight ends at 24.
  f.hours[22].rain_probability = 70;
  f.hours[23].rain_probability = 70;
  r = widgets::weather::rainOutlook(f, midnight + 20 * 3600);
  TEST_ASSERT_EQUAL_INT(22, r.from_hour);
  TEST_ASSERT_EQUAL_INT(24, r.to_hour);
  f.hours[22].rain_probability = 0;
  f.hours[23].rain_probability = 0;
  // Only values below the threshold ahead: no rain.
  r = widgets::weather::rainOutlook(f, midnight + 18 * 3600 + 1800);  // 18:30, 30 % left
  TEST_ASSERT_TRUE(r.rain);
  f.hours[18].rain_probability = 19;
  r = widgets::weather::rainOutlook(f, midnight + 18 * 3600 + 1800);
  TEST_ASSERT_FALSE(r.rain);
  // 01:00: 14:00 is more than 12 hours ahead.
  r = widgets::weather::rainOutlook(f, midnight + 3600);
  TEST_ASSERT_FALSE(r.rain);
}

void test_tile_evening() { check(tile(load(), kNow), "weather_muenchen_evening"); }

void test_tile_stale() { check(tile(load(), kNow, "Stand 18:30", true), "weather_muenchen_stale"); }

void test_tile_rainy_morning() {
  // 2026-10-21 08:15 from the same response: rain bars, dashed line up to
  // now, only two days left in the response.
  check(tile(load(), 1792533600 + 8 * 3600 + 15 * 60), "weather_muenchen_morning");
}

void test_icons() {
  // All weather symbols in the three sizes (32, 26, 50 px).
  ui::Bitmap bitmap(440, 124);
  ui::BitmapCanvas canvas(bitmap);
  for (int i = 0; i < 9; i++) {
    const auto kind = static_cast<ui::WeatherIcon>(i);
    ui::drawIcon(canvas, 8 + i * 48, 4, ui::weatherIcon(kind, 32));
    ui::drawIcon(canvas, 8 + i * 48, 44, ui::weatherIcon(kind, 26));
    ui::drawIcon(canvas, 8 + i * 48, 72, ui::weatherIcon(kind, 50));
  }
  check(bitmap, "weather_icons");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_config);
  RUN_TEST(test_url);
  RUN_TEST(test_parse);
  RUN_TEST(test_parse_rejects_broken);
  RUN_TEST(test_conditions);
  RUN_TEST(test_temperature);
  RUN_TEST(test_rain_outlook);
  RUN_TEST(test_tile_evening);
  RUN_TEST(test_tile_stale);
  RUN_TEST(test_tile_rainy_morning);
  RUN_TEST(test_icons);
  return UNITY_END();
}
