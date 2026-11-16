// The "Briefing" layout: the rules of the sentence, the adaptive weather
// chart (also with extreme values) and preview images of the band and the
// columns, compared with the references in test/fixtures/ui/ (update them
// with INKTAVLA_UPDATE_PREVIEWS=1).

#include <unity.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/fault_counts.h"
#include "core/log_texts.h"
#include "core/time.h"
#include "hal/png.h"
#include "ui/bitmap.h"
#include "ui/elements.h"
#include "ui/emoji.h"
#include "ui/fonts.h"
#include "ui/layout.h"
#include "ui/text.h"
#include "widgets/briefing/band.h"
#include "widgets/briefing/sentence.h"
#include "widgets/briefing/start.h"
#include "widgets/briefing/system_page.h"
#include "widgets/calendar/agenda.h"
#include "widgets/calendar/column_view.h"
#include "widgets/calendar/parser.h"
#include "widgets/mvg/column_view.h"
#include "widgets/mvg/config.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/sections.h"
#include "widgets/mvg/widget.h"
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
// probabilities, today and the next three days. The precipitation follows the
// probability from 30 % (100 % -> 5 mm, a full bar); the hours have the
// condition `hours` from 30 %, else the one of the day. Daylight from 7 to
// 17 h (the first night hour is 18 h, so tomorrow from 18 h on).
wx::Forecast forecast(const std::vector<float>& temps, const std::vector<int>& rain,
                      Condition hours = Condition::kOvercast,
                      Condition today = Condition::kOvercast, int64_t midnight = kMidnight,
                      bool day = true) {
  wx::Forecast f;
  f.current.time = midnight + 8 * 3600;
  f.current.condition = today;
  f.current.day = day;
  f.current.temperature = temps[8 % temps.size()];
  for (int h = 0; h < 24; h++) {
    wx::Hour hour;
    hour.time = midnight + h * 3600;
    hour.temperature = temps[h % temps.size()];
    hour.rain_probability = rain[h % rain.size()];
    hour.precipitation = hour.rain_probability >= 30 ? hour.rain_probability / 20.0f : 0.0f;
    hour.condition = hour.rain_probability >= 30 ? hours : today;
    hour.day = h >= 7 && h <= 17;
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
  return br::weatherSentence(f, now, lang);
}

// Adds the 24 hours of tomorrow (rain probability and condition per hour).
void addTomorrow(wx::Forecast& f, const std::vector<int>& rain,
                 const std::vector<Condition>& conditions) {
  for (int h = 0; h < 24; h++) {
    wx::Hour hour;
    hour.time = kMidnight + kDay + h * 3600;
    hour.temperature = 10;
    hour.rain_probability = rain[h % rain.size()];
    hour.precipitation = hour.rain_probability >= 30 ? hour.rain_probability / 20.0f : 0.0f;
    hour.condition = conditions[h % conditions.size()];
    f.hours.push_back(hour);
  }
}

// An hour of the day symbol tests.
wx::Hour hour(Condition c, float mm = 0, float sunshine = -1) {
  wx::Hour h;
  h.condition = c;
  h.precipitation = mm;
  h.sunshine = sunshine;
  return h;
}

Condition span(const std::vector<wx::Hour>& hours) {
  std::vector<const wx::Hour*> list;
  for (const wx::Hour& h : hours) list.push_back(&h);
  return wx::spanCondition(list);
}

widgets::mvg::Departure departure(const char* line, const char* destination, int64_t planned,
                                  bool cancelled) {
  widgets::mvg::Departure d;
  d.line = line;
  d.destination = destination;
  d.planned = planned;
  d.cancelled = cancelled;
  d.transport = widgets::mvg::TransportType::kSBahn;
  return d;
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

// Wordings of the morning (before 11 h): the whole day. 2026-10-20 picks
// the first of three wordings in the morning, the second in the afternoon
// and the third in the evening.
void test_morning_sentence() {
  const auto temps = course(7, 14);
  const std::string nb = "\u00a0";
  TEST_ASSERT_EQUAL_STRING(
      ("Heute wird es sonnig, bis" + nb + "14°.").c_str(),
      text(forecast(temps, {0}, Condition::kRain, Condition::kClear), kMorning).c_str());
  // Below 30 %: dry; the hours give the weather word.
  TEST_ASSERT_EQUAL_STRING(("Heute bewölkt, aber trocken, bis" + nb + "14°.").c_str(),
                           text(forecast(temps, rainFrom(14, 29)), kMorning).c_str());
  // 30-69 %: maybe.
  TEST_ASSERT_EQUAL_STRING(
      ("Meist bewölkt, ab 14" + nb + "Uhr vielleicht Regen. Bis" + nb + "14°.").c_str(),
      text(forecast(temps, rainFrom(14, 30), Condition::kRain), kMorning).c_str());
  // From 70 %: rain and the umbrella; the first hour of 70 % counts even if
  // a "maybe" comes earlier.
  std::vector<int> rain = rainFrom(12, 40);
  for (int h = 16; h < 24; h++) rain[h] = 80;
  TEST_ASSERT_EQUAL_STRING(
      ("Erst trocken, ab 16" + nb + "Uhr Regen – Schirm mit. Bis" + nb + "14°.").c_str(),
      text(forecast(temps, rain, Condition::kRain, Condition::kRain), kMorning).c_str());
  // Raining now: all day, or until it stops.
  TEST_ASSERT_EQUAL_STRING(
      ("Heute regnet es den ganzen Tag – Schirm mit. Höchstens" + nb + "14°.").c_str(),
      text(forecast(temps, rainFrom(0, 90), Condition::kRain), kMorning).c_str());
  std::vector<int> until_noon(24, 0);
  for (int h = 0; h < 12; h++) until_noon[h] = 90;
  TEST_ASSERT_EQUAL_STRING(
      ("Es regnet bis etwa 12" + nb + "Uhr – Schirm mit. Bis" + nb + "14°.").c_str(),
      text(forecast(temps, until_noon, Condition::kRain), kMorning).c_str());
  // Hours that have passed do not count.
  std::vector<int> night(24, 0);
  for (int h = 0; h < 6; h++) night[h] = 90;
  TEST_ASSERT_EQUAL_STRING(("Heute bewölkt, aber trocken, bis" + nb + "14°.").c_str(),
                           text(forecast(temps, night), kMorning).c_str());
}

void test_snow_thunder_heat_and_english() {
  const std::string nb = "\u00a0";
  // Snow: the temperature at its start, and the warning of ice.
  TEST_ASSERT_EQUAL_STRING(
      ("Ab 15" + nb + "Uhr schneit es, um" + nb + "1°. Es kann glatt werden.").c_str(),
      text(forecast(course(-4, 1), rainFrom(15, 75), Condition::kSnow, Condition::kSnow), kMorning)
          .c_str());
  const wx::Forecast storm = forecast(course(19, 33), rainFrom(17, 70), Condition::kThunder);
  TEST_ASSERT_EQUAL_STRING(
      ("Ab 17" + nb + "Uhr Gewitter – Schirm mit. Heiß, bis" + nb + "33°.").c_str(),
      text(storm, kMorning).c_str());
  TEST_ASSERT_TRUE(br::summarize(storm, kMorning).picture == ui::WeatherIcon::kThunder);
  TEST_ASSERT_EQUAL_STRING("Thunderstorms from 17:00 – take an umbrella. Hot, up to 33°.",
                           text(storm, kMorning, ui::Language::kEnglish).c_str());
  TEST_ASSERT_EQUAL_STRING(
      ("Sonnig und heiß, bis" + nb + "33°.").c_str(),
      text(forecast(course(19, 33), {0}, Condition::kOvercast, Condition::kClear), kMorning)
          .c_str());
}

// 11-18 h: the afternoon and the evening (temperature at 20 h).
void test_afternoon_sentence() {
  const auto temps = course(7, 14);  // 12° at 20 h
  const std::string nb = "\u00a0";
  const int64_t noon = kMidnight + 13 * 3600;
  TEST_ASSERT_EQUAL_STRING(
      ("Sonnig bis zum Abend, dann" + nb + "12°.").c_str(),
      text(forecast(temps, {0}, Condition::kRain, Condition::kClear), noon).c_str());
  std::vector<int> rain(24, 0);
  for (int h = 16; h < 24; h++) rain[h] = 85;
  TEST_ASSERT_EQUAL_STRING(
      ("Ab 16" + nb + "Uhr regnet es – Schirm mit. Am Abend" + nb + "12°.").c_str(),
      text(forecast(temps, rain, Condition::kRain), noon).c_str());
  std::vector<int> stops(24, 0);
  for (int h = 0; h < 17; h++) stops[h] = 90;
  TEST_ASSERT_EQUAL_STRING(
      ("Es regnet noch bis etwa 17" + nb + "Uhr, danach trocken. Am Abend" + nb + "12°.").c_str(),
      text(forecast(temps, stops, Condition::kRain), noon).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "Maybe rain from 16:00. 12° in the evening.",
      text(forecast(temps, rainFrom(16, 40), Condition::kRain), noon, ui::Language::kEnglish)
          .c_str());
}

// From 18 h: tomorrow.
void test_evening_sentence() {
  const std::string nb = "\u00a0";
  const int64_t evening = kMidnight + 20 * 3600 + 15 * 60;
  // Without hours of tomorrow: the daily code (partly cloudy), 7-13°.
  wx::Forecast f = forecast(course(7, 14), {0});
  TEST_ASSERT_EQUAL_STRING(
      ("Morgen ein trockener Tag mit Sonne und Wolken, bis" + nb + "13°.").c_str(),
      text(f, evening).c_str());
  // A big change of the high.
  f.days[1].high = 5;
  TEST_ASSERT_EQUAL_STRING(
      ("Morgen wird es deutlich kälter: höchstens" + nb + "5°, wolkig.").c_str(),
      text(f, evening).c_str());
  TEST_ASSERT_EQUAL_STRING("Much colder tomorrow: at most 5°, partly cloudy.",
                           text(f, evening, ui::Language::kEnglish).c_str());
  // Fog in the morning (daily code fog), then sun.
  wx::Forecast fog = forecast(course(7, 14), {0});
  fog.days[1].condition = Condition::kFog;
  std::vector<Condition> sky(24, Condition::kClear);
  for (int h = 0; h <= 10; h++) sky[h] = Condition::kFog;
  addTomorrow(fog, {0}, sky);
  TEST_ASSERT_EQUAL_STRING(("Morgen früh Nebel, später sonnig, bis" + nb + "13°.").c_str(),
                           text(fog, evening).c_str());
  // Rain from 10 h (daily code rain: the umbrella in the column).
  wx::Forecast wet = forecast(course(7, 14), {0});
  wet.days[1].condition = Condition::kRain;
  std::vector<int> rain(24, 0);
  for (int h = 10; h < 24; h++) rain[h] = 80;
  addTomorrow(wet, rain, {Condition::kRain});
  TEST_ASSERT_EQUAL_STRING(
      ("Morgen ab 10" + nb + "Uhr Regen – Schirm einpacken. Bis" + nb + "13°.").c_str(),
      text(wet, evening).c_str());
  // The shorter form leaves the temperature out.
  std::string shorter;
  br::weatherSentence(wet, evening, ui::Language::kGerman, &shorter);
  TEST_ASSERT_EQUAL_STRING(("Morgen ab 10" + nb + "Uhr Regen – Schirm einpacken.").c_str(),
                           shorter.c_str());
}

// The symbol of a day from its hours (2026-10-26).
void test_day_symbol() {
  using C = Condition;
  // One hour of a thunderstorm is enough.
  TEST_ASSERT_TRUE(span({hour(C::kClear), hour(C::kThunder, 0.2f)}) == C::kThunder);
  // Precipitation: 1 mm in all, or 3 hours with 0.1 mm or more.
  TEST_ASSERT_TRUE(span({hour(C::kClear), hour(C::kRain, 1.0f), hour(C::kClear)}) == C::kRain);
  TEST_ASSERT_TRUE(span({hour(C::kClear), hour(C::kShowers, 0.4f), hour(C::kShowers, 0.4f),
                         hour(C::kClear), hour(C::kClear)}) == C::kClear);
  TEST_ASSERT_TRUE(span({hour(C::kDrizzle, 0.1f), hour(C::kDrizzle, 0.1f),
                         hour(C::kDrizzle, 0.1f)}) == C::kRain);
  TEST_ASSERT_TRUE(span({hour(C::kSnow, 0.5f), hour(C::kSnow, 0.5f), hour(C::kRain, 0.5f)}) ==
                   C::kSnow);
  // Fog only in at least half of the hours.
  TEST_ASSERT_TRUE(span({hour(C::kFog), hour(C::kFog), hour(C::kClear), hour(C::kClear)}) ==
                   C::kFog);
  TEST_ASSERT_TRUE(span({hour(C::kFog), hour(C::kClear), hour(C::kClear)}) == C::kClear);
  // The most frequent; the cloudier one on a tie.
  TEST_ASSERT_TRUE(span({hour(C::kOvercast), hour(C::kPartlyCloudy)}) == C::kOvercast);
  TEST_ASSERT_TRUE(span({hour(C::kMainlyClear), hour(C::kPartlyCloudy), hour(C::kClear)}) ==
                   C::kPartlyCloudy);
  // Partly cloudy with sunshine in 70 % of the time: clear.
  TEST_ASSERT_TRUE(span({hour(C::kMainlyClear, 0, 3000), hour(C::kPartlyCloudy, 0, 2400)}) ==
                   C::kClear);
  TEST_ASSERT_TRUE(span({hour(C::kMainlyClear, 0, 1800), hour(C::kPartlyCloudy, 0, 2400)}) ==
                   C::kPartlyCloudy);
  TEST_ASSERT_TRUE(span({}) == C::kUnknown);

  // The case that led to the rules (a forecast of 2026-10-27): the daily
  // code is "overcast" (fog in an earlier forecast) because of the night;
  // from 8 to 18 h it is mainly clear. The symbol is the sun.
  wx::Forecast f = forecast(course(6, 16), {0});
  f.hours.clear();
  const int codes[] = {3, 3, 3, 3, 3, 3, 3, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3, 45, 45};
  for (int h = 0; h < 24; h++) {
    wx::Hour x;
    x.time = kMidnight + h * 3600;
    x.condition = wx::conditionFromWmo(codes[h]);
    x.day = h >= 8 && h <= 17;
    x.sunshine = h >= 9 && h <= 17 ? 3600 : 0;
    f.hours.push_back(x);
  }
  f.days[0].condition = Condition::kFog;
  TEST_ASSERT_TRUE(wx::dayCondition(f, f.days[0]) == Condition::kClear);
  // Without the hours of a day: its daily code.
  TEST_ASSERT_TRUE(wx::dayCondition(f, f.days[1]) == f.days[1].condition);
}

// Symbol and sentence follow the same hours and never contradict each other
// (2026-10-28).
void test_symbol_and_sentence_agree() {
  const auto temps = course(7, 14);
  const std::string nb = "\u00a0";
  // Daylight from 8 to 17 h: sun in the morning, cloud from 12 h on. At 13 h
  // the picture and the sentence talk about the rest of the day: cloudy.
  wx::Forecast f = forecast(temps, {0}, Condition::kClear, Condition::kClear);
  for (int h = 0; h < 24; h++) {
    f.hours[h].day = h >= 8 && h <= 17;
    f.hours[h].condition = h < 12 ? Condition::kClear : Condition::kOvercast;
  }
  const int64_t noon = kMidnight + 13 * 3600;
  TEST_ASSERT_TRUE(br::summarize(f, noon).picture == ui::WeatherIcon::kCloudy);
  const std::string afternoon = text(f, noon);
  TEST_ASSERT_TRUE_MESSAGE(afternoon.find("sonn") == std::string::npos, afternoon.c_str());
  TEST_ASSERT_TRUE_MESSAGE(afternoon.find("grau") != std::string::npos ||
                               afternoon.find("ewölkt") != std::string::npos ||
                               afternoon.find("Wolken") != std::string::npos,
                           afternoon.c_str());
  // In the morning both talk about the whole day: 4 clear, 6 cloudy hours.
  TEST_ASSERT_TRUE(br::summarize(f, kMorning).picture == ui::WeatherIcon::kCloudy);
  // Rain by amount (3 hours with 0.4 mm) while every hour has a probability
  // below 30 %: the symbol of the day is rain, so the sentence is not dry.
  wx::Forecast wet = forecast(temps, {0}, Condition::kClear, Condition::kClear);
  for (int h = 0; h < 24; h++) wet.hours[h].day = h >= 8 && h <= 17;
  for (int h = 10; h < 13; h++) {
    wet.hours[h].precipitation = 0.4f;
    wet.hours[h].condition = Condition::kRain;
  }
  wet.days[0].condition = Condition::kClear;
  TEST_ASSERT_TRUE(wx::dayCondition(wet, wet.days[0]) == Condition::kRain);
  TEST_ASSERT_EQUAL_STRING(
      ("Meist bewölkt, ab 10" + nb + "Uhr vielleicht Regen. Bis" + nb + "14°.").c_str(),
      text(wet, kMorning).c_str());
  // Without the wet hours the symbol would be dry and so is the sentence.
  for (int h = 10; h < 13; h++) wet.hours[h].precipitation = 0.0f;
  TEST_ASSERT_TRUE(wx::dayCondition(wet, wet.days[0]) != Condition::kRain);
  TEST_ASSERT_TRUE(text(wet, kMorning).find("Regen") == std::string::npos);
}

// The band picture by daylight (2026-10-28): from sunrise to sunset the
// weather of the rest of the daylight hours, at night the weather of now.
void test_band_picture_by_daylight() {
  const auto temps = course(7, 14);
  // Daylight from 8 to 17 h; clear until 13 h, overcast from 14 h on.
  auto make = [&](bool day_now, Condition now_condition) {
    wx::Forecast f = forecast(temps, {0}, Condition::kClear, now_condition, kMidnight, day_now);
    for (int h = 0; h < 24; h++) {
      f.hours[h].day = h >= 8 && h <= 17;
      f.hours[h].condition = h <= 13 ? Condition::kClear : Condition::kOvercast;
    }
    return f;
  };
  const wx::Forecast day = make(true, Condition::kClear);
  // 9 h: rest of the day has 5 clear and 4 overcast hours; 15 h: overcast only.
  TEST_ASSERT_TRUE(br::summarize(day, kMidnight + 9 * 3600).picture == ui::WeatherIcon::kSun);
  TEST_ASSERT_TRUE(br::summarize(day, kMidnight + 15 * 3600).picture == ui::WeatherIcon::kCloudy);
  // At night (the API says it is no day): the weather of now with the moon,
  // whatever the daylight hours of the day look like.
  const wx::Forecast night = make(false, Condition::kClear);
  TEST_ASSERT_TRUE(br::summarize(night, kMidnight + 6 * 3600).picture == ui::WeatherIcon::kMoon);
  TEST_ASSERT_TRUE(br::summarize(night, kMidnight + 20 * 3600).picture == ui::WeatherIcon::kMoon);
  const wx::Forecast foggy = make(false, Condition::kFog);
  TEST_ASSERT_TRUE(br::summarize(foggy, kMidnight + 20 * 3600).picture == ui::WeatherIcon::kFog);
}

void test_picture_follows_the_weather_of_now() {
  const auto temps = course(7, 14);
  // Rain only possible: no umbrella, a cloud.
  TEST_ASSERT_TRUE(
      br::summarize(forecast(temps, rainFrom(14, 50), Condition::kRain, Condition::kRain), kMorning)
          .picture == ui::WeatherIcon::kCloudy);
  std::vector<int> rain = rainFrom(16, 80);
  TEST_ASSERT_TRUE(br::summarize(forecast(temps, rain, Condition::kRain), kMorning).picture ==
                   ui::WeatherIcon::kRain);  // umbrella
  const wx::Forecast clear = forecast(temps, {0}, Condition::kOvercast, Condition::kClear);
  TEST_ASSERT_TRUE(br::summarize(clear, kMorning).picture == ui::WeatherIcon::kSun);
  TEST_ASSERT_EQUAL_STRING(
      "10°", br::nowText(br::summarize(clear, kMorning), ui::Language::kGerman).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "10°", br::nowText(br::summarize(clear, kMorning), ui::Language::kEnglish).c_str());
  // Clear night: the moon (the weather of now, not of tomorrow).
  TEST_ASSERT_TRUE(
      br::summarize(forecast(temps, {0}, Condition::kOvercast, Condition::kClear, kMidnight, false),
                    kMidnight + 21 * 3600)
          .picture == ui::WeatherIcon::kMoon);
  // Without values for today: nothing.
  TEST_ASSERT_FALSE(br::summarize(forecast(temps, {0}), kMidnight + 4 * kDay + 3600).valid);
  TEST_ASSERT_EQUAL_STRING("", text(forecast(temps, {0}), kMidnight + 4 * kDay + 3600).c_str());
}

void test_sentence_layout_falls_back_to_the_smaller_size() {
  const br::SentenceLayout a = br::layoutSentence(
      "Guten Morgen. Bis Mittag bleibt es trocken, ab 14 Uhr regnet es – Schirm mit.");
  TEST_ASSERT_FALSE(a.small);
  TEST_ASSERT_EQUAL_size_t(3, a.lines.size());
  const br::SentenceLayout b = br::layoutSentence(
      "Guten Abend. Keine Verbindung zum WLAN – die Angaben sind von 07:12. Ab 17\u00a0Uhr "
      "Gewitter "
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

wx::Forecast savedForecast(const char* fixture) {
  std::ifstream file(std::string("test/fixtures/weather/") + fixture, std::ios::binary);
  JsonDocument doc;
  deserializeJson(doc, file, DeserializationOption::Filter(wx::forecastFilter()));
  wx::Forecast f;
  TEST_ASSERT_TRUE(wx::parseForecast(doc, f));
  return f;
}

void test_rain_bar_heights() {
  // Nothing below 0.1 mm.
  TEST_ASSERT_EQUAL_INT(0, wx::rainBarHeight(0, 5));
  TEST_ASSERT_EQUAL_INT(0, wx::rainBarHeight(0.05f, 5));
  TEST_ASSERT_EQUAL_INT(0, wx::rainBarHeight(-1, 5));
  TEST_ASSERT_EQUAL_INT(0, wx::rainBarHeight(NAN, 5));
  // A day reaching 5 mm: the fixed square root scale, full from 5 mm.
  TEST_ASSERT_EQUAL_INT(5, wx::rainBarHeight(0.1f, 5));
  TEST_ASSERT_EQUAL_INT(10, wx::rainBarHeight(0.5f, 5));
  TEST_ASSERT_EQUAL_INT(20, wx::rainBarHeight(2, 5));
  TEST_ASSERT_EQUAL_INT(32, wx::rainBarHeight(5, 5));
  // Little rain: stretched until the wettest hour reaches 2/3 of the height
  // (21 px), at most 1.5 times.
  TEST_ASSERT_EQUAL_INT(21, wx::rainBarHeight(1.1f, 1.1f));
  TEST_ASSERT_EQUAL_INT(6, wx::rainBarHeight(0.1f, 1.1f));
  TEST_ASSERT_EQUAL_INT(12, wx::rainBarHeight(0.3f, 0.3f));  // 1.5 times 8 px
  // Moderate rain already above 2/3: not stretched.
  TEST_ASSERT_EQUAL_INT(25, wx::rainBarHeight(3, 3));
  // Heavy rain: the wettest hour is full, light rain is pressed down.
  TEST_ASSERT_EQUAL_INT(32, wx::rainBarHeight(9, 9));
  TEST_ASSERT_EQUAL_INT(15, wx::rainBarHeight(2, 9));
  TEST_ASSERT_EQUAL_INT(3, wx::rainBarHeight(0.1f, 9));
  // From 10 mm on the scale stays: 10 mm and more are full.
  TEST_ASSERT_EQUAL_INT(32, wx::rainBarHeight(12, 20));
  TEST_ASSERT_EQUAL_INT(30, wx::rainBarHeight(9, 20));
}

void test_rain_bars_between_the_hour_marks() {
  // Rain all day: 18 bars (6-23 h) of the same width with gaps of 2 px, from
  // the left to the right end of the bottom line, centred in the column.
  const struct {
    int width, pitch;
  } widths[] = {{214, 11}, {232, 12}, {240, 12}};
  for (const auto& w : widths) {
    const wx::ColumnChart chart =
        wx::columnChart(forecast(course(8, 12), rainFrom(0, 100)), kMorning, w.width);
    TEST_ASSERT_EQUAL_INT(w.pitch, chart.pitch);
    TEST_ASSERT_EQUAL_size_t(18, chart.bars.size());
    TEST_ASSERT_EQUAL_INT(chart.line_x0, chart.bars.front().x0);
    TEST_ASSERT_EQUAL_INT(chart.line_x1, chart.bars.back().x1);
    TEST_ASSERT_TRUE(chart.line_x0 >= 0 && chart.line_x1 < w.width);
    TEST_ASSERT_INT_WITHIN(1, chart.line_x0, w.width - 1 - chart.line_x1);
    for (size_t i = 0; i < chart.bars.size(); i++) {
      const auto& b = chart.bars[i];
      TEST_ASSERT_EQUAL_INT(w.pitch - 2, b.x1 - b.x0 + 1);
      TEST_ASSERT_EQUAL_INT(wx::kColumnRainMax, b.height);
      if (i > 0) TEST_ASSERT_EQUAL_INT(2, b.x0 - chart.bars[i - 1].x1 - 1);
    }
  }
  // The value of 17 h (rain from 16 to 17 h) stands left of the mark 17.
  std::vector<int> rain(24, 0);
  rain[17] = 100;
  wx::ColumnChart chart = wx::columnChart(forecast(course(8, 12), rain), kMorning, 214);
  TEST_ASSERT_EQUAL_size_t(1, chart.bars.size());
  int mark = -1;
  for (const auto& l : chart.labels) {
    if (l.hour == 17) mark = l.x;
  }
  TEST_ASSERT_EQUAL_INT(mark - chart.pitch + 1, chart.bars[0].x0);
  TEST_ASSERT_EQUAL_INT(mark - 2, chart.bars[0].x1);
  // The value of 5 h (rain from 4 to 5 h) is not shown; dry hours have no
  // bar, but the bottom line stays.
  rain.assign(24, 0);
  rain[5] = 100;
  chart = wx::columnChart(forecast(course(8, 12), rain), kMorning, 214);
  TEST_ASSERT_EQUAL_size_t(0, chart.bars.size());
  TEST_ASSERT_TRUE(chart.line_x1 > chart.line_x0);
  // The real day of steady rain: a bar for every hour from 6 to 18 h, also
  // for 0.1 mm (read as 0.0999999 from the JSON).
  chart = wx::columnChart(savedForecast("muenchen_20261025.json"), 1792910460, 214);
  TEST_ASSERT_EQUAL_size_t(13, chart.bars.size());
  TEST_ASSERT_EQUAL_INT(6, chart.bars[8].height);    // 0.1 mm from 13 to 14 h
  TEST_ASSERT_EQUAL_INT(21, chart.bars[11].height);  // 1.1 mm from 16 to 17 h (2/3)
  // Without data the line still has its place.
  const wx::ColumnChart empty = wx::columnChart(wx::Forecast{}, kMorning, 214);
  TEST_ASSERT_EQUAL_INT(chart.line_x0, empty.line_x0);
  TEST_ASSERT_EQUAL_INT(chart.line_x1, empty.line_x1);
}

void test_chart_full_day() {
  // Step L (2026-11-16): the whole day 0-24 h, labels every four hours
  // (not at 0 and 24 h), the rain of every hour as a bar.
  const wx::Forecast f = forecast(course(4, 12), rainFrom(20, 80));
  const wx::ColumnChart chart = wx::columnChart(f, kMorning, 367, wx::ChartSpan::kFullDay);
  TEST_ASSERT_EQUAL_INT(14, chart.pitch);  // (367 - 12) / 24
  TEST_ASSERT_EQUAL_INT(16, chart.line_x0);
  TEST_ASSERT_EQUAL_INT(15 + 24 * 14 - 2, chart.line_x1);
  TEST_ASSERT_EQUAL_size_t(5, chart.labels.size());
  const int hours[] = {4, 8, 12, 16, 20};
  for (size_t i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL_INT(hours[i], chart.labels[i].hour);
    TEST_ASSERT_EQUAL_INT(15 + hours[i] * 14, chart.labels[i].x);
  }
  // Rain from 20 h: the values of 20-23 h, bars from 19 to 23 h.
  TEST_ASSERT_EQUAL_size_t(4, chart.bars.size());
  TEST_ASSERT_EQUAL_INT(15 + 19 * 14 + 1, chart.bars[0].x0);
  // The day of 5-23 h stays as it was.
  TEST_ASSERT_EQUAL_size_t(5, wx::columnChart(f, kMorning, 367).labels.size());
  TEST_ASSERT_EQUAL_INT(8, wx::columnChart(f, kMorning, 367).labels[0].hour);
}

void test_day_parts() {
  // Step L (V1): tomorrow in three parts from the hourly values. Morning and
  // midday dry and clear, rain from 16 h; the temperature of 8, 13 and 18 h.
  const int64_t tomorrow = kMidnight + kDay;
  const std::vector<float> temps = course(4, 12);
  wx::Forecast f = forecast(temps, rainFrom(16, 80), Condition::kRain, Condition::kClear, tomorrow);
  std::vector<wx::DayPart> parts = wx::dayParts(f, tomorrow + 12 * 3600);
  TEST_ASSERT_EQUAL_size_t(3, parts.size());
  const Condition want[] = {Condition::kClear, Condition::kClear, Condition::kRain};
  const int hours[] = {8, 13, 18};
  for (size_t i = 0; i < 3; i++) {
    TEST_ASSERT_TRUE(parts[i].known);
    TEST_ASSERT_TRUE(want[i] == parts[i].condition);
    TEST_ASSERT_EQUAL_INT(wx::displayTemperature(temps[hours[i]]), parts[i].temperature);
  }
  // Without 8 h: the mean of 6, 7, 9 and 10 h. Midday with one hour only:
  // unknown. No hours of tomorrow at all (the day of the forecast is today):
  // all unknown.
  std::vector<wx::Hour> kept;
  for (const wx::Hour& h : f.hours) {
    const int hour = static_cast<int>((h.time - tomorrow) / 3600);
    if (hour == 8 || (hour >= 11 && hour <= 14)) continue;
    kept.push_back(h);
  }
  f.hours = kept;
  parts = wx::dayParts(f, tomorrow + 12 * 3600);
  const float mean = (temps[6] + temps[7] + temps[9] + temps[10]) / 4;
  TEST_ASSERT_EQUAL_INT(wx::displayTemperature(mean), parts[0].temperature);
  TEST_ASSERT_FALSE(parts[1].known);
  TEST_ASSERT_TRUE(parts[2].known);
  for (const wx::DayPart& p : wx::dayParts(f, tomorrow + kDay + 12 * 3600)) {
    TEST_ASSERT_FALSE(p.known);
  }
}

void test_chart_curve_is_smoothed() {
  // Saved response of 2026-10-22 07:41: the curve runs through the hourly
  // values averaged with their neighbours (1-2-1); the labels show the values
  // of their hours and sit on the curve.
  constexpr int64_t kThursday = 1792647660;
  const wx::Forecast f = savedForecast("muenchen_20261022.json");
  const wx::ColumnChart chart = wx::columnChart(f, kThursday, 214);
  TEST_ASSERT_EQUAL_size_t(18 * 10 + 1, chart.curve.size());  // 5-23 h
  TEST_ASSERT_EQUAL_size_t(5, chart.labels.size());
  for (const auto& l : chart.labels) {
    for (const wx::Hour& h : f.hours) {
      core::LocalTime t;
      core::toLocalTime(h.time, t);
      if (t.day == 22 && t.hour == l.hour) {
        TEST_ASSERT_EQUAL_INT(wx::displayTemperature(h.temperature), l.temperature);
      }
    }
    bool on_curve = false;
    for (const auto& p : chart.curve) {
      on_curve = on_curve || (std::abs(p.x - l.x) <= 1 && std::abs(p.y - l.y) <= 1);
    }
    TEST_ASSERT_TRUE(on_curve);
  }
  // A single warm hour is softened: its point lies a quarter of the way
  // between the neighbours and the peak.
  std::vector<float> temps(24, 10);
  temps[14] = 18;
  const wx::ColumnChart peak = wx::columnChart(forecast(temps, {0}), kMorning, 214);
  for (const auto& l : peak.labels) {
    if (l.hour == 14) TEST_ASSERT_EQUAL_INT(18, l.temperature);
  }
  double top = peak.curve_bottom;
  for (const auto& p : peak.curve) top = std::min(top, p.y);
  TEST_ASSERT_TRUE(top > peak.curve_top + 1);  // not drawn up to the top any more
}

void test_preview_weather_extremes() {
  auto list = extremes();
  // A real day of steady rain (2026-10-25, 07:41): amounts of 0.1-1.1 mm.
  list.push_back(savedForecast("muenchen_20261025.json"));
  ui::Bitmap bitmap(5 * 214 + 4 * 20, 260);
  bitmap.clear(ui::Color::kWhite);
  const char* notes[] = {"Frost", "Hitze", "Flach", "Sprung", "Dauerregen"};
  for (size_t i = 0; i < list.size(); i++) {
    ui::BitmapCanvas column(bitmap, static_cast<int>(i) * 234, 0, 214, 260);
    wx::ColumnOptions options;
    options.place = notes[i];
    options.now_s = i < 4 ? kMorning : 1792910460;
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
                  const char* note = "KW 43", bool alert = false,
                  const cal::DisplayConfig& display = {}) {
  ui::BitmapCanvas column(bitmap, x, 0, 214, 260);
  cal::ColumnOptions options;
  options.note = note;
  options.note_alert = alert;
  options.display = display;
  cal::drawAgendaColumn(
      column, cal::buildAgenda(occurrences(fixture, now), now, ui::Language::kGerman, display),
      options);
}

void test_preview_calendar_columns() {
  ui::Bitmap bitmap(4 * 214 + 3 * 20, 260);
  bitmap.clear(ui::Color::kWhite);
  drawCalendar(bitmap, 0, "week.ics", kMorning);                // Tuesday morning
  drawCalendar(bitmap, 234, "week.ics", kMorning + 13 * 3600);  // evening: past events stay
  drawCalendar(bitmap, 468, "busy.ics", kMorning);              // today fills the column
  drawCalendar(bitmap, 702, "week.ics", kMorning + 3 * kDay, "Stand 07:41", true);  // Friday
  check(bitmap, "briefing_calendar_columns");
}

// Emoji in titles: icons, and the unknown ones left out.
void test_preview_calendar_emoji() {
  ui::Bitmap bitmap(214, 260);
  bitmap.clear(ui::Color::kWhite);
  drawCalendar(bitmap, 0, "emoji.ics", kMorning);
  check(bitmap, "briefing_calendar_emoji");
}

// All emoji icons in three font sizes (event titles, later events, locations).
void test_preview_emoji_icons() {
  std::string all;
  for (size_t i = 0; i < ui::kEmojiIconCount; i++) {
    const uint32_t cp = ui::kEmojiFirstCodepoint + static_cast<uint32_t>(i);
    all += static_cast<char>(0xE0 | (cp >> 12));
    all += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    all += static_cast<char>(0x80 | (cp & 0x3F));
  }
  ui::Bitmap bitmap(660, 3 * 40);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas canvas(bitmap, 0, 0, 660, 3 * 40);
  ui::drawText(canvas, ui::fonts::kFraunces20, 10, 28, all);
  ui::drawText(canvas, ui::fonts::kFraunces18, 10, 68, all);
  ui::drawText(canvas, ui::fonts::kRobotoCondensedMedium15, 10, 108, all);
  check(bitmap, "emoji_icons");
}

// An agenda of today with the given timed events (from 08:00, hourly).
std::vector<cal::AgendaDay> todayWith(const std::vector<std::string>& titles) {
  cal::AgendaDay day;
  day.day = cal::localDay(kMorning);
  int hour = 8;
  for (const std::string& title : titles) {
    cal::Entry e;
    e.time = (hour < 10 ? "0" : "") + std::to_string(hour) + ":00";
    hour++;
    e.title = title;
    day.entries.push_back(e);
  }
  return {day};
}

// Settings of the calendar (2026-11-05): the hint "+ 2 weitere Termine",
// places, single lines, events left out; long titles filled up to "…".
void test_preview_calendar_settings() {
  ui::Bitmap bitmap(5 * 214 + 4 * 20, 260);
  bitmap.clear(ui::Color::kWhite);
  cal::DisplayConfig more;
  more.more = true;
  more.location = true;
  drawCalendar(bitmap, 0, "busy.ics", kMorning, "KW 43", false, more);  // places first
  drawCalendar(bitmap, 234, "week.ics", kMorning, "KW 43", false, more);
  cal::DisplayConfig plain;
  plain.wrap = false;
  plain.all_day = false;
  plain.past = false;
  plain.exclude = {"KINO"};
  drawCalendar(bitmap, 468, "week.ics", kMorning + 308 * 60, "KW 43", false, plain);
  cal::ColumnOptions options;
  options.note = "KW 43";
  ui::BitmapCanvas long_titles(bitmap, 702, 0, 214, 260);
  cal::drawAgendaColumn(
      long_titles,
      todayWith({"Mittagessen mit Jonas und Mareike", "Elterngespräch mit der Klassenlehrerin",
                 "Abstimmung mit dem Vertrieb zur Messeplanung", "Zahnarzt"}),
      options);
  std::vector<std::string> many;
  for (int i = 0; i < 9; i++) many.push_back("Termin " + std::to_string(i + 1));
  options.display.more = true;
  ui::BitmapCanvas hint(bitmap, 936, 0, 214, 260);
  cal::drawAgendaColumn(hint, todayWith(many), options);
  check(bitmap, "briefing_calendar_settings");
}

// The hint counts only events that are still ahead and needs its line.
void test_calendar_more_hint() {
  std::vector<std::string> titles;
  for (int i = 0; i < 9; i++) titles.push_back("Termin " + std::to_string(i + 1));
  cal::ColumnOptions options;
  options.display.more = true;
  ui::Bitmap with(214, 260), without(214, 260);
  ui::BitmapCanvas a(with), b(without);
  cal::drawAgendaColumn(a, todayWith(titles), options);
  options.display.more = false;
  cal::drawAgendaColumn(b, todayWith(titles), options);
  // Without the hint 8 rows fit (37 + 8 * 27 = 253), with it 7 and the hint
  // ("+ 2 weitere Termine") below.
  const auto blackRows = [](const ui::Bitmap& bm, int y0, int y1) {
    int n = 0;
    for (int y = y0; y < y1; y++) {
      for (int x = 0; x < bm.width(); x++) n += bm.get(x, y) == ui::Color::kBlack;
    }
    return n;
  };
  TEST_ASSERT_GREATER_THAN(0, blackRows(without, 37 + 7 * 27, 37 + 8 * 27));
  TEST_ASSERT_EQUAL_INT(0, blackRows(with, 37 + 7 * 27, 37 + 7 * 27 + 4));
  TEST_ASSERT_GREATER_THAN(0, blackRows(with, 37 + 7 * 27 + 4, 37 + 7 * 27 + 20));
  TEST_ASSERT_EQUAL_STRING("+ 2 weitere Termine", ui::moreEvents(ui::Language::kGerman, 2).c_str());
  TEST_ASSERT_EQUAL_STRING("+ 1 more event", ui::moreEvents(ui::Language::kEnglish, 1).c_str());
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
  ui::Bitmap bitmap(3 * 280 + 2 * 20, 260);
  bitmap.clear(ui::Color::kWhite);
  widgets::mvg::ColumnOptions options;
  options.station = "Pasing";
  options.now_s = kMvgNow;
  ui::BitmapCanvas a(bitmap, 0, 0, 280, 260);
  TEST_ASSERT_EQUAL_INT(6, widgets::mvg::drawDeparturesColumn(a, pasing(true), options));
  ui::BitmapCanvas b(bitmap, 300, 0, 280, 260);
  TEST_ASSERT_EQUAL_INT(8, widgets::mvg::drawDeparturesColumn(b, pasing(false), options));
  ui::BitmapCanvas c(bitmap, 600, 0, 280, 260);
  options.note = "Stand 22:21";
  options.note_alert = true;
  std::vector<widgets::mvg::Section> empty = pasing(true);
  empty[1].departures.clear();
  widgets::mvg::drawDeparturesColumn(c, empty, options);
  check(bitmap, "briefing_departure_columns");
  TEST_ASSERT_EQUAL_INT(7, widgets::mvg::columnRowsThatFit(260, 2));
  TEST_ASSERT_EQUAL_INT(8, widgets::mvg::columnRowsThatFit(260, 1));
}

// Saved responses of two stations; counts the requests.
class TwoStations : public widgets::mvg::Source {
 public:
  std::vector<std::string> requests;
  bool fetchDepartures(const widgets::mvg::Config& config, JsonDocument& doc) override {
    requests.push_back(config.station + " " + widgets::mvg::transportTypesParam(config));
    const char* file = config.station == "de:09162:8" ? "donnersbergerbruecke_20261019.json"
                                                      : "pasing_20261019.json";
    std::ifstream in(std::string("test/fixtures/mvg/") + file, std::ios::binary);
    return widgets::mvg::readDepartures(in, doc) == DeserializationError::Ok;
  }
};

widgets::mvg::Config mvgConfig(const char* json) {
  JsonDocument doc;
  deserializeJson(doc, json);
  core::ConfigIssues issues;
  const widgets::mvg::Config c = widgets::mvg::parseConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  return c;
}

// The switch "incidents": false hides only the disruption text of the view;
// the warning signs at the departures stay.
void test_view_incidents_switch() {
  const char* base = R"("station":"de:09162:10","name":"Pasing","sections":[
        {"name":"In die Stadt","lines":["S3:H:S","S4:H:S","S6:H:S","S8:H:S"]},
        {"name":"Raus","lines":["S3:R:S","S4:R:S","S6:R:S","S8:R:S"]}])";
  auto render = [&](const std::string& extra, bool* has_incidents) {
    TwoStations api;
    widgets::mvg::MvgSource source(mvgConfig(("{" + std::string(base) + extra + "}").c_str()), api);
    TEST_ASSERT_TRUE(source.fetch(kMvgNow));
    *has_incidents = false;
    for (const auto& s : source.sections()) {
      for (const auto& d : s.departures) *has_incidents = *has_incidents || !d.incidents.empty();
    }
    ui::Bitmap bitmap(280, 260);
    bitmap.clear(ui::Color::kWhite);
    ui::BitmapCanvas canvas(bitmap, 0, 0, 280, 260);
    widgets::mvg::DeparturesColumnView view(source);
    widgets::DrawContext context;
    context.now_s = kMvgNow;
    view.draw(canvas, context);
    return std::vector<uint8_t>(bitmap.data(), bitmap.data() + bitmap.size());
  };
  bool on_has = false, off_has = false;
  const auto on = render("", &on_has);
  const auto off = render(R"(,"incidents":false)", &off_has);
  TEST_ASSERT_TRUE(on_has);
  TEST_ASSERT_TRUE(off_has);  // data is still fetched, only drawing is switched off
  TEST_ASSERT_TRUE(on != off);

  // Off looks like a column without any disruption message.
  TwoStations api;
  widgets::mvg::MvgSource source(mvgConfig(("{" + std::string(base) + "}").c_str()), api);
  TEST_ASSERT_TRUE(source.fetch(kMvgNow));
  ui::Bitmap bitmap(280, 260);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas canvas(bitmap, 0, 0, 280, 260);
  widgets::mvg::ColumnOptions options;
  options.station = "Pasing";
  options.now_s = kMvgNow;
  widgets::mvg::drawDeparturesColumn(canvas, source.sections(), options);
  TEST_ASSERT_TRUE(off == std::vector<uint8_t>(bitmap.data(), bitmap.data() + bitmap.size()));
  TEST_ASSERT_FALSE(options.show_incidents);  // the default draws no text
}

// Only the first message is shown; the others as "+2 weitere". The lines
// stand in front only if the column shows more than one line.
void test_incident_label() {
  const std::vector<widgets::mvg::Incident> three = {
      {"Reparatur an einem Signal", {"S2"}},
      {"Verspätung eines vorausfahrenden Zuges.", {"S2", "S3"}},
      {"Technische Störung am Zug", {"S3"}}};
  auto label = widgets::mvg::incidentLabel(three, true, ui::Language::kGerman);
  TEST_ASSERT_EQUAL_STRING("S2: Reparatur an einem Signal.", label.text.c_str());
  TEST_ASSERT_EQUAL_STRING("+2 weitere", label.more.c_str());
  label = widgets::mvg::incidentLabel(three, false, ui::Language::kEnglish);
  TEST_ASSERT_EQUAL_STRING("Reparatur an einem Signal.", label.text.c_str());
  TEST_ASSERT_EQUAL_STRING("+2 more", label.more.c_str());
  label = widgets::mvg::incidentLabel({three[1]}, true, ui::Language::kGerman);
  TEST_ASSERT_EQUAL_STRING("S2, S3: Verspätung eines vorausfahrenden Zuges.", label.text.c_str());
  TEST_ASSERT_EQUAL_STRING("", label.more.c_str());
  label = widgets::mvg::incidentLabel({three[0], three[2]}, true, ui::Language::kGerman);
  TEST_ASSERT_EQUAL_STRING("+1 weitere", label.more.c_str());
  TEST_ASSERT_EQUAL_STRING(
      "", widgets::mvg::incidentLabel({}, true, ui::Language::kGerman).text.c_str());
}

void test_preview_departure_incidents() {
  ui::Bitmap bitmap(3 * 280 + 2 * 20, 260);
  bitmap.clear(ui::Color::kWhite);
  widgets::mvg::ColumnOptions options;
  options.station = "Pasing";
  options.now_s = kMvgNow;
  options.show_incidents = true;

  // Two directions with the disruption messages of the departures drawn.
  TwoStations api;
  widgets::mvg::MvgSource directions(mvgConfig(R"({"station":"de:09162:10","sections":[
        {"name":"In die Stadt","lines":["S3:H:S","S4:H:S","S6:H:S","S8:H:S"]},
        {"name":"Raus","lines":["S3:R:S","S4:R:S","S6:R:S","S8:R:S"]}]})"),
                                     api);
  TEST_ASSERT_TRUE(directions.fetch(kMvgNow));
  TEST_ASSERT_EQUAL_size_t(1, api.requests.size());
  std::vector<std::string> text;
  ui::BitmapCanvas a(bitmap, 0, 0, 280, 260);
  TEST_ASSERT_EQUAL_INT(
      4, widgets::mvg::drawDeparturesColumn(a, directions.sections(), options, &text));
  TEST_ASSERT_EQUAL_size_t(2, text.size());

  // Two stations: one request each, the sections in the configured order.
  api.requests.clear();
  widgets::mvg::MvgSource stations(mvgConfig(R"({"station":"de:09162:10","sections":[
        {"name":"Pasing","lines":["S3:H:S","S4:H:S"],"note":"Bahnhof"},
        {"name":"Donnersbergerbr.","station":"de:09162:8","note":"4 min zu Fuß"}]})"),
                                   api);
  TEST_ASSERT_TRUE(stations.fetch(kMvgNow));
  TEST_ASSERT_EQUAL_size_t(2, api.requests.size());
  TEST_ASSERT_EQUAL_STRING("de:09162:10 SBAHN", api.requests[0].c_str());
  TEST_ASSERT_EQUAL_STRING("de:09162:8 SBAHN,UBAHN,TRAM,BUS,REGIONAL_BUS,BAHN",
                           api.requests[1].c_str());
  TEST_ASSERT_EQUAL_STRING("Pasing", stations.sections()[0].name.c_str());
  TEST_ASSERT_TRUE(stations.sections()[0].own_station);
  TEST_ASSERT_TRUE(stations.sections()[1].own_station);
  TEST_ASSERT_FALSE(directions.sections()[0].own_station);
  options.show_incidents = false;
  ui::BitmapCanvas b(bitmap, 300, 0, 280, 260);
  TEST_ASSERT_EQUAL_INT(7, widgets::mvg::drawDeparturesColumn(b, stations.sections(), options));

  // One list with the messages of its departures, in English; the age badge
  // stays on the title.
  options.show_incidents = true;
  options.language = ui::Language::kEnglish;
  options.note = "Stand 22:21";
  options.note_alert = true;
  ui::BitmapCanvas c(bitmap, 600, 0, 280, 260);
  TEST_ASSERT_EQUAL_INT(6, widgets::mvg::drawDeparturesColumn(c, pasing(false), options, &text));
  TEST_ASSERT_EQUAL_size_t(2, text.size());
  check(bitmap, "briefing_departure_incidents");

  // A failed request keeps the old data.
  class Failing : public widgets::mvg::Source {
   public:
    bool fetchDepartures(const widgets::mvg::Config&, JsonDocument&) override { return false; }
  } failing;
  widgets::mvg::MvgSource none(mvgConfig(R"({"station":"de:09162:10"})"), failing);
  TEST_ASSERT_FALSE(none.fetch(kMvgNow));
  TEST_ASSERT_TRUE(none.sections().empty());
}

