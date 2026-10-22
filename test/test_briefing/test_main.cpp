// The "Briefing" layout: the rules of the sentence, the adaptive weather
// chart (also with extreme values) and preview images of the band and the
// columns, compared with the references in test/fixtures/ui/ (update them
// with INKTAVLA_UPDATE_PREVIEWS=1).

#include <unity.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/time.h"
#include "hal/png.h"
#include "ui/bitmap.h"
#include "ui/elements.h"
#include "ui/layout.h"
#include "widgets/briefing/band.h"
#include "widgets/briefing/sentence.h"
#include "widgets/calendar/agenda.h"
#include "widgets/calendar/column_view.h"
#include "widgets/calendar/parser.h"
#include "widgets/mvg/column_view.h"
#include "widgets/mvg/config.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/sections.h"
#include "widgets/weather/column_view.h"
#include "widgets/weather/forecast.h"

namespace {

namespace br = widgets::briefing;
namespace cal = widgets::calendar;
namespace wx = widgets::weather;
using wx::Condition;

constexpr int64_t kDay = 86400;
// 2026-10-20 00:00 CEST (Tuesday) and 07:42 that morning.
constexpr int64_t kMidnight = 1792447200;
constexpr int64_t kMorning = kMidnight + 7 * 3600 + 42 * 60;
// Departures fixture: 2026-10-19 22:25 CEST.
constexpr int64_t kMvgNow = 1792441500;

// --- Helpers ----------------------------------------------------------------------

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

// A forecast for the day of `midnight`: 24 hourly temperatures and rain
// probabilities, today and the next three days.
wx::Forecast forecast(const std::vector<float>& temps, const std::vector<int>& rain,
                      Condition hours = Condition::kOvercast,
                      Condition today = Condition::kOvercast, int64_t midnight = kMidnight,
                      bool day = true) {
  wx::Forecast f;
  f.current.time = midnight + 8 * 3600;
  f.current.condition = today;
  f.current.day = day;
  for (int h = 0; h < 24; h++) {
    wx::Hour hour;
    hour.time = midnight + h * 3600;
    hour.temperature = temps[h % temps.size()];
    hour.rain_probability = rain[h % rain.size()];
    hour.condition = hour.rain_probability >= 30 ? hours : Condition::kOvercast;
    f.hours.push_back(hour);
  }
  const float low = *std::min_element(temps.begin(), temps.end());
  const float high = *std::max_element(temps.begin(), temps.end());
  const Condition later[] = {Condition::kPartlyCloudy, Condition::kRain, Condition::kOvercast};
  for (int d = 0; d < 4; d++) {
    wx::Day day_values;
    day_values.time = midnight + d * kDay;
    day_values.low = d == 0 ? low : 6.0f + d;
    day_values.high = d == 0 ? high : 12.0f + d;
    day_values.condition = d == 0 ? today : later[d - 1];
    f.days.push_back(day_values);
  }
  return f;
}

std::vector<float> course(float low, float high) {
  // Low in the early morning, high in the afternoon.
  std::vector<float> t;
  for (int h = 0; h < 24; h++) {
    const double x = (h - 15) / 9.0;
    t.push_back(static_cast<float>(high - (high - low) * std::min(1.0, x * x)));
  }
  return t;
}

std::vector<int> rainFrom(int hour, int pct) {
  std::vector<int> r(24, 0);
  for (int h = hour; h < 24; h++) r[h] = pct;
  return r;
}

std::string text(const wx::Forecast& f, int64_t now, ui::Language lang = ui::Language::kGerman) {
  core::LocalTime t;
  core::toLocalTime(now, t);
  return br::weatherText(br::summarize(f, now), br::greetingFor(t.hour), lang);
}

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

// --- Sentence ---------------------------------------------------------------------

void test_greeting_by_time_of_day() {
  TEST_ASSERT_TRUE(br::greetingFor(5) == br::Greeting::kMorning);
  TEST_ASSERT_TRUE(br::greetingFor(10) == br::Greeting::kMorning);
  TEST_ASSERT_TRUE(br::greetingFor(11) == br::Greeting::kDay);
  TEST_ASSERT_TRUE(br::greetingFor(17) == br::Greeting::kDay);
  TEST_ASSERT_TRUE(br::greetingFor(18) == br::Greeting::kEvening);
  TEST_ASSERT_EQUAL_STRING("Guten Abend.",
                           br::greetingText(br::Greeting::kEvening, ui::Language::kGerman));
  TEST_ASSERT_EQUAL_STRING("Good afternoon.",
                           br::greetingText(br::Greeting::kDay, ui::Language::kEnglish));
}

void test_rain_levels() {
  const auto temps = course(7, 14);
  // Below 30 %: dry.
  TEST_ASSERT_EQUAL_STRING("Heute bleibt es trocken.",
                           text(forecast(temps, rainFrom(14, 29)), kMorning).c_str());
  // 30-69 %: maybe.
  TEST_ASSERT_EQUAL_STRING(
      "Ab 14\u00a0Uhr vielleicht Regen.",
      text(forecast(temps, rainFrom(14, 30), Condition::kRain), kMorning).c_str());
  // From 70 %: rain and the umbrella; the first hour of 70 % counts even if
  // a "maybe" comes earlier.
  std::vector<int> rain = rainFrom(12, 40);
  for (int h = 16; h < 24; h++) rain[h] = 80;
  const wx::Forecast f = forecast(temps, rain, Condition::kRain, Condition::kRain);
  TEST_ASSERT_EQUAL_STRING("Ab 16\u00a0Uhr regnet es – Schirm mit.", text(f, kMorning).c_str());
  TEST_ASSERT_TRUE(br::summarize(f, kMorning).picture == ui::WeatherIcon::kRain);  // umbrella
  // Already raining.
  TEST_ASSERT_EQUAL_STRING("Es regnet – Schirm mit.", text(f, kMidnight + 17 * 3600).c_str());
  // Hours that have passed do not count.
  TEST_ASSERT_EQUAL_STRING(
      "Heute bleibt es trocken.",
      text(forecast(temps, std::vector<int>{90, 90, 90, 90, 90, 90, 0, 0, 0, 0, 0, 0,
                                            0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0}),
           kMorning)
          .c_str());
}

void test_snow_thunder_heat_and_english() {
  const auto cold = course(-4, 1);
  TEST_ASSERT_EQUAL_STRING(
      "Ab 15\u00a0Uhr schneit es.",
      text(forecast(cold, rainFrom(15, 75), Condition::kSnow, Condition::kSnow), kMorning).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "Ab 15\u00a0Uhr vielleicht Schnee.",
      text(forecast(cold, rainFrom(15, 50), Condition::kSnow), kMorning).c_str());
  const auto hot = course(19, 33);
  const wx::Forecast storm = forecast(hot, rainFrom(17, 70), Condition::kThunder);
  TEST_ASSERT_EQUAL_STRING("Ab 17\u00a0Uhr Gewitter – Schirm mit. Heiß, bis 33°.",
                           text(storm, kMorning).c_str());
  TEST_ASSERT_TRUE(br::summarize(storm, kMorning).picture == ui::WeatherIcon::kThunder);
  TEST_ASSERT_EQUAL_STRING("Thunderstorms from 17:00 – take an umbrella. Hot, up to 33°.",
                           text(storm, kMorning, ui::Language::kEnglish).c_str());
  TEST_ASSERT_EQUAL_STRING("It stays dry.", text(forecast(course(7, 14), rainFrom(0, 0)),
                                                 kMidnight + 19 * 3600, ui::Language::kEnglish)
                                                .c_str());
  TEST_ASSERT_EQUAL_STRING(
      "7° bis 14°",
      br::rangeText(br::summarize(forecast(course(7, 14), {0}), kMorning), ui::Language::kGerman)
          .c_str());
}

void test_picture_follows_the_weather_of_the_day() {
  const auto temps = course(7, 14);
  // Rain only possible: no umbrella, a cloud.
  TEST_ASSERT_TRUE(
      br::summarize(forecast(temps, rainFrom(14, 50), Condition::kRain, Condition::kRain), kMorning)
          .picture == ui::WeatherIcon::kCloudy);
  TEST_ASSERT_TRUE(
      br::summarize(forecast(temps, {0}, Condition::kOvercast, Condition::kClear), kMorning)
          .picture == ui::WeatherIcon::kSun);
  // Clear night: the moon.
  TEST_ASSERT_TRUE(
      br::summarize(forecast(temps, {0}, Condition::kOvercast, Condition::kClear, kMidnight, false),
                    kMidnight + 21 * 3600)
          .picture == ui::WeatherIcon::kMoon);
  // Without values for today: nothing.
  TEST_ASSERT_FALSE(br::summarize(forecast(temps, {0}), kMidnight + 4 * kDay + 3600).valid);
  TEST_ASSERT_EQUAL_STRING(
      "", br::weatherText(br::WeatherSummary{}, br::Greeting::kDay, ui::Language::kGerman).c_str());
}

void test_sentence_layout_falls_back_to_the_smaller_size() {
  const br::SentenceLayout a = br::layoutSentence(
      "Guten Morgen. Bis Mittag bleibt es trocken, ab 14 Uhr regnet es – Schirm mit.");
  TEST_ASSERT_FALSE(a.small);
  TEST_ASSERT_EQUAL_size_t(3, a.lines.size());
  const br::SentenceLayout b = br::layoutSentence(
      "Guten Abend. Keine Verbindung – die Angaben sind von 07:12. Ab 17\u00a0Uhr Gewitter "
      "– Schirm mit. Heiß, bis 33°.");
  TEST_ASSERT_TRUE(b.small);
  TEST_ASSERT_TRUE(b.lines.size() <= 3);
}

// --- Weather chart ------------------------------------------------------------------

void checkChart(const wx::Forecast& f, const char* name) {
  const wx::ColumnChart chart = wx::columnChart(f, kMorning, 214);
  TEST_ASSERT_FALSE_MESSAGE(chart.curve.empty(), name);
  int highest = 0;
  for (const auto& b : chart.bars) highest = std::max(highest, b.height);
  TEST_ASSERT_TRUE_MESSAGE(highest <= wx::kColumnRainMax, name);
  for (const auto& p : chart.curve) {
    TEST_ASSERT_TRUE_MESSAGE(p.y >= chart.curve_top, name);
    TEST_ASSERT_TRUE_MESSAGE(p.y <= chart.curve_bottom, name);
    // Never into the bars (2 px line width).
    TEST_ASSERT_TRUE_MESSAGE(p.y + 1 < chart.base - highest, name);
    TEST_ASSERT_TRUE_MESSAGE(p.x >= 0 && p.x < 214, name);
  }
  TEST_ASSERT_EQUAL_size_t_MESSAGE(5, chart.labels.size(), name);
  for (const auto& l : chart.labels) {
    // Label 8 px above the point, at most 12 px high: below the title.
    TEST_ASSERT_TRUE_MESSAGE(l.y - 8 - 12 > ui::kColumnTitleBaseline + 4, name);
  }
}

std::vector<wx::Forecast> extremes() {
  std::vector<int> midday(24, 0);
  for (int h = 10; h < 16; h++) midday[h] = 100;
  return {
      forecast(course(-15, -5), rainFrom(0, 100), Condition::kSnow, Condition::kSnow),    // frost
      forecast(course(30, 38), rainFrom(0, 0), Condition::kOvercast, Condition::kClear),  // heat
      forecast(std::vector<float>(24, 12.4f), rainFrom(0, 50)),                           // flat
      forecast(course(-2, 25), midday, Condition::kThunder, Condition::kThunder),         // jump
  };
}

void test_chart_never_meets_the_rain_bars() {
  const char* names[] = {"frost", "heat", "flat", "jump"};
  const auto list = extremes();
  for (size_t i = 0; i < list.size(); i++) checkChart(list[i], names[i]);
  // A flat day stays flat: 1 degree is at most an eighth of the height.
  const wx::ColumnChart flat =
      wx::columnChart(forecast(course(12, 13), rainFrom(0, 0)), kMorning, 214);
  double top = 1e9, bottom = -1e9;
  for (const auto& p : flat.curve) {
    top = std::min(top, p.y);
    bottom = std::max(bottom, p.y);
  }
  TEST_ASSERT_TRUE(bottom - top <= (flat.curve_bottom - flat.curve_top) / 8.0 + 1);
  // Without rain the curve gets more height than with heavy rain.
  const wx::ColumnChart wet = wx::columnChart(list[0], kMorning, 214);
  TEST_ASSERT_TRUE(flat.curve_bottom > wet.curve_bottom);
}

void test_preview_weather_extremes() {
  const auto list = extremes();
  ui::Bitmap bitmap(4 * 214 + 3 * 20, 259);
  bitmap.clear(ui::Color::kWhite);
  const char* notes[] = {"Frost", "Hitze", "Flach", "Sprung"};
  for (size_t i = 0; i < list.size(); i++) {
    ui::BitmapCanvas column(bitmap, static_cast<int>(i) * 234, 0, 214, 259);
    wx::ColumnOptions options;
    options.note = notes[i];
    options.now_s = kMorning;
    wx::drawWeatherColumn(column, list[i], options);
  }
  check(bitmap, "briefing_weather_extremes");
}

// --- Columns ------------------------------------------------------------------------

cal::Occurrences occurrences(const char* fixture, int64_t now) {
  cal::Parser parser(now - 2 * kDay, now + 10 * kDay);
  parser.feed(readFile(std::string("test/fixtures/calendar/") + fixture));
  parser.finish();
  TEST_ASSERT_TRUE(parser.valid());
  const int64_t today = cal::localDay(now);
  return cal::expandEvents(parser, today, today + 8);
}

void drawCalendar(ui::Bitmap& bitmap, int x, const char* fixture, int64_t now,
                  const char* note = "KW 43", bool alert = false) {
  ui::BitmapCanvas column(bitmap, x, 0, 214, 259);
  cal::ColumnOptions options;
  options.note = note;
  options.note_alert = alert;
  cal::drawAgendaColumn(
      column, cal::buildAgenda(occurrences(fixture, now), now, ui::Language::kGerman), options);
}

void test_preview_calendar_columns() {
  ui::Bitmap bitmap(4 * 214 + 3 * 20, 259);
  bitmap.clear(ui::Color::kWhite);
  drawCalendar(bitmap, 0, "week.ics", kMorning);                // Tuesday morning
  drawCalendar(bitmap, 234, "week.ics", kMorning + 13 * 3600);  // evening: past events stay
  drawCalendar(bitmap, 468, "busy.ics", kMorning);              // today fills the column
  drawCalendar(bitmap, 702, "week.ics", kMorning + 3 * kDay, "Stand 07:41", true);  // Friday
  check(bitmap, "briefing_calendar_columns");
}

std::vector<widgets::mvg::Section> pasing(bool sections) {
  std::ifstream file("test/fixtures/mvg/pasing_20261019.json", std::ios::binary);
  JsonDocument doc;
  widgets::mvg::readDepartures(file, doc);
  JsonDocument config;
  if (sections) {
    deserializeJson(config, R"({"station":"de:09162:10","sections":[
      {"name":"In die Stadt","lines":["S3:H:S","S4:H:S","S5:H:S","S6:H:S","S8:H:S","19:H:T"]},
      {"name":"Raus","lines":["S3:R:S","S4:R:S","S6:R:S","S8:R:S","14:H:T","RB6:H:Z"]}]})");
  } else {
    deserializeJson(config, R"({"station":"de:09162:10"})");
  }
  core::ConfigIssues issues;
  const widgets::mvg::Config c = widgets::mvg::parseConfig(config.as<JsonObjectConst>(), issues);
  return widgets::mvg::departureSections(doc, c, 10);
}

