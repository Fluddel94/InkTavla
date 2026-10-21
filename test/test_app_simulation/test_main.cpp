// The complete main loop without hardware: configuration, widgets, screen
// and display controller with a simulated clock, a simulated MVG source
// (saved API response, failures on demand) and the PNG display. The frames
// are written to .pio/simulation/; the panel images at important moments
// are compared with reference images in test/fixtures/ui/ (update them with
// INKTAVLA_UPDATE_PREVIEWS=1, see test_ui_preview).

#include <unity.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/config.h"
#include "core/time.h"
#include "hal/png.h"
#include "hal/png_display.h"
#include "widgets/app.h"
#include "widgets/calendar/parser.h"
#include "widgets/mvg/departures.h"
#include "widgets/weather/forecast.h"

namespace {

constexpr int64_t kStart = 1792441490;  // 2026-10-19 22:24:50 CEST

class SimClock : public core::Clock {
 public:
  int64_t monotonicMs() const override { return ms; }
  int64_t wallTime() const override { return kStart + ms / 1000; }
  int64_t ms = 0;
};

// MVG source with a saved response; fails on demand like a lost connection.
class FixtureSource : public widgets::mvg::Source {
 public:
  bool fetchDepartures(const widgets::mvg::Config& config, JsonDocument& doc) override {
    requests++;
    last_types = widgets::mvg::transportTypesParam(config);
    if (fail) return false;
    std::ifstream file("test/fixtures/mvg/pasing_20261019.json", std::ios::binary);
    return widgets::mvg::readDepartures(file, doc) == DeserializationError::Ok;
  }
  bool fetchStationName(const std::string& station, std::string& name) override {
    name_requests++;
    if (fail || station != "de:09162:10") return false;
    name = "München-Pasing";
    return true;
  }
  bool fail = false;
  int requests = 0;
  int name_requests = 0;
  std::string last_types;
};

// Weather source with the saved Open-Meteo response of 2026-10-20 18:30.
class WeatherFixtureSource : public widgets::weather::Source {
 public:
  bool fetchForecast(const widgets::weather::Config& config, JsonDocument& doc) override {
    (void)config;
    requests++;
    if (fail) return false;
    std::ifstream file("test/fixtures/weather/muenchen_20261020.json", std::ios::binary);
    if (deserializeJson(doc, file,
                        DeserializationOption::Filter(widgets::weather::forecastFilter())) !=
        DeserializationError::Ok) {
      return false;
    }
    // One day earlier, to fit the simulated evening of 2026-10-19.
    doc["current"]["time"] = doc["current"]["time"].as<int64_t>() - kDay;
    for (const char* list : {"hourly", "daily"}) {
      for (JsonVariant t : doc[list]["time"].as<JsonArray>()) t.set(t.as<int64_t>() - kDay);
    }
    return true;
  }
  static constexpr int64_t kDay = 86400;
  bool fail = false;
  int requests = 0;
};

// Calendar source with the hand-written week of test/fixtures/calendar/.
class CalendarFixtureSource : public widgets::calendar::Source {
 public:
  bool configured() const override { return has_address; }
  bool fetchCalendar(widgets::calendar::Parser& parser) override {
    requests++;
    if (fail) return false;
    std::ifstream file("test/fixtures/calendar/week.ics", std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    parser.feed(text.str());
    return true;
  }
  bool has_address = true;
  bool fail = false;
  int requests = 0;
};

constexpr char kConfig[] = R"({
  "language": "de",
  "display": {"full_refresh_minutes": 60},
  "layout": {"template": "one_plus_two", "slots": ["mvg", "weather", "calendar"]},
  "widgets": {"mvg": {"station": "de:09162:10", "name": "Pasing", "sections": [
    {"name": "Stadt", "lines": ["S3:H:S", "S4:H:S", "S5:H:S", "S6:H:S", "S8:H:S", "19:H:T"]},
    {"name": "Raus", "lines": ["S3:R:S", "S4:R:S", "S6:R:S", "S8:R:S", "14:H:T", "RB6:H:Z"]}
  ]},
  "weather": {"latitude": 48.137, "longitude": 11.575, "name": "München"}}
})";

struct Rig {
  // calendar_address: whether an iCal address is set (read once, like on the
  // device where saving one restarts it).
  explicit Rig(const char* config_text, const char* name, bool calendar_address = true)
      : display(800, 480, std::string(".pio/simulation/") + name), app(clock, display, services()) {
    calendar.has_address = calendar_address;
    std::filesystem::remove_all(std::string(".pio/simulation/") + name);
    JsonDocument doc;
    core::ConfigIssues read_issues;
    core::readConfig(config_text, std::char_traits<char>::length(config_text), doc, read_issues);
    app.configure(doc.as<JsonObjectConst>(), issues);
    issues.insert(issues.end(), read_issues.begin(), read_issues.end());
  }

