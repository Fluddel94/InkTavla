// The "Briefing" layout: the rules of the sentence, the adaptive weather
// chart (also with extreme values) and preview images of the band and the
// columns, compared with the references in test/fixtures/ui/ (update them
// with INKTAVLA_UPDATE_PREVIEWS=1).

#include <unity.h>

#include <algorithm>
#include <cmath>
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

// An event of today or a later day (hour < 0: all day).
cal::Occurrence event(int day, int hour, int minute, const char* title) {
  cal::Occurrence o;
  const int64_t midnight = kMidnight + day * kDay;
  o.all_day = hour < 0;
  o.start = o.all_day ? midnight : midnight + hour * 3600 + minute * 60;
  o.end = o.all_day ? midnight + kDay : o.start + 3600;
  o.first_day = cal::localDay(o.start);
  o.last_day = o.first_day;
  o.summary = title;
  return o;
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

// Saved response of 2026-10-22 07:30 with values every 15 minutes.
wx::Forecast quarterForecast() {
  std::ifstream file("test/fixtures/weather/muenchen_20261022.json", std::ios::binary);
  JsonDocument doc;
  deserializeJson(doc, file, DeserializationOption::Filter(wx::forecastFilter()));
  wx::Forecast f;
  TEST_ASSERT_TRUE(wx::parseForecast(doc, f));
  return f;
}

void test_chart_from_values_every_15_minutes() {
  constexpr int64_t kThursday = 1792647660;  // 2026-10-22 07:41
  const wx::Forecast f = quarterForecast();
  wx::Forecast hourly = f;
  hourly.quarters = {};
  const wx::ColumnChart fine = wx::columnChart(f, kThursday, 214);
  const wx::ColumnChart coarse = wx::columnChart(hourly, kThursday, 214);
  // 5-23 h: 73 values every 15 minutes instead of 19 hourly ones.
  TEST_ASSERT_EQUAL_size_t(72 * 3 + 1, fine.curve.size());
  TEST_ASSERT_EQUAL_size_t(18 * 10 + 1, coarse.curve.size());
  TEST_ASSERT_EQUAL_size_t(5, fine.labels.size());
  for (size_t i = 0; i < fine.labels.size(); i++) {
    const auto& l = fine.labels[i];
    // Same values on the full hour, and every label sits on the curve.
    TEST_ASSERT_EQUAL_INT(coarse.labels[i].temperature, l.temperature);
    bool on_curve = false;
    for (const auto& p : fine.curve) {
      on_curve = on_curve || (std::abs(p.x - l.x) <= 1 && std::abs(p.y - l.y) <= 1);
    }
    TEST_ASSERT_TRUE(on_curve);
  }
  // A gap in the 15 minute values: hourly values for the whole day.
  wx::Forecast gap = f;
  gap.quarters.temperatures[100] = NAN;
  TEST_ASSERT_EQUAL_size_t(coarse.curve.size(), wx::columnChart(gap, kThursday, 214).curve.size());
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

void test_preview_departure_incidents() {
  ui::Bitmap bitmap(3 * 264 + 2 * 20, 259);
  bitmap.clear(ui::Color::kWhite);
  widgets::mvg::ColumnOptions options;
  options.station = "Pasing";
  options.now_s = kMvgNow;

  // Two directions with the disruption messages of their lines.
  TwoStations api;
  widgets::mvg::MvgSource directions(mvgConfig(R"({"station":"de:09162:10","sections":[
        {"name":"In die Stadt","lines":["S3:H:S","S4:H:S","S6:H:S","S8:H:S"]},
        {"name":"Raus","lines":["S3:R:S","S4:R:S","S6:R:S","S8:R:S"]}]})"),
                                     api);
  TEST_ASSERT_TRUE(directions.fetch(kMvgNow));
  TEST_ASSERT_EQUAL_size_t(1, api.requests.size());
  TEST_ASSERT_EQUAL_STRING(
      "Störung S4, S6, S8",
      widgets::mvg::incidentHeading(directions.incidents(), ui::Language::kGerman).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "S6: Technische Störung am Zug. S4, S6, S8: Verspätung eines vorausfahrenden Zuges.",
      widgets::mvg::incidentText(directions.incidents()).c_str());
  options.incidents = directions.incidents();
  ui::BitmapCanvas a(bitmap, 0, 0, 264, 259);
  TEST_ASSERT_EQUAL_INT(4, widgets::mvg::drawDeparturesColumn(a, directions.sections(), options));

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
  TEST_ASSERT_EQUAL_STRING("4 min zu Fuß", stations.sections()[1].note.c_str());
  options.incidents.clear();
  ui::BitmapCanvas b(bitmap, 284, 0, 264, 259);
  TEST_ASSERT_EQUAL_INT(7, widgets::mvg::drawDeparturesColumn(b, stations.sections(), options));

  // One list with one message, in English; the age badge stays on the title.
  options.incidents = {{"Verspätungen wegen einer Signalstörung", {"S8"}}};
  options.language = ui::Language::kEnglish;
  options.note = "Stand 22:21";
  options.note_alert = true;
  ui::BitmapCanvas c(bitmap, 568, 0, 264, 259);
  TEST_ASSERT_EQUAL_INT(5, widgets::mvg::drawDeparturesColumn(c, pasing(false), options));
  check(bitmap, "briefing_departure_incidents");

  // A failed request keeps the old data.
  class Failing : public widgets::mvg::Source {
   public:
    bool fetchDepartures(const widgets::mvg::Config&, JsonDocument&) override { return false; }
  } failing;
  widgets::mvg::MvgSource none(mvgConfig(R"({"station":"de:09162:10"})"), failing);
  TEST_ASSERT_FALSE(none.fetch(kMvgNow));
  TEST_ASSERT_TRUE(none.incidents().empty());
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
  TEST_ASSERT_EQUAL_STRING("Guten Morgen. Ab 16\u00a0Uhr regnet es \u2013 Schirm mit.",
                           br::bandSentence(context, data).c_str());
  // Without weather data only the greeting; offline with the time.
  TEST_ASSERT_EQUAL_STRING("Guten Morgen.", br::bandSentence(context, br::BandData{}).c_str());
  context.network_connected = false;
  br::BandData offline;
  offline.as_of = "07:12";
  TEST_ASSERT_EQUAL_STRING("Guten Morgen. Keine Verbindung \u2013 die Angaben sind von 07:12.",
                           br::bandSentence(context, offline).c_str());
  context.now_s = 0;  // no time yet
  TEST_ASSERT_EQUAL_STRING("", br::bandSentence(context, br::BandData{}).c_str());
}