void test_preview_departure_columns() {
  ui::Bitmap bitmap(3 * 264 + 2 * 20, 259);
  bitmap.clear(ui::Color::kWhite);
  widgets::mvg::ColumnOptions options;
  options.station = "Pasing";
  options.now_s = kMvgNow;
  ui::BitmapCanvas a(bitmap, 0, 0, 264, 259);
  TEST_ASSERT_EQUAL_INT(7, widgets::mvg::drawDeparturesColumn(a, pasing(true), options));
  ui::BitmapCanvas b(bitmap, 284, 0, 264, 259);
  TEST_ASSERT_EQUAL_INT(8, widgets::mvg::drawDeparturesColumn(b, pasing(false), options));
  ui::BitmapCanvas c(bitmap, 568, 0, 264, 259);
  options.note = "Stand 22:21";
  options.note_alert = true;
  std::vector<widgets::mvg::Section> empty = pasing(true);
  empty[1].departures.clear();
  widgets::mvg::drawDeparturesColumn(c, empty, options);
  check(bitmap, "briefing_departure_columns");
  TEST_ASSERT_EQUAL_INT(7, widgets::mvg::columnRowsThatFit(259, 2));
  TEST_ASSERT_EQUAL_INT(8, widgets::mvg::columnRowsThatFit(259, 1));
}