// Hand-written response (test/fixtures/mvg/rules_incidents.json, times from
// 2026-10-19 05:00 UTC like the other rules_*.json), one minute before the
// first departure.
constexpr int64_t kIncidentsNow = 1792386000 - 60;

class IncidentsFixture : public widgets::mvg::Source {
 public:
  bool fetchDepartures(const widgets::mvg::Config&, JsonDocument& doc) override {
    std::ifstream in("test/fixtures/mvg/rules_incidents.json", std::ios::binary);
    return widgets::mvg::readDepartures(in, doc) == DeserializationError::Ok;
  }
};

// The disruption text is made from the departures that are drawn, not from
// everything the API sent (2026-10-28: three messages above two trains).
void test_incidents_only_of_drawn_departures() {
  IncidentsFixture api;
  widgets::mvg::MvgSource source(mvgConfig(R"({"station":"de:09162:10","sections":[
        {"name":"Zentrum","lines":["S2:H:S"]},{"name":"Auswaerts","lines":["S2:R:S"]}]})"),
                                 api);
  TEST_ASSERT_TRUE(source.fetch(kIncidentsNow));
  // The response has three different texts for S2, in all departures ...
  std::vector<widgets::mvg::Incident> all;
  for (const auto& s : source.sections()) {
    for (const auto& d : s.departures) {
      for (const auto& m : d.incidents) widgets::mvg::mergeIncidents(all, {{m.message, {m.line}}});
    }
  }
  TEST_ASSERT_EQUAL_size_t(3, all.size());

  // ... but with three and two rows only two of them are on the display: the
  // first one and "+1 weitere".
  widgets::mvg::ColumnOptions options;
  options.station = "Test";
  options.now_s = kIncidentsNow;
  options.show_incidents = true;
  ui::Bitmap bitmap(280, 260);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas canvas(bitmap, 0, 0, 280, 260);
  std::vector<std::string> text;
  const int drawn = widgets::mvg::drawDeparturesColumn(canvas, source.sections(), options, &text);
  TEST_ASSERT_EQUAL_INT(5, drawn);
  TEST_ASSERT_EQUAL_size_t(1, text.size());
  TEST_ASSERT_EQUAL_STRING("defektes Stellwerk. +1 weitere", text[0].c_str());  // no "S2: "

  // Fewer rows (a lower column): only the first message, nothing else.
  ui::Bitmap low(280, 214);
  low.clear(ui::Color::kWhite);
  ui::BitmapCanvas low_canvas(low, 0, 0, 280, 214);
  widgets::mvg::drawDeparturesColumn(low_canvas, source.sections(), options, &text);
  TEST_ASSERT_EQUAL_size_t(1, text.size());
  TEST_ASSERT_EQUAL_STRING("defektes Stellwerk.", text[0].c_str());

  // More rows (a taller column): the further messages are counted.
  ui::Bitmap tall(280, 400);
  tall.clear(ui::Color::kWhite);
  ui::BitmapCanvas tall_canvas(tall, 0, 0, 280, 400);
  widgets::mvg::drawDeparturesColumn(tall_canvas, source.sections(), options, &text);
  TEST_ASSERT_EQUAL_size_t(1, text.size());
  TEST_ASSERT_EQUAL_STRING("defektes Stellwerk. +2 weitere", text[0].c_str());
}