  widgets::Services services() {
    widgets::Services s;
    s.mvg = &source;
    s.weather = &weather;
    s.calendar = &calendar;
    return s;
  }

  // Runs the loop until the given simulated time (ms since start).
  void runUntil(int64_t end_ms) {
    while (clock.ms <= end_ms) {
      const int64_t wait = app.step();
      if (clock.ms + std::max<int64_t>(wait, 1) > end_ms) {
        clock.ms = end_ms + 1;
        break;
      }
      clock.ms += std::max<int64_t>(wait, 1);
    }
  }

  SimClock clock;
  FixtureSource source;
  WeatherFixtureSource weather;
  CalendarFixtureSource calendar;
  hal::PngDisplay display;
  widgets::App app;
  core::ConfigIssues issues;
};

std::string readFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void checkPanel(const hal::PngDisplay& display, const char* name) {
  const std::string png = display.png();
  std::filesystem::create_directories(".pio/preview");
  std::ofstream(std::string(".pio/preview/") + name + ".png", std::ios::binary) << png;
  const std::string reference = std::string("test/fixtures/ui/") + name + ".png";
  if (std::getenv("INKTAVLA_UPDATE_PREVIEWS") != nullptr) {
    std::ofstream(reference, std::ios::binary) << png;
    return;
  }
  const std::string expected = readFile(reference);
  TEST_ASSERT_TRUE_MESSAGE(!expected.empty(), ("missing reference " + reference).c_str());
  TEST_ASSERT_TRUE_MESSAGE(hal::png::criticalChunks(expected) == png,
                           ("differs from " + reference + ", see .pio/preview/").c_str());
}

bool contains(const std::vector<hal::Rect>& regions, int x, int y) {
  for (const hal::Rect& r : regions) {
    if (r.x == x && r.y == y) return true;
  }
  return false;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_normal_operation_and_refresh_types() {
  Rig rig(kConfig, "normal");
  TEST_ASSERT_EQUAL_size_t(0, rig.issues.size());
  rig.app.controller().setNetworkConnected(true);

  rig.runUntil(0);
  TEST_ASSERT_EQUAL_INT(1, rig.source.requests);
  TEST_ASSERT_EQUAL_STRING("SBAHN,TRAM,BAHN", rig.source.last_types.c_str());
  TEST_ASSERT_EQUAL_size_t(1, rig.display.updates().size());
  TEST_ASSERT_TRUE(rig.display.updates()[0].mode == hal::Refresh::kFull);
  checkPanel(rig.display, "sim_start");

  // 22:25:01: new minute in the header, MVG updated at second 1 - one
  // partial refresh of the changed areas only (weather and calendar stay).
  rig.runUntil(11000);
  TEST_ASSERT_EQUAL_INT(2, rig.source.requests);
  TEST_ASSERT_EQUAL_size_t(2, rig.display.updates().size());
  const hal::PngDisplay::Update& u = rig.display.updates()[1];
  TEST_ASSERT_TRUE(u.mode == hal::Refresh::kPartial);
  TEST_ASSERT_TRUE(contains(u.regions, 0, 0));       // header
  TEST_ASSERT_FALSE(contains(u.regions, 360, 42));   // weather (every 30 minutes)
  TEST_ASSERT_FALSE(contains(u.regions, 360, 262));  // calendar (every 15 minutes)

  // One hour later at least one full refresh against ghosting.
  rig.runUntil(3700000);
  int full = 0;
  for (const auto& update : rig.display.updates()) full += update.mode == hal::Refresh::kFull;
  TEST_ASSERT_EQUAL_INT(2, full);
  // One request per minute, the calendar every 15 minutes.
  TEST_ASSERT_INT_WITHIN(1, 63, rig.source.requests);
  TEST_ASSERT_INT_WITHIN(1, 5, rig.calendar.requests);
}

void test_short_outage_is_bridged_then_age_is_shown() {
  Rig rig(kConfig, "outage");
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(11000);  // 22:25:01, data from then
  rig.source.fail = true;
  rig.app.controller().setNetworkConnected(false);

  // 59 s after the first failure (22:26:01) still no hint, and the lost
  // connection is not shown yet either.
  rig.runUntil(11000 + 59000);
  TEST_ASSERT_TRUE(rig.app.controller().state().network_connected);
  rig.runUntil(11000 + 60000 + 59000);
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kFresh);
  TEST_ASSERT_FALSE(rig.app.controller().state().network_connected);
  // Then the age badge "Stand 22:25".
  rig.runUntil(11000 + 60000 + 61000);
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kStale);
  checkPanel(rig.display, "sim_stale");

  // Back online: the badge disappears with the next successful update.
  rig.source.fail = false;
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(11000 + 60000 + 75000);
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kFresh);
}