void test_sentence_parts_by_rank() {
  const wx::Forecast dry = forecast(course(7, 14), {0}, Condition::kOvercast, Condition::kClear);
  std::vector<int> rain = rainFrom(14, 80);
  const wx::Forecast wet = forecast(course(7, 14), rain, Condition::kRain, Condition::kRain);
  const std::vector<widgets::mvg::Incident> incidents = {
      {"Verspätung eines vorausfahrenden Zuges", {"S3"}}};
  std::vector<widgets::mvg::Section> sections(1);
  sections[0].departures = {departure("S6", "Ebersberg", kMorning + 12 * 60, true),
                            departure("S6", "Ebersberg", kMorning + 2 * 3600, true)};
  cal::Occurrences events = {event(0, -1, 0, "Geburtstag Oma"), event(0, 9, 30, "Zahnarzt"),
                             event(0, 7, 0, "Laufen")};

  br::SentenceInput in;
  in.now_s = kMorning;
  in.forecast = &dry;
  // Only the weather: the calm part.
  std::vector<br::Part> parts = br::sentenceParts(in);
  TEST_ASSERT_EQUAL_size_t(1, parts.size());
  TEST_ASSERT_TRUE(parts[0].topic == br::Topic::kCalm);
  // With an event the calm part is left out; the next event of today, not
  // the one that has passed or the all-day one.
  in.events = &events;
  TEST_ASSERT_EQUAL_STRING("Guten Morgen. Um\u00a009:30 Zahnarzt.",
                           br::joinSentence("Guten Morgen.", br::sentenceParts(in), 9).c_str());
  // Everything: ranked, read in a fixed order.
  in.forecast = &wet;
  in.departures = &sections;
  in.incidents = &incidents;
  in.online = false;
  in.as_of = "07:12";
  parts = br::sentenceParts(in);
  TEST_ASSERT_EQUAL_size_t(5, parts.size());
  TEST_ASSERT_TRUE(parts[0].topic == br::Topic::kOffline);
  TEST_ASSERT_TRUE(parts[1].topic == br::Topic::kCancellation);
  TEST_ASSERT_TRUE(parts[2].topic == br::Topic::kDisruption);
  TEST_ASSERT_TRUE(parts[3].topic == br::Topic::kWeather);
  TEST_ASSERT_TRUE(parts[4].topic == br::Topic::kEvent);
  TEST_ASSERT_EQUAL_STRING(
      "Die S6 um\u00a007:54 nach Ebersberg fällt aus. Störung bei der S3 – plane mehr Zeit ein.",
      br::joinSentence("", {parts[2], parts[1]}, 2).c_str());
  // Too long for three lines: the parts of the lowest rank are left out.
  in.online = true;
  std::string used;
  const br::SentenceLayout layout = br::layoutParts("Guten Morgen.", br::sentenceParts(in), &used);
  TEST_ASSERT_TRUE(layout.lines.size() <= 3);
  TEST_ASSERT_EQUAL_STRING(
      "Guten Morgen. Die S6 um\u00a007:54 nach Ebersberg fällt aus. Störung bei der S3 – plane "
      "mehr "
      "Zeit ein.",
      used.c_str());
  TEST_ASSERT_TRUE(layout.small);
  // A part that does not fit is skipped; a shorter one of a lower rank is
  // still taken.
  const std::vector<br::Part> skip = {
      {br::Topic::kCancellation, "Die S6 um\u00a007:53 nach Ebersberg fällt aus."},
      {br::Topic::kDisruption, "Störungen bei S4, S6 und S8 – plane mehr Zeit ein."},
      {br::Topic::kEvent, "Um\u00a018:30 Kino mit Lena."}};
  br::layoutParts("Guten Morgen.", skip, &used);
  TEST_ASSERT_EQUAL_STRING(
      "Guten Morgen. Die S6 um\u00a007:53 nach Ebersberg fällt aus. Um\u00a018:30 Kino mit Lena.",
      used.c_str());
  // A cancellation more than an hour ahead is not mentioned.
  sections[0].departures.erase(sections[0].departures.begin());
  TEST_ASSERT_EQUAL_STRING("",
                           br::cancellationText(sections, kMorning, ui::Language::kGerman).c_str());
}