// A long message above two sections gets two lines of text and every
// section keeps two departures (2026-10-28: the text must not be cut to one
// line while the display has the room).
void test_incident_text_two_lines_with_two_departures_each() {
  const std::string message =
      "Wegen einer Stellwerksstörung kommt es zu Verspätungen und Ausfällen auf der gesamten "
      "Strecke zwischen Petershausen und München Hbf";
  auto sections = [&](bool own_stations) {
    std::vector<widgets::mvg::Section> result(2);
    result[0].name = "Zentrum";
    result[1].name = "Auswärts";
    for (auto& s : result) {
      s.own_station = own_stations;
      for (int i = 0; i < 6; i++) {
        widgets::mvg::Departure d;
        d.line = "S2";
        d.destination = "Dachau";
        d.planned = kIncidentsNow + 600 * i;
        d.incidents = {{message, "S2"}};
        s.departures.push_back(d);
      }
    }
    return result;
  };
  widgets::mvg::ColumnOptions options;
  options.station = "Test";
  options.now_s = kIncidentsNow;
  options.show_incidents = true;
  for (const bool own_stations : {false, true}) {
    ui::Bitmap bitmap(280, 260);
    bitmap.clear(ui::Color::kWhite);
    ui::BitmapCanvas canvas(bitmap, 0, 0, 280, 260);
    std::vector<std::string> text;
    const int drawn =
        widgets::mvg::drawDeparturesColumn(canvas, sections(own_stations), options, &text);
    TEST_ASSERT_EQUAL_size_t(2, text.size());
    TEST_ASSERT_TRUE(drawn >= 4);
  }
  // One list: three lines of text, and still more than two rows.
  std::vector<widgets::mvg::Section> list(1);
  list[0] = sections(false)[0];
  list[0].name.clear();
  ui::Bitmap bitmap(280, 260);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas canvas(bitmap, 0, 0, 280, 260);
  std::vector<std::string> text;
  const int drawn = widgets::mvg::drawDeparturesColumn(canvas, list, options, &text);
  TEST_ASSERT_EQUAL_size_t(3, text.size());
  TEST_ASSERT_TRUE(drawn >= 4);
}