// --- Band ---------------------------------------------------------------------------

void test_preview_bands() {
  ui::Bitmap bitmap(800, 3 * 205 + 2 * 10);
  bitmap.clear(ui::Color::kWhite);
  struct Case {
    wx::Forecast forecast;
    int64_t now;
    bool online;
    ui::Language language;
  };
  std::vector<int> rain = rainFrom(14, 40);
  for (int h = 16; h < 24; h++) rain[h] = 85;
  const Case cases[] = {
      // Morning, rain from 16 h.
      {forecast(course(7, 14), rain, Condition::kRain, Condition::kRain), kMorning, true,
       ui::Language::kGerman},
      // Noon, offline, rain possible.
      {forecast(course(9, 16), rainFrom(15, 45), Condition::kRain, Condition::kPartlyCloudy),
       kMidnight + 12 * 3600, false, ui::Language::kGerman},
      // Clear evening in English.
      {forecast(course(4, 11), {0}, Condition::kOvercast, Condition::kClear, kMidnight, false),
       kMidnight + 20 * 3600 + 15 * 60, true, ui::Language::kEnglish},
  };
  for (int i = 0; i < 3; i++) {
    widgets::DrawContext context;
    context.now_s = cases[i].now;
    context.language = cases[i].language;
    context.network_connected = cases[i].online;
    ui::BitmapCanvas canvas(bitmap, 0, i * 215, 800, 205);
    br::drawBand(canvas, context, &cases[i].forecast, "11:48");
  }
  check(bitmap, "briefing_bands");

  widgets::DrawContext context;
  context.now_s = kMorning;
  context.network_connected = true;
  TEST_ASSERT_EQUAL_STRING("Guten Morgen. Ab 16\u00a0Uhr regnet es \u2013 Schirm mit.",
                           br::bandSentence(context, &cases[0].forecast, "").c_str());
  // Without weather data only the greeting; offline with the time.
  TEST_ASSERT_EQUAL_STRING("Guten Morgen.", br::bandSentence(context, nullptr, "").c_str());
  context.network_connected = false;
  TEST_ASSERT_EQUAL_STRING("Guten Morgen. Keine Verbindung \u2013 die Angaben sind von 07:12.",
                           br::bandSentence(context, nullptr, "07:12").c_str());
  context.now_s = 0;  // no time yet
  TEST_ASSERT_EQUAL_STRING("", br::bandSentence(context, nullptr, "").c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_greeting_by_time_of_day);
  RUN_TEST(test_rain_levels);
  RUN_TEST(test_snow_thunder_heat_and_english);
  RUN_TEST(test_picture_follows_the_weather_of_the_day);
  RUN_TEST(test_sentence_layout_falls_back_to_the_smaller_size);
  RUN_TEST(test_chart_never_meets_the_rain_bars);
  RUN_TEST(test_preview_weather_extremes);
  RUN_TEST(test_preview_calendar_columns);
  RUN_TEST(test_preview_departure_columns);
  RUN_TEST(test_preview_bands);
  return UNITY_END();
}