void test_sentence_parts_in_detail() {
  const auto de = ui::Language::kGerman;
  const auto en = ui::Language::kEnglish;
  std::vector<widgets::mvg::Incident> two = {{"A", {"S8", "S3"}}};
  TEST_ASSERT_EQUAL_STRING("Störungen bei S3 und S8 – plane mehr Zeit ein.",
                           br::disruptionText(two, de).c_str());
  two.push_back({"B", {"U6"}});
  TEST_ASSERT_EQUAL_STRING("Disruptions on S3, S8 and U6 – allow extra time.",
                           br::disruptionText(two, en).c_str());
  two.push_back({"C", {"19"}});
  TEST_ASSERT_EQUAL_STRING("Störungen bei mehreren Linien – plane mehr Zeit ein.",
                           br::disruptionText(two, de).c_str());
  TEST_ASSERT_EQUAL_STRING("", br::disruptionText({}, de).c_str());

  // Events: all-day today if nothing is ahead; in the evening tomorrow's
  // first; long titles shortened.
  cal::Occurrences events = {event(0, -1, 0, "Geburtstag Oma"), event(0, 7, 0, "Laufen"),
                             event(1, 8, 15, "Abstimmung mit dem Architekturbüro wegen Umbau")};
  TEST_ASSERT_EQUAL_STRING("Heute: Geburtstag Oma.",
                           br::eventText(events, kMidnight + 12 * 3600, de).c_str());
  TEST_ASSERT_EQUAL_STRING("Morgen um\u00a008:15 Abstimmung mit dem Architekturbü\u2026.",
                           br::eventText(events, kMidnight + 19 * 3600, de).c_str());
  TEST_ASSERT_EQUAL_STRING("Today: Geburtstag Oma.",
                           br::eventText(events, kMidnight + 12 * 3600, en).c_str());

  // Temperature jump: only in the evening, from 8 degrees.
  wx::Forecast f = forecast(course(7, 14), {0});
  f.days[1].high = 23;
  TEST_ASSERT_EQUAL_STRING("Morgen wird es deutlich wärmer, bis 23°.",
                           br::temperatureJumpText(f, kMidnight + 20 * 3600, de).c_str());
  TEST_ASSERT_EQUAL_STRING("", br::temperatureJumpText(f, kMorning, de).c_str());
  f.days[1].high = 5.6f;
  TEST_ASSERT_EQUAL_STRING("Tomorrow much colder, at most 6°.",
                           br::temperatureJumpText(f, kMidnight + 20 * 3600, en).c_str());
  f.days[1].high = 8;
  TEST_ASSERT_EQUAL_STRING("", br::temperatureJumpText(f, kMidnight + 20 * 3600, de).c_str());
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
  const std::vector<widgets::mvg::Incident> incidents = {
      {"Verspätung eines vorausfahrenden Zuges", {"S3", "S4"}}};
  std::vector<widgets::mvg::Section> sections(1);
  sections[0].departures = {departure("S6", "Ebersberg", kMorning + 12 * 60, true)};
  const cal::Occurrences events = {event(0, 9, 30, "Zahnarzt"), event(1, 8, 0, "Team meeting")};

  widgets::DrawContext context;
  context.network_connected = true;
  // Morning: disruption and rain.
  br::BandData a;
  a.forecast = &wet;
  a.incidents = &incidents;
  context.now_s = kMorning;
  ui::BitmapCanvas first(bitmap, 0, 0, 800, 205);
  br::drawBand(first, context, a);
  // Morning: cancellation and the next event, dry.
  br::BandData b;
  b.forecast = &dry;
  b.departures = &sections;
  b.events = &events;
  ui::BitmapCanvas second(bitmap, 0, 215, 800, 205);
  br::drawBand(second, context, b);
  // Evening in English: much colder tomorrow and tomorrow's first event.
  br::BandData c;
  c.forecast = &cold;
  c.events = &events;
  context.now_s = kMidnight + 20 * 3600;
  context.language = ui::Language::kEnglish;
  ui::BitmapCanvas third(bitmap, 0, 430, 800, 205);
  br::drawBand(third, context, c);
  check(bitmap, "briefing_band_topics");
  TEST_ASSERT_EQUAL_STRING(
      "Good evening. Tomorrow much colder, at most 5°. Tomorrow at\u00a008:00: Team "
      "meeting.",
      br::bandSentence(context, c).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_greeting_by_time_of_day);
  RUN_TEST(test_rain_levels);
  RUN_TEST(test_snow_thunder_heat_and_english);
  RUN_TEST(test_picture_follows_the_weather_of_the_day);
  RUN_TEST(test_sentence_layout_falls_back_to_the_smaller_size);
  RUN_TEST(test_sentence_parts_by_rank);
  RUN_TEST(test_sentence_parts_in_detail);
  RUN_TEST(test_chart_never_meets_the_rain_bars);
  RUN_TEST(test_chart_from_values_every_15_minutes);
  RUN_TEST(test_preview_weather_extremes);
  RUN_TEST(test_preview_calendar_columns);
  RUN_TEST(test_preview_departure_columns);
  RUN_TEST(test_preview_departure_incidents);
  RUN_TEST(test_preview_bands);
  RUN_TEST(test_preview_band_topics);
  return UNITY_END();
}