// Departures of the made-up cases below: `incident` on the departures with
// a "!" in the pattern, "x" = cancelled.
widgets::mvg::Section incidentSection(const char* name, bool own_station, const char* line,
                                      const char* destination, const char* pattern,
                                      const std::string& incident) {
  widgets::mvg::Section s;
  s.name = name;
  s.own_station = own_station;
  int minute = 0;
  for (const char* p = pattern; *p != '\0'; p++, minute += 10) {
    widgets::mvg::Departure d;
    d.line = line;
    d.destination = destination;
    d.planned = kIncidentsNow + 60 + minute * 60;
    d.transport = widgets::mvg::TransportType::kSBahn;
    d.cancelled = *p == 'x';
    d.warning = *p == 'x' || *p == '!';
    if (d.warning) d.incidents = {{incident, line}};
    s.departures.push_back(d);
  }
  return s;
}

// Long messages: pin line on top and the text below it (two directions and
// two stations), hyphenation, "+1 weitere", the cancellation sign before the
// time (2026-10-28).
void test_incident_variants() {
  const std::string long_message =
      "Wegen einer Stellwerksstörung kommt es zu Verspätungen und Ausfällen";
  ui::Bitmap bitmap(3 * 280 + 2 * 20, 260);
  bitmap.clear(ui::Color::kWhite);
  widgets::mvg::ColumnOptions options;
  options.station = "Karlsfeld";
  options.now_s = kIncidentsNow;
  options.show_incidents = true;
  std::vector<std::string> text;

  // Two directions of one line: no prefix; a second message: "+1 weitere".
  std::vector<widgets::mvg::Section> directions = {
      incidentSection("München", false, "S2", "Markt Schwaben", "x!..", long_message),
      incidentSection("Dachau", false, "S2", "Petershausen", "x!..", long_message)};
  directions[1].departures[1].incidents = {{"Bauarbeiten", "S2"}};
  ui::BitmapCanvas a(bitmap, 0, 0, 280, 260);
  TEST_ASSERT_EQUAL_INT(4, widgets::mvg::drawDeparturesColumn(a, directions, options, &text));
  TEST_ASSERT_EQUAL_size_t(2, text.size());
  TEST_ASSERT_TRUE(text[0].rfind("S2:", 0) == std::string::npos);
  TEST_ASSERT_TRUE(text[1].size() > 10 && text[1].find(" +1 weitere") == text[1].size() - 11);

  // Two stations: the text stands below the pin title of the first one.
  std::vector<widgets::mvg::Section> stations = {
      incidentSection("Karlsfeld", true, "S2", "Dachau", "x!..", long_message),
      incidentSection("Dachau Bf", true, "S2", "München", "!...", long_message)};
  ui::BitmapCanvas b(bitmap, 300, 0, 280, 260);
  TEST_ASSERT_TRUE(widgets::mvg::drawDeparturesColumn(b, stations, options, &text) >= 4);
  TEST_ASSERT_EQUAL_size_t(2, text.size());

  // One list with two lines: the line in front of the message.
  widgets::mvg::Section list = incidentSection("", false, "S2", "Dachau", "!x..", long_message);
  list.departures[2].line = "S3";
  list.departures[3].line = "S3";
  ui::BitmapCanvas c(bitmap, 600, 0, 280, 260);
  widgets::mvg::drawDeparturesColumn(c, {list}, options, &text);
  TEST_ASSERT_TRUE(text[0].rfind("S2: ", 0) == 0);
  check(bitmap, "briefing_departure_incidents_long");
}