void test_no_connection_from_the_start() {
  Rig rig(kConfig, "offline");
  rig.source.fail = true;
  rig.runUntil(30000);
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kWaiting);
  rig.runUntil(75000);
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kNoData);
  checkPanel(rig.display, "sim_no_connection");
}

void test_station_name_from_the_api() {
  // Without "name" the title comes from the API, asked once.
  Rig rig(R"({"widgets": {"mvg": {"station": "de:09162:10"}}})", "name");
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(130000);
  TEST_ASSERT_EQUAL_INT(1, rig.source.name_requests);
  TEST_ASSERT_TRUE(rig.source.requests >= 3);
  checkPanel(rig.display, "sim_station_name");
}

void test_missing_settings_and_unknown_widget() {
  Rig rig(R"({"layout": {"slots": ["mvg", "clock", "calendar"]}, "language": "en"})", "setup",
          false);  // no iCal address in NVS
  bool unknown = false;
  for (const core::ConfigIssue& i : rig.issues) unknown = unknown || i.path == "layout.slots";
  TEST_ASSERT_TRUE(unknown);
  rig.runUntil(120000);
  TEST_ASSERT_EQUAL_INT(0, rig.source.requests);    // no station: nothing fetched
  TEST_ASSERT_EQUAL_INT(0, rig.calendar.requests);  // no address: nothing fetched
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kNotSetUp);
  checkPanel(rig.display, "sim_not_set_up");
}

void test_widget_on_a_slot_it_does_not_fit() {
  // Weather and calendar are made for wide tiles, not narrow columns.
  constexpr char kThreeColumns[] = R"({"layout": {"template": "three_columns"},
    "widgets": {"mvg": {"station": "de:09162:10", "name": "Pasing"},
    "weather": {"latitude": 48.137, "longitude": 11.575}}})";
  Rig rig(kThreeColumns, "does_not_fit");
  int misfits = 0;
  for (const core::ConfigIssue& i : rig.issues) {
    if (i.path == "layout.slots" && i.message.find("does not fit") != std::string::npos) misfits++;
  }
  TEST_ASSERT_EQUAL_INT(2, misfits);
  TEST_ASSERT_EQUAL_STRING("widget \"weather\" does not fit slot 2 (narrow column); it fits: tile",
                           rig.issues[0].message.c_str());
  // Only the MVG source exists: nothing is fetched for the other two.
  TEST_ASSERT_EQUAL_size_t(1, rig.app.sources().size());
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  TEST_ASSERT_EQUAL_INT(0, rig.weather.requests);
  TEST_ASSERT_EQUAL_INT(0, rig.calendar.requests);
  checkPanel(rig.display, "sim_does_not_fit");

  // The check before saving reports the same.
  JsonDocument doc;
  deserializeJson(doc, kThreeColumns);
  core::ConfigIssues issues;
  widgets::validateConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(2, issues.size());
}

void test_one_source_for_several_views() {
  // The weather twice: one source, fetched once, drawn in both tiles.
  Rig rig(R"({"layout": {"slots": ["mvg", "weather", "weather"]},
    "widgets": {"weather": {"latitude": 48.137, "longitude": 11.575, "name": "München"}}})",
          "shared");
  TEST_ASSERT_EQUAL_size_t(0, rig.issues.size());
  TEST_ASSERT_EQUAL_size_t(2, rig.app.sources().size());
  TEST_ASSERT_EQUAL_STRING("mvg", rig.app.sources()[0]->id());
  TEST_ASSERT_EQUAL_STRING("weather", rig.app.sources()[1]->id());
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  TEST_ASSERT_EQUAL_INT(1, rig.weather.requests);
  // Both tiles show the same picture (the second one 220 px lower).
  const ui::Bitmap& b = rig.app.screen().bitmap();
  int differences = 0, black = 0;
  for (int y = 42; y < 42 + 218; y++) {
    for (int x = 360; x < 800; x++) {
      const bool top = b.get(x, y) == ui::Color::kBlack;
      black += top;
      differences += top != (b.get(x, y + 220) == ui::Color::kBlack);
    }
  }
  TEST_ASSERT_GREATER_THAN(1000, black);
  TEST_ASSERT_EQUAL_INT(0, differences);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_normal_operation_and_refresh_types);
  RUN_TEST(test_short_outage_is_bridged_then_age_is_shown);
  RUN_TEST(test_no_connection_from_the_start);
  RUN_TEST(test_station_name_from_the_api);
  RUN_TEST(test_missing_settings_and_unknown_widget);
  RUN_TEST(test_widget_on_a_slot_it_does_not_fit);
  RUN_TEST(test_one_source_for_several_views);
  return UNITY_END();
}