// --- System page ------------------------------------------------------------------------

namespace {

constexpr int64_t kSystemNow = 1793947330;  // 2026-11-06 07:42:10 CET

// A morning with a short Wi-Fi dropout and a calendar address that stopped
// working, after a planned restart the evening before.
std::vector<core::LogEntry> systemLogSample() {
  std::vector<core::LogEntry> entries;
  uint32_t seq = 0;
  auto add = [&](uint16_t boot, int64_t wall, uint32_t up, core::LogLevel level, const char* area,
                 const char* text) {
    core::LogEntry e;
    e.wall_s = wall;
    e.uptime_s = up;
    e.boot = boot;
    e.seq = ++seq;
    e.level = level;
    std::snprintf(e.area, sizeof(e.area), "%s", area);
    std::snprintf(e.text, sizeof(e.text), "%s", text);
    entries.push_back(e);
  };
  using L = core::LogLevel;
  const int64_t t = kSystemNow;
  add(2, t - 50000, 30000, L::kInfo, "system", "restarting (settings saved)");
  add(3, 0, 0, L::kInfo, "system", "start 0.3.0-dev, reason: software restart");
  add(3, 0, 4, L::kInfo, "wifi", core::wifiConnectedText(4, -61, 6).c_str());
  add(3, t - 49990, 5, L::kInfo, "time", "clock set (NTP)");
  add(3, t - 3600, 2600, L::kInfo, "mvg", core::fetchSummaryText(60, 0, 800, 1400).c_str());
  add(3, t - 3600, 2600, L::kInfo, "memory",
      core::memoryText(183 * 1024, 141 * 1024, 96 * 1024, 7602 * 1024).c_str());
  add(3, t - 1490, 48500, L::kWarning, "wifi", "disconnected, reason 200 (BEACON_TIMEOUT)");
  add(3, t - 1481, 48509, L::kInfo, "wifi", core::wifiConnectedText(9, -64, 6).c_str());
  add(3, t - 1430, 48560, L::kWarning, "calendar", "fetch failed: HTTP 404 (1.2 s)");
  add(3, t - 1200, 48790, L::kInfo, "keys", "KEY1: refresh");
  add(3, t - 1190, 48800, L::kInfo, "calendar", "works again after 1 failed attempt (0:04:00)");
  add(3, t - 730, 49260, L::kWarning, "calendar", "fetch failed: HTTP 404 (1.1 s)");
  add(3, t - 600, 49390, L::kInfo, "mvg", core::fetchSummaryText(59, 1, 900, 3100).c_str());
  add(3, t - 600, 49390, L::kInfo, "weather", core::fetchSummaryText(2, 0, 1600, 1700).c_str());
  add(3, t - 600, 49390, L::kInfo, "calendar", core::fetchSummaryText(2, 2, 2800, 3000).c_str());
  add(3, t - 600, 49390, L::kInfo, "memory",
      core::memoryText(179 * 1024, 138 * 1024, 92 * 1024, 7598 * 1024).c_str());
  add(3, t - 600, 49390, L::kInfo, "wifi", core::wifiSignalText(-78).c_str());
  add(3, t - 140, 49850, L::kError, "calendar", "failing for 30 min (3 attempts): HTTP 404");
  add(3, t - 5, 49985, L::kInfo, "keys", "KEY2: system page (waited 2.4 s)");
  std::reverse(entries.begin(), entries.end());  // newest first
  return entries;
}

core::SystemReport systemReportSample() {
  core::SystemReport r;
  core::DeviceInfo& d = r.device;
  d.firmware = "0.3.0-dev";
  d.uptime_s = 49990;
  d.restart_reason = "software restart";
  d.boot = 3;
  d.wifi_connected = true;
  d.rssi_dbm = -78;  // weak: no warning sign while connected
  d.heap_free = 179 * 1024;
  d.heap_min_free = 138 * 1024;
  d.heap_largest = 92 * 1024;
  d.psram_free = 7598 * 1024;
  d.psram_size = 8 * 1024 * 1024;
  d.time_valid = true;
  core::SourceReport mvg;
  mvg.name = "mvg";
  mvg.state = core::DataState::kFresh;
  mvg.last_success_wall_s = kSystemNow - 9;
  mvg.last_duration_ms = 850;
  mvg.successes = 830;
  mvg.failures = 3;
  mvg.fails = 2;
  mvg.fails_24h = 1;
  mvg.last_failure_wall_s = kSystemNow - 2400;
  mvg.problem = "timed out after 0 bytes";
  core::SourceReport weather;
  weather.name = "weather";
  weather.state = core::DataState::kFresh;
  weather.last_success_wall_s = kSystemNow - 700;
  weather.last_duration_ms = 1620;
  weather.successes = 28;
  core::SourceReport calendar;
  calendar.name = "calendar";
  calendar.state = core::DataState::kStale;
  calendar.last_success_wall_s = kSystemNow - 1190;
  calendar.last_duration_ms = 1100;
  calendar.successes = 54;
  calendar.failures = 4;
  calendar.fails = 1;
  calendar.fails_24h = 1;
  calendar.failures_in_row = 3;
  calendar.last_failure_wall_s = kSystemNow - 130;
  calendar.problem = "HTTP 404";
  r.sources = {mvg, weather, calendar};
  r.wifi_drops = 1;
  r.wifi_drops_24h = 1;
  r.warnings = 3;
  r.errors = 1;
  return r;
}

}  // namespace

// The system page (KEY2, always English) with a morning of a Wi-Fi dropout,
// a weak signal and a failing calendar address, and right after a crash
// without a clock.
void test_preview_system_page() {
  const std::vector<core::LogEntry> log = systemLogSample();
  const core::SystemReport report = systemReportSample();
  ui::Bitmap bitmap(800, 2 * 480 + 10);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas gap(bitmap, 0, 480, 800, 10);
  gap.fillDotted(0, 0, 800, 10);
  ui::BitmapCanvas morning(bitmap, 0, 0, 800, 480);
  const size_t lines = widgets::briefing::systemPageLogLines();
  TEST_ASSERT_EQUAL(23, lines);
  widgets::briefing::drawSystemPage(
      morning, report,
      std::vector<core::LogEntry>(log.begin(), log.begin() + std::min(log.size(), lines)),
      kSystemNow);

  // After a crash: the lines before it came back from the RTC memory, the
  // clock is not set yet, no data yet.
  core::SystemReport crash = report;
  crash.device.uptime_s = 12;
  crash.device.restart_reason = "crash (panic)";
  crash.device.restart_unplanned = true;
  crash.device.wifi_connected = false;
  crash.device.time_valid = false;
  crash.device.heap_min_free = crash.device.heap_free;
  // The fails of the last 24 hours came back from the RTC memory too.
  for (core::SourceReport& s : crash.sources) {
    const uint32_t day = s.fails_24h;
    s = {s.name};
    s.fails_24h = day;
  }
  crash.wifi_drops = 0;
  crash.warnings = 1;
  crash.errors = 1;
  std::vector<core::LogEntry> after;
  core::LogEntry e;
  e.boot = 4;
  e.level = core::LogLevel::kError;
  e.uptime_s = 0;
  std::snprintf(e.area, sizeof(e.area), "system");
  std::snprintf(e.text, sizeof(e.text), "start 0.3.0-dev, reason: crash (panic)");
  after.push_back(e);
  for (const core::LogEntry& old : log) {
    if (old.level != core::LogLevel::kInfo || std::string(old.area) == "system") {
      after.push_back(old);
    }
  }
  ui::BitmapCanvas crashed(bitmap, 0, 490, 800, 480);
  widgets::briefing::drawSystemPage(crashed, crash, after, 0);
  check(bitmap, "system_page");
}

// The longest form of every log text with numbers fits a line of the log on
// the system page, and the Wi-Fi rows fit the overview (Befund 2026-11-06:
// "verbunden · -78 dBm (schwach)" with a warning sign was cut off).
void test_system_page_longest_texts_fit() {
  const int log_width = widgets::briefing::systemPageLogTextWidth();
  const int value_width = widgets::briefing::systemPageValueWidth();
  TEST_ASSERT_GREATER_THAN(250, log_width);
  const ui::Font& log_font = ui::fonts::kRobotoCondensedMedium15;
  const ui::Font& value_font = ui::fonts::kRobotoCondensedBold16;
  const std::vector<std::string> log_lines = {
      core::startText("0.10.0-rc.1", "USB (flashing or reset)"),
      core::startText("0.10.0-rc.1", "voltage drop (brownout)"),
      core::wifiConnectedText(86400 + 86399, -100, 13),
      core::wifiSignalText(-100),
      core::wifiSignalText(-30),
      core::wifiRestartText(99),
      // Internal heap and PSRAM of the ESP32-S3 (512 KB, 8 MB) at most.
      core::memoryText(512 * 1024, 512 * 1024, 512 * 1024, 8192 * 1024),
      // At most one success a minute and one failure every 10 s.
      core::fetchSummaryText(360, 360, 99900, 99900),
      "fetch stopped for a button after " + core::secondsText(99900),
      "slow fetch: " + core::secondsText(99900),
      "KEY2: system page (waited 99.9 s)",
  };
  for (const std::string& line : log_lines) {
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(log_width, ui::textWidth(log_font, line), line.c_str());
  }
  const std::vector<std::string> values = {"connected", "not connected", "-100 dBm (excellent)",
                                           "-100 dBm (weak)", "voltage drop (brownout)"};
  for (const std::string& value : values) {
    // Without a warning sign; "not connected" has one (about 20 px).
    const int extra = value == "not connected" ? 20 : 0;
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(value_width, ui::textWidth(value_font, value) + extra,
                                      value.c_str());
  }
  // The fail and drop counters fit (a fail a minute for a day: 1440).
  for (const char* label : {"Fails", "Drops"}) {
    const std::string line = core::faultCountText(label, 99999, 9999);
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(
        value_width, ui::textWidth(ui::fonts::kRobotoCondensedMedium15, line), line.c_str());
  }
  // Three sources, all with a failure line, end above "KEY2: back".
  const int key2_top = 480 - ui::kMarginBottom - ui::fonts::kRobotoCondensedMedium16.cap_height;
  TEST_ASSERT_LESS_OR_EQUAL(key2_top - 8, widgets::briefing::systemPageSourcesBottom(3) + 4);
}

// --- Band ---------------------------------------------------------------------------

// Start screen: loading the data (German), and waiting for a saved Wi-Fi
// with a long name (English).
void test_preview_start_screen() {
  ui::Bitmap bitmap(800, 2 * 480 + 10);
  bitmap.clear(ui::Color::kWhite);
  ui::BitmapCanvas gap(bitmap, 0, 480, 800, 10);
  gap.fillDotted(0, 0, 800, 10);
  ui::BitmapCanvas loading(bitmap, 0, 0, 800, 480);
  widgets::briefing::drawStartScreen(loading, ui::Language::kGerman, {"0.1.0", "Heimnetz"}, false);
  ui::BitmapCanvas waiting(bitmap, 0, 490, 800, 480);
  widgets::briefing::drawStartScreen(
      waiting, ui::Language::kEnglish,
      {"0.1.0",
       "A very long network name that goes on and on and on and on and on and on "
       "and on and on and on and on and on and on and on and on"},
      true);
  check(bitmap, "start_screen");
}

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
    br::BandData data;
    data.forecast = &cases[i].forecast;
    data.as_of = "11:48";
    br::drawBand(canvas, context, data);
  }
  check(bitmap, "briefing_bands");

  widgets::DrawContext context;
  context.now_s = kMorning;
  context.network_connected = true;
  br::BandData data;
  data.forecast = &cases[0].forecast;
  TEST_ASSERT_EQUAL_STRING(
      "Guten Morgen. Erst trocken, ab 16\u00a0Uhr Regen \u2013 Schirm mit. Bis\u00a014°.",
      br::bandSentence(context, data).c_str());
  // Without weather data only the greeting; offline with the time.
  TEST_ASSERT_EQUAL_STRING("Guten Morgen.", br::bandSentence(context, br::BandData{}).c_str());
  context.network_connected = false;
  br::BandData offline;
  offline.as_of = "07:12";
  TEST_ASSERT_EQUAL_STRING(
      "Guten Morgen. Keine Verbindung zum WLAN \u2013 die Angaben sind von 07:12.",
      br::bandSentence(context, offline).c_str());
  context.now_s = 0;  // no time yet
  TEST_ASSERT_EQUAL_STRING("", br::bandSentence(context, br::BandData{}).c_str());
}

void test_sentence_parts() {
  std::vector<int> rain = rainFrom(16, 80);
  const wx::Forecast wet = forecast(course(7, 14), rain, Condition::kRain, Condition::kRain);
  std::vector<widgets::mvg::Section> sections(2);
  sections[0].departures = {departure("S2", "Dachau", kMorning + 12 * 60, true),
                            departure("S2", "Dachau", kMorning + 2 * 3600, true)};
  sections[1].departures = {departure("S2", "Erding", kMorning + 7 * 60, false)};

  br::SentenceInput in;
  in.now_s = kMorning;
  in.forecast = &wet;
  in.departures = &sections;
  in.online = false;
  in.as_of = "07:12";
  // Offline, the cancellation and the weather, in this order.
  std::vector<br::Part> parts = br::sentenceParts(in);
  TEST_ASSERT_EQUAL_size_t(3, parts.size());
  TEST_ASSERT_TRUE(parts[0].topic == br::Topic::kOffline);
  TEST_ASSERT_TRUE(parts[1].topic == br::Topic::kCancellation);
  TEST_ASSERT_TRUE(parts[2].topic == br::Topic::kWeather);
  TEST_ASSERT_EQUAL_STRING("Die S2 um\u00a007:54 nach Dachau fällt aus.", parts[1].text.c_str());
  // The long offline notice would leave no room for the cancellation; its
  // shorter form makes room (the weather still does not fit).
  std::string used;
  const br::SentenceLayout layout = br::layoutParts("Guten Morgen.", parts, &used);
  TEST_ASSERT_TRUE(layout.lines.size() <= 3);
  TEST_ASSERT_EQUAL_STRING(
      "Guten Morgen. Kein WLAN – Stand 07:12. Die S2 um\u00a007:54 nach Dachau fällt aus.",
      used.c_str());
  // Too long for three lines: the weather in its shorter form.
  in.online = true;
  br::layoutParts("Guten Morgen.", br::sentenceParts(in), &used);
  TEST_ASSERT_EQUAL_STRING(
      "Guten Morgen. Die S2 um\u00a007:54 nach Dachau fällt aus. Erst trocken, ab 16\u00a0Uhr "
      "Regen – Schirm mit. Bis\u00a014°.",
      used.c_str());
  // A part that does not fit even in its shorter form is left out.
  const std::vector<br::Part> skip = {
      {br::Topic::kOffline, "Kein WLAN seit 07:12.", ""},
      {br::Topic::kCancellation, "Die S2 um\u00a007:54 nach Dachau fällt aus.", ""},
      {br::Topic::kWeather,
       "Heute regnet es den ganzen Tag und am Abend kommt noch ein Gewitter dazu.",
       "Heute regnet es den ganzen Tag und abends gibt es Gewitter."}};
  br::layoutParts("Guten Morgen.", skip, &used);
  TEST_ASSERT_EQUAL_STRING(
      "Guten Morgen. Kein WLAN seit 07:12. Die S2 um\u00a007:54 nach Dachau fällt aus.",
      used.c_str());
}

void test_cancellations() {
  const auto de = ui::Language::kGerman;
  std::vector<widgets::mvg::Section> sections(2);
  // Among the next two departures of each section (direction) for itself
  // (2026-10-28): two trains of the other direction come first, the
  // cancellation is still said.
  sections[0].departures = {departure("S2", "Dachau", kMorning + 20 * 60, true)};
  sections[1].departures = {departure("S2", "Erding", kMorning + 7 * 60, false),
                            departure("S2", "Markt Schwaben", kMorning + 12 * 60, false)};
  TEST_ASSERT_EQUAL_STRING("Die S2 um\u00a008:02 nach Dachau fällt aus.",
                           br::cancellationText(sections, kMorning, de).c_str());
  // Not the third departure of a section.
  sections[0].departures = {departure("S2", "Dachau", kMorning + 2 * 60, false),
                            departure("S2", "Dachau", kMorning + 10 * 60, false),
                            departure("S2", "Dachau", kMorning + 20 * 60, true)};
  TEST_ASSERT_EQUAL_STRING("", br::cancellationText(sections, kMorning, de).c_str());
  // At most one: the earliest of all sections.
  sections[0].departures = {departure("S2", "Dachau", kMorning + 20 * 60, true)};
  sections[1].departures = {departure("S2", "Erding", kMorning + 5 * 60, true)};
  TEST_ASSERT_EQUAL_STRING(
      "The S2 at\u00a007:47 to Erding is cancelled.",
      br::cancellationText(sections, kMorning, ui::Language::kEnglish).c_str());
  // Not more than an hour ahead, and not after it has left.
  sections[0].departures = {departure("S2", "Dachau", kMorning + 2 * 3600, true)};
  sections[1].departures = {departure("S2", "Erding", kMorning - 60, true)};
  TEST_ASSERT_EQUAL_STRING("", br::cancellationText(sections, kMorning, de).c_str());
}

// Widgets whose updates fail: named after the offline part's rank, only with
// Wi-Fi (2026-10-28).
void test_failed_updates() {
  const auto de = ui::Language::kGerman;
  using S = br::Source;
  TEST_ASSERT_EQUAL_STRING("", br::failureText({}, de).c_str());
  TEST_ASSERT_EQUAL_STRING("Die Abfahrten lassen sich gerade nicht abrufen – Stand 07:41.",
                           br::failureText({{S::kDepartures, "07:41"}}, de).c_str());
  TEST_ASSERT_EQUAL_STRING("Das Wetter lässt sich gerade nicht abrufen.",
                           br::failureText({{S::kWeather, ""}}, de).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "Wetter, Abfahrten und Kalender lassen sich gerade nicht abrufen.",
      br::failureText({{S::kWeather, ""}, {S::kDepartures, "07:41"}, {S::kCalendar, ""}}, de)
          .c_str());
  TEST_ASSERT_EQUAL_STRING(
      "Weather and calendar cannot be loaded right now.",
      br::failureText({{S::kWeather, ""}, {S::kCalendar, ""}}, ui::Language::kEnglish).c_str());
  // A wrong calendar address.
  TEST_ASSERT_EQUAL_STRING("Der Kalender lässt sich nicht abrufen – bitte die Adresse prüfen.",
                           br::failureText({{S::kCalendar, "", true}}, de).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "The calendar cannot be loaded – please check its address.",
      br::failureText({{S::kCalendar, "", true}}, ui::Language::kEnglish).c_str());
  br::SentenceInput in;
  in.now_s = kMorning;
  in.failed = {{S::kDepartures, "07:41"}};
  std::vector<br::Part> parts = br::sentenceParts(in);
  TEST_ASSERT_EQUAL_size_t(1, parts.size());
  TEST_ASSERT_TRUE(parts[0].topic == br::Topic::kFailure);
  // Offline: only the Wi-Fi.
  in.online = false;
  in.as_of = "07:12";
  parts = br::sentenceParts(in);
  TEST_ASSERT_EQUAL_size_t(1, parts.size());
  TEST_ASSERT_EQUAL_STRING("Keine Verbindung zum WLAN – die Angaben sind von 07:12.",
                           parts[0].text.c_str());
}

// Tomorrow from the first full hour after sunset, between 18 and 21 h
// (2026-10-28).
void test_evening_start_by_sunset() {
  auto withNight = [](int first_night_hour, int64_t midnight = kMidnight) {
    wx::Forecast f = forecast(course(7, 14), {0}, Condition::kClear, Condition::kClear, midnight);
    for (int h = 0; h < 24; h++) f.hours[h].day = h >= 7 && h < first_night_hour;
    return f;
  };
  TEST_ASSERT_EQUAL_INT(18, br::eveningStartHour(withNight(17), kMorning));  // winter: 16:20
  TEST_ASSERT_EQUAL_INT(20, br::eveningStartHour(withNight(20), kMorning));  // September
  TEST_ASSERT_EQUAL_INT(21, br::eveningStartHour(withNight(22), kMorning));  // June: 21:15
  wx::Forecast polar = withNight(24);
  TEST_ASSERT_EQUAL_INT(21, br::eveningStartHour(polar, kMorning));
  // Without hours of today: 18 h in winter time, 19 h in summer time
  // (2026-10-20 is summer time; 2026-11-20 winter time).
  wx::Forecast none = withNight(18);
  none.hours.clear();
  TEST_ASSERT_EQUAL_INT(19, br::eveningStartHour(none, kMorning));
  TEST_ASSERT_EQUAL_INT(18, br::eveningStartHour(none, kMorning + 31 * kDay));
  // In June at 20:15 the sentence is still about the evening, at 21 h about
  // tomorrow.
  const wx::Forecast june = withNight(22);
  TEST_ASSERT_TRUE(br::weatherPeriod(june, kMidnight + 20 * 3600 + 15 * 60) == br::Greeting::kDay);
  TEST_ASSERT_TRUE(br::weatherPeriod(june, kMidnight + 21 * 3600) == br::Greeting::kEvening);
  TEST_ASSERT_TRUE(br::weatherPeriod(june, kMidnight + 10 * 3600) == br::Greeting::kMorning);
}

// After sunset and before tomorrow: the evening, with the sky of now like the
// band picture (2026-10-28).
void test_evening_after_sunset() {
  const std::string nb = "\u00a0";
  // Sunny afternoon, clear night from 17 h; at 17:30 the picture is the moon.
  wx::Forecast f =
      forecast(course(4, 11), {0}, Condition::kClear, Condition::kClear, kMidnight, false);
  for (int h = 0; h < 24; h++) f.hours[h].day = h >= 7 && h <= 16;
  const int64_t dusk = kMidnight + 17 * 3600 + 30 * 60;
  TEST_ASSERT_EQUAL_INT(18, br::eveningStartHour(f, dusk));
  TEST_ASSERT_TRUE(br::summarize(f, dusk).picture == ui::WeatherIcon::kMoon);
  const std::string t = text(f, dusk);
  TEST_ASSERT_TRUE_MESSAGE(t.find("Abend") != std::string::npos, t.c_str());
  TEST_ASSERT_TRUE_MESSAGE(t.find("Nachmittag") == std::string::npos, t.c_str());
  TEST_ASSERT_TRUE_MESSAGE(t.find("sonnig") == std::string::npos, t.c_str());
  // Fog now: a foggy evening, as the picture.
  f.current.condition = Condition::kFog;
  TEST_ASSERT_TRUE(br::summarize(f, dusk).picture == ui::WeatherIcon::kFog);
  TEST_ASSERT_TRUE_MESSAGE(text(f, dusk).find("eblig") != std::string::npos, text(f, dusk).c_str());
}

// Tomorrow: the weather word of the daily code (the symbol of the column),
// the times from the hours (2026-10-28).
void test_tomorrow_follows_the_daily_code() {
  const std::string nb = "\u00a0";
  const int64_t evening = kMidnight + 20 * 3600 + 15 * 60;
  // Fog in the daily code, only at night in the hours: "zeitweise".
  wx::Forecast fog = forecast(course(7, 14), {0});
  fog.days[1].condition = Condition::kFog;
  std::vector<Condition> sky(24, Condition::kClear);
  sky[22] = sky[23] = Condition::kFog;
  addTomorrow(fog, {0}, sky);
  for (size_t i = 24; i < fog.hours.size(); i++) {
    const int h = static_cast<int>(i) - 24;
    fog.hours[i].day = h >= 7 && h <= 17;
  }
  TEST_ASSERT_EQUAL_STRING(("Morgen zeitweise neblig, sonst trocken, bis" + nb + "13°.").c_str(),
                           text(fog, evening).c_str());
  // Sunny code with dense morning fog in the hours: the code wins, no fog.
  wx::Forecast clear = forecast(course(7, 14), {0});
  clear.days[1].condition = Condition::kClear;
  std::vector<Condition> morning_fog(24, Condition::kClear);
  for (int h = 0; h <= 10; h++) morning_fog[h] = Condition::kFog;
  addTomorrow(clear, {0}, morning_fog);
  TEST_ASSERT_TRUE(text(clear, evening).find("Nebel") == std::string::npos);
  TEST_ASSERT_TRUE(text(clear, evening).find("sonn") != std::string::npos ||
                   text(clear, evening).find("Sonne") != std::string::npos);
  // Likely rain in the hours, but a dry code (cloud in the column): no
  // umbrella, only "vielleicht".
  wx::Forecast wet = forecast(course(7, 14), {0});
  wet.days[1].condition = Condition::kOvercast;
  std::vector<int> rain(24, 0);
  for (int h = 10; h < 24; h++) rain[h] = 80;
  addTomorrow(wet, rain, {Condition::kOvercast});
  TEST_ASSERT_EQUAL_STRING(
      ("Morgen ab 10" + nb + "Uhr vielleicht Regen, bis" + nb + "13°.").c_str(),
      text(wet, evening).c_str());
  // Rain in the code, but not from 6 to 22 h: rain "zeitweise".
  wx::Forecast night_rain = forecast(course(7, 14), {0});
  night_rain.days[1].condition = Condition::kRain;
  addTomorrow(night_rain, {0}, {Condition::kOvercast});
  TEST_ASSERT_EQUAL_STRING(("Morgen zeitweise Regen, sonst trocken, bis" + nb + "13°.").c_str(),
                           text(night_rain, evening).c_str());
}

// The three wordings of an event (2026-11-16, V1); a full stop unless the
// title ends with "…".
std::vector<std::string> eventForms(bool timed, bool tomorrow, bool english,
                                    const std::string& title, const std::string& time = "") {
  const std::string at = english ? "at " + time : "um " + time;
  std::vector<std::string> f;
  if (!english) {
    if (timed && !tomorrow)
      f = {"Um " + time + " " + title, "Heute " + at + ": " + title,
           "Als Nächstes " + at + ": " + title};
    if (timed && tomorrow)
      f = {"Morgen " + at + " " + title, "Morgen als Erstes " + at + ": " + title,
           "Für morgen vorgemerkt: " + time + " " + title};
    if (!timed && !tomorrow)
      f = {"Heute: " + title, "Heute ganztägig: " + title, "Den ganzen Tag: " + title};
    if (!timed && tomorrow)
      f = {"Morgen: " + title, "Morgen ganztägig: " + title, "Morgen den ganzen Tag: " + title};
  } else {
    if (timed && !tomorrow)
      f = {title + " " + at, "Next up " + at + ": " + title, "Coming up " + at + ": " + title};
    if (timed && tomorrow)
      f = {"Tomorrow " + at + ": " + title, title + " tomorrow " + at,
           "First thing tomorrow, " + time + ": " + title};
    if (!timed && !tomorrow)
      f = {"Today: " + title, "All day today: " + title, "On the calendar today: " + title};
    if (!timed && tomorrow)
      f = {"Tomorrow: " + title, "All day tomorrow: " + title, "Coming up tomorrow: " + title};
  }
  const std::string ellipsis = "…";
  for (std::string& t : f) {
    if (t.size() < ellipsis.size() ||
        t.compare(t.size() - ellipsis.size(), ellipsis.size(), ellipsis) != 0)
      t += ".";
  }
  return f;
}

void assertOneOf(const std::string& text, const std::vector<std::string>& forms) {
  TEST_ASSERT_TRUE_MESSAGE(std::find(forms.begin(), forms.end(), text) != forms.end(),
                           text.c_str());
}

void test_event_sentence() {
  // The next event of today that has not begun; in the evening the first
  // one of tomorrow (2026-11-16). The wording changes (see
  // test_event_wordings); here: which event.
  const cal::Occurrences week = occurrences("week.ics", kMorning);
  const cal::DisplayConfig display;
  const auto text = [&](int64_t now, bool tomorrow) {
    return br::eventText(week, display, now, tomorrow, ui::Language::kGerman);
  };
  assertOneOf(text(kMorning, false), eventForms(true, false, false, "Team-Standup", "08:30"));
  assertOneOf(text(kMidnight + 14 * 3600, false),
              eventForms(true, false, false, "Zahnarzt", "17:15"));
  // The dentist has begun and nothing else follows today: nothing.
  TEST_ASSERT_EQUAL_STRING("", text(kMidnight + 17 * 3600 + 30 * 60, false).c_str());
  // Tomorrow: the timed event before the all-day one ("Papiertonne").
  assertOneOf(text(kMidnight + 20 * 3600, true),
              eventForms(true, true, false, "Kino mit Lena", "18:30"));
  // Only an all-day event on the day.
  assertOneOf(text(kMorning + 4 * kDay, false), eventForms(false, false, false, "Geburtstag Oma"));
  assertOneOf(text(kMidnight + 3 * kDay + 20 * 3600, true),
              eventForms(false, true, false, "Geburtstag Oma"));
  assertOneOf(br::eventText(week, display, kMidnight + 20 * 3600, true, ui::Language::kEnglish),
              eventForms(true, true, true, "Kino mit Lena", "18:30"));
  // The settings of the column apply: words left out, no all-day events.
  cal::DisplayConfig without = display;
  without.exclude = {"STANDUP"};  // stored in capitals
  assertOneOf(br::eventText(week, without, kMorning, false, ui::Language::kGerman),
              eventForms(true, false, false, "Mittagessen mit Jonas", "12:30"));
  without.all_day = false;
  TEST_ASSERT_EQUAL_STRING(
      "", br::eventText(week, without, kMorning + 4 * kDay, false, ui::Language::kGerman).c_str());

  // An event of several days is named on its first day only.
  cal::Occurrences trip(1);
  trip[0].all_day = true;
  trip[0].first_day = cal::localDay(kMorning) - 1;
  trip[0].last_day = trip[0].first_day + 3;
  trip[0].start = kMidnight - kDay;
  trip[0].end = kMidnight + 3 * kDay;
  trip[0].summary = "Urlaub";
  TEST_ASSERT_EQUAL_STRING(
      "", br::eventText(trip, display, kMorning, false, ui::Language::kGerman).c_str());
  assertOneOf(br::eventText(trip, display, kMorning - kDay, false, ui::Language::kGerman),
              eventForms(false, false, false, "Urlaub"));

  // In the band: last part, after the weather; off with the switch.
  widgets::DrawContext context;
  context.network_connected = true;
  context.now_s = kMorning;
  const wx::Forecast dry = forecast(course(7, 14), rainFrom(0, 0));
  br::BandData data;
  data.forecast = &dry;
  data.events = &week;
  const std::string sentence = br::bandSentence(context, data);
  TEST_ASSERT_TRUE(sentence.size() > 30);
  TEST_ASSERT_EQUAL_size_t(sentence.size() - std::strlen("Um\u00a008:30 Team-Standup."),
                           sentence.find("Um\u00a008:30 Team-Standup."));
  data.event_sentence = false;
  TEST_ASSERT_EQUAL_size_t(std::string::npos, br::bandSentence(context, data).find("Team-Standup"));
}

void test_event_wordings() {
  // Every wording once (2026-11-16, V1): the choice moves on by one every
  // day, so three days in a row give all three; each has a shorter form
  // with the title cut to 260 px; German and English.
  const std::string title = "Jahresgespräch mit der Abteilungsleitung über die Planung";
  const cal::DisplayConfig display;
  for (int english = 0; english < 2; english++) {
    const ui::Language lang = english ? ui::Language::kEnglish : ui::Language::kGerman;
    const std::string cut = "Jahresgespr\u2026";
    for (int timed = 0; timed < 2; timed++) {
      for (int tomorrow = 0; tomorrow < 2; tomorrow++) {
        std::vector<std::string> seen, shorter_seen;
        for (int day = 0; day < 3; day++) {
          const int64_t midnight = kMidnight + day * kDay;
          // Now: 07:42, or 20:00 for tomorrow's event.
          const int64_t now = midnight + (tomorrow ? 20 * 3600 : 7 * 3600 + 42 * 60);
          const int64_t event_day = midnight + (tomorrow ? kDay : 0);
          cal::Occurrences e(1);
          e[0].summary = title;
          e[0].all_day = !timed;
          e[0].first_day = e[0].last_day = cal::localDay(event_day + 12 * 3600);
          e[0].start = timed ? event_day + 9 * 3600 + 15 * 60 : event_day;
          e[0].end = timed ? e[0].start + 3600 : event_day + kDay;
          std::string shorter;
          seen.push_back(br::eventText(e, display, now, tomorrow, lang, &shorter));
          shorter_seen.push_back(shorter);
        }
        std::vector<std::string> want = eventForms(timed, tomorrow, english, title, "09:15");
        std::vector<std::string> want_short = eventForms(timed, tomorrow, english, cut, "09:15");
        for (size_t i = 0; i < 3; i++) {
          assertOneOf(seen[i], want);
          // The shorter form belongs to the same wording.
          const size_t index = std::find(want.begin(), want.end(), seen[i]) - want.begin();
          TEST_ASSERT_EQUAL_STRING(want_short[index].c_str(), shorter_seen[i].c_str());
        }
        std::sort(seen.begin(), seen.end());
        std::sort(want.begin(), want.end());
        TEST_ASSERT_TRUE_MESSAGE(seen == want, want[0].c_str());
      }
    }
  }
  // English: the title first ("Kino mit Lena at 18:30.").
  cal::Occurrences cinema(1);
  cinema[0].summary = "Kino mit Lena";
  cinema[0].start = kMidnight + 18 * 3600 + 30 * 60;
  cinema[0].end = cinema[0].start + 7200;
  cinema[0].first_day = cinema[0].last_day = cal::localDay(kMorning);
  bool title_first = false;
  for (int day = 0; day < 3; day++) {
    cinema[0].start = kMidnight + day * kDay + 18 * 3600 + 30 * 60;
    cinema[0].end = cinema[0].start + 7200;
    cinema[0].first_day = cinema[0].last_day = cal::localDay(kMorning + day * kDay);
    title_first |= br::eventText(cinema, display, kMorning + day * kDay, false,
                                 ui::Language::kEnglish) == "Kino mit Lena at 18:30.";
  }
  TEST_ASSERT_TRUE(title_first);
}

void test_wordings_change_every_day() {
  // The weather sentence too (2026-11-16: with three wordings the same one
  // was chosen every day): the same sunny morning one day later.
  const wx::Forecast sunny =
      forecast(course(7, 14), rainFrom(0, 0), Condition::kClear, Condition::kClear);
  wx::Forecast next = sunny;
  for (wx::Hour& h : next.hours) h.time += kDay;
  for (wx::Day& d : next.days) d.time += kDay;
  next.current.time += kDay;
  const std::string a = br::weatherSentence(sunny, kMorning, ui::Language::kGerman);
  const std::string b = br::weatherSentence(next, kMorning + kDay, ui::Language::kGerman);
  TEST_ASSERT_TRUE_MESSAGE(a != b, a.c_str());
}

void test_preview_band_switches() {
  // The band switches of the widgets: the weather without its sentence
  // (the picture stays), the departures without their cancellations, and a
  // placeholder where the weather is not there at all.
  ui::Bitmap bitmap(800, 3 * 205 + 2 * 10);
  bitmap.clear(ui::Color::kWhite);
  const wx::Forecast wet =
      forecast(course(7, 14), rainFrom(14, 80), Condition::kRain, Condition::kRain);
  std::vector<widgets::mvg::Section> sections(1);
  sections[0].departures = {departure("S2", "Dachau", kMorning + 12 * 60, true)};
  widgets::DrawContext context;
  context.network_connected = true;
  context.now_s = kMorning;

  br::BandData no_sentence;  // weather switch off: greeting, cancellation, picture
  no_sentence.forecast = &wet;
  no_sentence.weather_sentence = false;
  no_sentence.departures = &sections;
  ui::BitmapCanvas first(bitmap, 0, 0, 800, 205);
  br::drawBand(first, context, no_sentence);
  const std::string with_cancellation = br::bandSentence(context, no_sentence);
  TEST_ASSERT_NOT_NULL(std::strstr(with_cancellation.c_str(), "S2"));
  TEST_ASSERT_NULL(std::strstr(with_cancellation.c_str(), "Regen"));

  br::BandData no_cancellations = no_sentence;  // departures switch off: weather only
  no_cancellations.weather_sentence = true;
  no_cancellations.departures_sentence = false;
  ui::BitmapCanvas second(bitmap, 0, 215, 800, 205);
  br::drawBand(second, context, no_cancellations);
  TEST_ASSERT_NULL(std::strstr(br::bandSentence(context, no_cancellations).c_str(), "S2"));
  TEST_ASSERT_NOT_NULL(std::strstr(br::bandSentence(context, no_cancellations).c_str(), "Regen"));

  br::BandData missing;  // no weather on the screen: the placeholder
  missing.weather_missing = true;
  ui::BitmapCanvas third(bitmap, 0, 430, 800, 205);
  br::drawBand(third, context, missing);
  TEST_ASSERT_EQUAL_STRING("Guten Morgen.", br::bandSentence(context, missing).c_str());
  check(bitmap, "briefing_band_switches");
}

void test_preview_band_topics() {
  ui::Bitmap bitmap(800, 3 * 205 + 2 * 10);
  bitmap.clear(ui::Color::kWhite);
  std::vector<int> rain = rainFrom(14, 80);
  const wx::Forecast wet = forecast(course(7, 14), rain, Condition::kRain, Condition::kRain);
  const wx::Forecast dry = forecast(course(4, 11), {0}, Condition::kOvercast, Condition::kClear);
  wx::Forecast cold = forecast(course(9, 16), {0}, Condition::kOvercast, Condition::kPartlyCloudy,
                               kMidnight, false);
  cold.days[1].high = 5;
  std::vector<widgets::mvg::Section> sections(1);
  sections[0].departures = {departure("S2", "Dachau", kMorning + 12 * 60, true)};

  widgets::DrawContext context;
  context.network_connected = true;
  // Morning: rain from 14 h.
  br::BandData a;
  a.forecast = &wet;
  context.now_s = kMorning;
  ui::BitmapCanvas first(bitmap, 0, 0, 800, 205);
  br::drawBand(first, context, a);
  // Morning: a cancellation, then the dry day.
  br::BandData b;
  b.forecast = &dry;
  b.departures = &sections;
  ui::BitmapCanvas second(bitmap, 0, 215, 800, 205);
  br::drawBand(second, context, b);
  // Evening in English: much colder tomorrow.
  br::BandData c;
  c.forecast = &cold;
  context.now_s = kMidnight + 20 * 3600;
  context.language = ui::Language::kEnglish;
  ui::BitmapCanvas third(bitmap, 0, 430, 800, 205);
  br::drawBand(third, context, c);
  check(bitmap, "briefing_band_topics");
  TEST_ASSERT_EQUAL_STRING("Good evening. Much colder tomorrow: at most 5°, partly cloudy.",
                           br::bandSentence(context, c).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_greeting_by_time_of_day);
  RUN_TEST(test_morning_sentence);
  RUN_TEST(test_snow_thunder_heat_and_english);
  RUN_TEST(test_afternoon_sentence);
  RUN_TEST(test_evening_sentence);
  RUN_TEST(test_day_symbol);
  RUN_TEST(test_symbol_and_sentence_agree);
  RUN_TEST(test_band_picture_by_daylight);
  RUN_TEST(test_event_sentence);
  RUN_TEST(test_event_wordings);
  RUN_TEST(test_wordings_change_every_day);
  RUN_TEST(test_preview_band_switches);
  RUN_TEST(test_picture_follows_the_weather_of_now);
  RUN_TEST(test_sentence_layout_falls_back_to_the_smaller_size);
  RUN_TEST(test_sentence_parts);
  RUN_TEST(test_cancellations);
  RUN_TEST(test_failed_updates);
  RUN_TEST(test_evening_start_by_sunset);
  RUN_TEST(test_evening_after_sunset);
  RUN_TEST(test_tomorrow_follows_the_daily_code);
  RUN_TEST(test_chart_never_meets_the_rain_bars);
  RUN_TEST(test_chart_full_day);
  RUN_TEST(test_day_parts);
  RUN_TEST(test_chart_curve_is_smoothed);
  RUN_TEST(test_rain_bar_heights);
  RUN_TEST(test_rain_bars_between_the_hour_marks);
  RUN_TEST(test_preview_weather_extremes);
  RUN_TEST(test_preview_calendar_columns);
  RUN_TEST(test_preview_calendar_emoji);
  RUN_TEST(test_preview_emoji_icons);
  RUN_TEST(test_preview_calendar_settings);
  RUN_TEST(test_calendar_more_hint);
  RUN_TEST(test_preview_departure_columns);
  RUN_TEST(test_view_incidents_switch);
  RUN_TEST(test_incident_label);
  RUN_TEST(test_preview_departure_incidents);
  RUN_TEST(test_incident_variants);
  RUN_TEST(test_incidents_only_of_drawn_departures);
  RUN_TEST(test_incident_text_two_lines_with_two_departures_each);
  RUN_TEST(test_preview_start_screen);
  RUN_TEST(test_preview_system_page);
  RUN_TEST(test_system_page_longest_texts_fit);
  RUN_TEST(test_preview_bands);
  RUN_TEST(test_preview_band_topics);
  return UNITY_END();
}
