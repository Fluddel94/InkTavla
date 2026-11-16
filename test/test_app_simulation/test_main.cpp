// The complete main loop without hardware: configuration, widgets, screen
// and display controller with a simulated clock, a simulated MVG source
// (saved API response, failures on demand) and the PNG display. The frames
// are written to .pio/simulation/; the panel images at important moments
// are compared with reference images in test/fixtures/ui/ (update them with
// INKTAVLA_UPDATE_PREVIEWS=1, see test_ui_preview).

#include <unity.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
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
  int64_t wallTime() const override { return start + ms / 1000; }
  int64_t ms = 0;
  int64_t start = kStart;
};

// MVG source with a saved response; fails on demand like a lost connection.
class FixtureSource : public widgets::mvg::Source {
 public:
  bool fetchDepartures(const widgets::mvg::Config& config, JsonDocument& doc) override {
    requests++;
    last_types = widgets::mvg::transportTypesParam(config);
    if (fail) return false;
    std::ifstream file("test/fixtures/mvg/pasing_20261019.json", std::ios::binary);
    if (widgets::mvg::readDepartures(file, doc) != DeserializationError::Ok) return false;
    // Moved in time for simulations at another time of day.
    for (JsonVariant d : doc.as<JsonArray>()) {
      d["plannedDepartureTime"] = d["plannedDepartureTime"].as<int64_t>() + shift_s * 1000;
    }
    return true;
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
  int64_t shift_s = 0;
};

// Weather source with a saved Open-Meteo response: by default the one of
// 2026-10-20 18:30 (hourly values only).
class WeatherFixtureSource : public widgets::weather::Source {
 public:
  bool fetchForecast(const widgets::weather::Config& config, JsonDocument& doc) override {
    (void)config;
    requests++;
    if (fail) return false;
    std::ifstream file("test/fixtures/weather/" + fixture, std::ios::binary);
    if (deserializeJson(doc, file,
                        DeserializationOption::Filter(widgets::weather::forecastFilter())) !=
        DeserializationError::Ok) {
      return false;
    }
    // By default one day earlier, to fit the simulated evening of
    // 2026-10-19.
    doc["current"]["time"] = doc["current"]["time"].as<int64_t>() + shift_s;
    for (const char* list : {"hourly", "daily"}) {
      for (JsonVariant t : doc[list]["time"].as<JsonArray>()) t.set(t.as<int64_t>() + shift_s);
    }
    return true;
  }
  static constexpr int64_t kDay = 86400;
  int64_t shift_s = -kDay;
  std::string fixture = "muenchen_20261020.json";
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
    std::ifstream file("test/fixtures/calendar/" + fixture, std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    parser.feed(text.str());
    return true;
  }
  bool has_address = true;
  bool fail = false;
  std::string fixture = "week.ics";
  int requests = 0;
};

constexpr char kConfig[] = R"({
  "language": "de",
  "display": {"full_refresh_minutes": 60},
  "widgets": {"mvg": {"station": "de:09162:10", "name": "Pasing", "sections": [
    {"name": "Stadt", "lines": ["S3:H:S", "S4:H:S", "S5:H:S", "S6:H:S", "S8:H:S", "19:H:T"]},
    {"name": "Raus", "lines": ["S3:R:S", "S4:R:S", "S6:R:S", "S8:R:S", "14:H:T", "RB6:H:Z"]}
  ]},
  "weather": {"latitude": 48.137, "longitude": 11.575, "name": "München"}}
})";

struct Rig {
  // calendar_address: whether an iCal address is set (read once, like on the
  // device where saving one restarts it).
  explicit Rig(const char* config_text, const char* name, bool calendar_address = true,
               int64_t start = kStart)
      : display(800, 480, std::string(".pio/simulation/") + name), app(clock, display, services()) {
    clock.start = start;
    // The saved departures are from kStart; move them to the start time.
    source.shift_s = start - kStart;
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

  // 22:25:01: new minute in the header line, MVG updated at second 1 - one
  // partial refresh of the changed areas only (weather and calendar stay).
  rig.runUntil(11000);
  TEST_ASSERT_EQUAL_INT(2, rig.source.requests);
  TEST_ASSERT_EQUAL_size_t(2, rig.display.updates().size());
  const hal::PngDisplay::Update& u = rig.display.updates()[1];
  TEST_ASSERT_TRUE(u.mode == hal::Refresh::kPartial);
  TEST_ASSERT_TRUE(contains(u.regions, 0, 0));       // header line with the clock
  TEST_ASSERT_FALSE(contains(u.regions, 313, 205));  // calendar (every 15 minutes)
  TEST_ASSERT_FALSE(contains(u.regions, 553, 205));  // weather (every 30 minutes)

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
  // Without Wi-Fi the start screen stays and says so.
  Rig rig(kConfig, "offline");
  rig.app.setStartInfo({"0.1.0", "Heimnetz"});
  rig.source.fail = true;
  rig.runUntil(30000);
  TEST_ASSERT_TRUE(rig.app.controller().starting());
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kWaiting);
  TEST_ASSERT_EQUAL_size_t(1, rig.display.updates().size());
  checkPanel(rig.display, "sim_waiting");

  // Connected: weather and calendar arrive, the departures fail. That is a
  // result too, so the briefing follows at once, with a full refresh.
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(31000);
  TEST_ASSERT_FALSE(rig.app.controller().starting());
  TEST_ASSERT_TRUE(rig.display.updates().back().mode == hal::Refresh::kFull);

  // The connection is lost again: without ever having departures the column
  // shows "Keine Verbindung" once both grace periods are over.
  rig.app.controller().setNetworkConnected(false);
  rig.runUntil(95000);
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kNoData);
  TEST_ASSERT_FALSE(rig.app.controller().state().network_connected);
  checkPanel(rig.display, "sim_no_connection");
}

void test_start_screen() {
  // Data at the first connection: the start screen is not even drawn, the
  // first frame is the briefing (in the simulation fetches take no time).
  {
    Rig rig(kConfig, "start_connected");
    rig.app.controller().setNetworkConnected(true);
    rig.runUntil(0);
    TEST_ASSERT_FALSE(rig.app.controller().starting());
    TEST_ASSERT_FALSE(rig.app.controller().state().starting);
  }
  // No Wi-Fi saved: the Wi-Fi setup screen replaces the start screen at once.
  {
    Rig rig(kConfig, "start_no_wifi");
    rig.runUntil(1000);
    TEST_ASSERT_TRUE(rig.app.controller().state().starting);
    core::AccessNotice wifi;
    wifi.mode = core::AccessMode::kWifiNotSetUp;
    rig.app.controller().setAccessNotice(wifi);
    rig.runUntil(2000);
    TEST_ASSERT_FALSE(rig.app.controller().state().starting);
    TEST_ASSERT_TRUE(rig.display.updates().back().mode == hal::Refresh::kFull);
  }
  // The settings page (reopened after saving) does not end it.
  {
    Rig rig(kConfig, "start_settings");
    core::AccessNotice settings;
    settings.mode = core::AccessMode::kSettings;
    settings.code = "482913";
    rig.app.controller().setAccessNotice(settings);
    rig.runUntil(1000);
    TEST_ASSERT_TRUE(rig.app.controller().starting());
  }
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
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(120000);
  TEST_ASSERT_EQUAL_INT(0, rig.source.requests);    // no station: nothing fetched
  TEST_ASSERT_EQUAL_INT(0, rig.calendar.requests);  // no address: nothing fetched
  TEST_ASSERT_TRUE(rig.app.controller().state().contents[0].state == core::DataState::kNotSetUp);
  checkPanel(rig.display, "sim_not_set_up");
}

void test_widget_in_a_column_it_does_not_fit() {
  // The departures need 280 px (step M) and band + 3 has only one column
  // of that width: the second one does not fit.
  constexpr char kTwice[] = R"({"layout": {"slots": ["calendar", "mvg", "mvg"]},
    "widgets": {"mvg": {"station": "de:09162:10", "name": "Pasing"},
    "weather": {"latitude": 48.137, "longitude": 11.575}}})";
  Rig rig(kTwice, "does_not_fit");
  int misfits = 0;
  std::string message;
  for (const core::ConfigIssue& i : rig.issues) {
    if (i.path == "layout.slots" && i.message.find("does not fit") != std::string::npos) {
      misfits++;
      message = i.message;
    }
  }
  TEST_ASSERT_EQUAL_INT(1, misfits);
  TEST_ASSERT_EQUAL_STRING("widget \"mvg\" does not fit slot 3 (214 px); it needs 280 px",
                           message.c_str());
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  checkPanel(rig.display, "sim_does_not_fit");

  // The check before saving reports the same.
  JsonDocument doc;
  deserializeJson(doc, kTwice);
  core::ConfigIssues issues;
  widgets::validateConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(1, issues.size());
  TEST_ASSERT_EQUAL_STRING(message.c_str(), issues[0].message.c_str());
}

void test_layout_follows_the_widgets() {
  using widgets::layoutOptions;
  // Band + 3: the wide column is where the widget sits that needs M.
  TEST_ASSERT_EQUAL_INT(3, layoutOptions({"mvg", "calendar", "weather"}).columns);
  TEST_ASSERT_EQUAL_INT(0, layoutOptions({"mvg", "calendar", "weather"}).wide_column);
  TEST_ASSERT_EQUAL_INT(1, layoutOptions({"calendar", "mvg", "weather"}).wide_column);
  TEST_ASSERT_EQUAL_INT(2, layoutOptions({"calendar", "weather", "mvg"}).wide_column);
  // The first of two; none or an unknown widget: the first column.
  TEST_ASSERT_EQUAL_INT(1, layoutOptions({"calendar", "mvg", "mvg"}).wide_column);
  TEST_ASSERT_EQUAL_INT(0, layoutOptions({"calendar", "weather", "weather"}).wide_column);
  TEST_ASSERT_EQUAL_INT(0, layoutOptions({"clock", "weather", "calendar"}).wide_column);
  // Two entries: band + 2.
  TEST_ASSERT_EQUAL_INT(2, layoutOptions({"weather", "mvg"}).columns);
  TEST_ASSERT_FALSE(layoutOptions({"weather", "mvg"}, false).dividers);
  // Every widget fits from its smallest step on.
  const widgets::WidgetType* mvg = widgets::findWidgetType("mvg");
  TEST_ASSERT_FALSE(mvg->fits(ui::SizeStep::kS));
  TEST_ASSERT_TRUE(mvg->fits(ui::SizeStep::kM));
  TEST_ASSERT_TRUE(mvg->fits(ui::SizeStep::kL));
  TEST_ASSERT_TRUE(widgets::findWidgetType("calendar")->fits(ui::SizeStep::kS));
  TEST_ASSERT_TRUE(mvg->band);
  TEST_ASSERT_FALSE(widgets::findWidgetType("calendar")->band);
}

void test_settings_page_gets_the_layouts() {
  JsonDocument doc;
  deserializeJson(doc, "{" + widgets::settingsLayoutJson() + "}");
  TEST_ASSERT_EQUAL_size_t(3, doc["widgets"].size());
  TEST_ASSERT_EQUAL_STRING("mvg", doc["widgets"][0]["id"]);
  TEST_ASSERT_EQUAL_STRING("M", doc["widgets"][0]["min"]);
  TEST_ASSERT_TRUE(doc["widgets"][0]["band"].as<bool>());
  TEST_ASSERT_TRUE(doc["widgets"][0]["band_alone"].isNull());
  TEST_ASSERT_TRUE(doc["widgets"][1]["band_alone"].as<bool>());  // the weather
  TEST_ASSERT_EQUAL_STRING("S", doc["widgets"][2]["min"]);
  TEST_ASSERT_FALSE(doc["widgets"][2]["band"].as<bool>());
  TEST_ASSERT_EQUAL_size_t(2, doc["layouts"].size());
  TEST_ASSERT_EQUAL_size_t(3, doc["layouts"][0]["steps"].size());
  TEST_ASSERT_EQUAL_STRING("M", doc["layouts"][0]["steps"][0]);
  TEST_ASSERT_EQUAL_STRING("M", doc["layouts"][0]["wide"]);
  TEST_ASSERT_EQUAL_size_t(2, doc["layouts"][1]["steps"].size());
  TEST_ASSERT_EQUAL_STRING("L", doc["layouts"][1]["steps"][1]);
  TEST_ASSERT_EQUAL_INT(214, doc["steps"]["S"].as<int>());
  TEST_ASSERT_EQUAL_INT(280, doc["steps"]["M"].as<int>());
  TEST_ASSERT_EQUAL_INT(367, doc["steps"]["L"].as<int>());
  TEST_ASSERT_EQUAL_INT(800, doc["screen"]["width"].as<int>());
  TEST_ASSERT_EQUAL_INT(26, doc["screen"]["gap"].as<int>());
}

void test_one_source_for_several_views() {
  // The weather twice: one source, fetched once, drawn in both columns.
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
  // Both columns show the same picture (the second one 240 px further
  // right).
  const ui::Bitmap& b = rig.app.screen().bitmap();
  int differences = 0, black = 0;
  for (int y = 205; y < 480; y++) {
    for (int x = 326; x < 326 + 214; x++) {
      const bool left = b.get(x, y) == ui::Color::kBlack;
      black += left;
      differences += left != (b.get(x + 240, y) == ui::Color::kBlack);
    }
  }
  TEST_ASSERT_GREATER_THAN(1000, black);
  TEST_ASSERT_EQUAL_INT(0, differences);
}

// Wednesday 2026-10-21 at 07:41:50: the forecast of a morning with values
// every 15 minutes (saved 2026-10-22 07:30, moved back by a day), the week of
// the calendar fixture and the departures moved to that morning.
constexpr int64_t kBriefingMorning = 1792561310;
constexpr char kBriefingConfig[] = R"({
  "language": "de",
  "widgets": {"mvg": {"station": "de:09162:10", "name": "Pasing", "sections": [
    {"name": "In die Stadt", "lines": ["S3:H:S", "S4:H:S", "S5:H:S", "S6:H:S", "S8:H:S", "19:H:T"]},
    {"name": "Raus", "lines": ["S3:R:S", "S4:R:S", "S6:R:S", "S8:R:S", "14:H:T", "RB6:H:Z"]}
  ]},
  "weather": {"latitude": 48.137, "longitude": 11.575, "name": "München"}}
})";

void test_briefing_morning() {
  Rig rig(kBriefingConfig, "briefing", true, kBriefingMorning);
  rig.weather.fixture = "muenchen_20261022.json";
  TEST_ASSERT_EQUAL_size_t(0, rig.issues.size());
  rig.app.controller().setNetworkConnected(true);
  // Order of the design: departures, calendar, weather.
  TEST_ASSERT_EQUAL_STRING("mvg", rig.app.sources()[0]->id());
  TEST_ASSERT_EQUAL_STRING("calendar", rig.app.sources()[1]->id());
  TEST_ASSERT_EQUAL_STRING("weather", rig.app.sources()[2]->id());
  rig.runUntil(0);
  TEST_ASSERT_TRUE(rig.display.updates()[0].mode == hal::Refresh::kFull);
  checkPanel(rig.display, "sim_briefing_morning");

  // 07:42:01: only the clock and the departures change.
  rig.runUntil(11000);
  const hal::PngDisplay::Update& u = rig.display.updates().back();
  TEST_ASSERT_TRUE(u.mode == hal::Refresh::kPartial);
  TEST_ASSERT_TRUE(contains(u.regions, 0, 0));       // header line with the clock
  TEST_ASSERT_FALSE(contains(u.regions, 0, 44));     // sentence unchanged
  TEST_ASSERT_FALSE(contains(u.regions, 553, 205));  // weather unchanged

  // Wi-Fi lost: shown after 60 s in the header line and in the sentence.
  rig.app.controller().setNetworkConnected(false);
  rig.weather.fail = true;
  rig.source.fail = true;
  rig.runUntil(11000 + 59000);
  TEST_ASSERT_TRUE(rig.app.controller().state().network_connected);
  rig.runUntil(11000 + 125000);
  TEST_ASSERT_FALSE(rig.app.controller().state().network_connected);
  checkPanel(rig.display, "sim_briefing_offline");
}

void test_access_notices() {
  // Settings page open: its notice replaces the sentence, the columns stay;
  // only the band is refreshed.
  Rig rig(kBriefingConfig, "access", true, kBriefingMorning);
  rig.weather.fixture = "muenchen_20261022.json";
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  core::AccessNotice settings;
  settings.mode = core::AccessMode::kSettings;
  settings.address = "http://192.168.178.45";
  settings.code = "482913";
  settings.qr = "http://192.168.178.45/?code=482913";
  settings.closes_wall_s = kBriefingMorning + 30 * 60;
  const size_t before = rig.display.updates().size();
  rig.app.controller().setAccessNotice(settings);
  rig.runUntil(1000);
  TEST_ASSERT_EQUAL_size_t(before + 1, rig.display.updates().size());
  const hal::PngDisplay::Update& u = rig.display.updates().back();
  TEST_ASSERT_TRUE(u.mode == hal::Refresh::kPartial);
  TEST_ASSERT_TRUE(contains(u.regions, 0, 44));      // band: notice instead of the sentence
  TEST_ASSERT_FALSE(contains(u.regions, 553, 205));  // weather column unchanged
  checkPanel(rig.display, "sim_settings_open");

  // Saved Wi-Fi not reached since the start: the whole screen.
  core::AccessNotice wifi;
  wifi.mode = core::AccessMode::kWifiNotConnected;
  wifi.network = "Heimnetz";
  rig.app.controller().setAccessNotice(wifi);
  rig.app.controller().requestFullRefresh();
  rig.runUntil(2000);
  TEST_ASSERT_TRUE(rig.display.updates().back().mode == hal::Refresh::kFull);
  checkPanel(rig.display, "sim_wifi_missing");

  // No network saved, a setup attempt failed (wrong password?).
  core::AccessNotice failed;
  failed.mode = core::AccessMode::kWifiNotSetUp;
  failed.network = "Heimnetz";
  rig.app.controller().setAccessNotice(failed);
  rig.runUntil(2500);
  checkPanel(rig.display, "sim_wifi_failed");

  // Closed again: the briefing as before.
  rig.app.controller().setAccessNotice({});
  rig.runUntil(3000);
  TEST_ASSERT_TRUE(rig.app.controller().state().notice.mode == core::AccessMode::kNone);
}

// A wrong calendar address (the server sends a sign-in page instead of the
// iCal file): shown at once in the column and in the sentence, without the
// minute of "Wird geladen …" and "Keine Daten".
void test_calendar_address_wrong() {
  Rig rig(kBriefingConfig, "calendar_address", true, kBriefingMorning);
  rig.weather.fixture = "muenchen_20261022.json";
  rig.calendar.fixture = "not_ical.html";
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  TEST_ASSERT_EQUAL_INT(1, rig.calendar.requests);
  checkPanel(rig.display, "sim_calendar_address");
}

// The morning of the briefing with other widgets in the columns.
void checkLayout(const char* slots, const char* name) {
  std::string config = kBriefingConfig;
  config.replace(config.find('{'), 1, std::string(R"({"layout": {"slots": )") + slots + "},");
  Rig rig(config.c_str(), "briefing", true, kBriefingMorning);
  rig.weather.fixture = "muenchen_20261022.json";
  TEST_ASSERT_EQUAL_size_t(0, rig.issues.size());
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  checkPanel(rig.display, name);
}

void test_briefing_layouts() {
  // Band + 3: the wide column follows the departures (step M).
  checkLayout(R"(["calendar", "mvg", "weather"])", "sim_layout_wide_middle");
  checkLayout(R"(["weather", "calendar", "mvg"])", "sim_layout_wide_right");
  // Without a widget that needs M the first column is the wide one: the
  // calendar and the weather at step M.
  checkLayout(R"(["calendar", "weather", "weather"])", "sim_layout_calendar_m");
  checkLayout(R"(["weather", "calendar", "calendar"])", "sim_layout_weather_m");
  // Band + 2: every widget at step L.
  checkLayout(R"(["mvg", "weather"])", "sim_layout_two_mvg_weather");
  checkLayout(R"(["calendar", "mvg"])", "sim_layout_two_calendar_mvg");
}

void test_band_plus_two_refreshes_its_halves() {
  std::string config = kBriefingConfig;
  config.replace(config.find('{'), 1, R"({"layout": {"slots": ["weather", "mvg"]},)");
  Rig rig(config.c_str(), "briefing", true, kBriefingMorning);
  rig.weather.fixture = "muenchen_20261022.json";
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  // 07:42:01: the clock changes, the weather (left half) does not. The
  // refresh areas of the columns are the halves of the screen.
  rig.runUntil(11000);
  const hal::PngDisplay::Update& u = rig.display.updates().back();
  TEST_ASSERT_TRUE(u.mode == hal::Refresh::kPartial);
  TEST_ASSERT_TRUE(contains(u.regions, 0, 0));
  TEST_ASSERT_FALSE(contains(u.regions, 0, 205));
  const std::vector<hal::Rect> areas = rig.app.screen().areas();
  TEST_ASSERT_TRUE(contains(areas, 0, 205));
  TEST_ASSERT_TRUE(contains(areas, 400, 205));
  TEST_ASSERT_FALSE(contains(areas, 313, 205));
  TEST_ASSERT_FALSE(contains(areas, 553, 205));
}

// The band switch of the weather ("widgets.weather.band").
void test_band_switch_of_the_weather() {
  // Off and in a column: the column and the picture stay, the sentence
  // leaves out the weather.
  const std::string name = R"("name": "München")";
  const std::string off = R"("name": "München", "band": false)";
  std::string config = kBriefingConfig;
  config.replace(config.find(name), name.size(), off);
  Rig rig(config.c_str(), "band_off", true, kBriefingMorning);
  rig.weather.fixture = "muenchen_20261022.json";
  TEST_ASSERT_EQUAL_size_t(0, rig.issues.size());
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  TEST_ASSERT_EQUAL_INT(1, rig.weather.requests);
  checkPanel(rig.display, "sim_band_weather_sentence_off");

  // Off and in no column: nothing is fetched, a placeholder takes the place
  // of the picture; the settings are still checked.
  std::string two = kBriefingConfig;
  two.replace(two.find('{'), 1, R"({"layout": {"slots": ["mvg", "calendar"]},)");
  const std::string two_on = two;
  two.replace(two.find(name), name.size(), off);
  Rig alone(two.c_str(), "band_off_alone", true, kBriefingMorning);
  alone.weather.fixture = "muenchen_20261022.json";
  TEST_ASSERT_EQUAL_size_t(0, alone.issues.size());
  for (const auto& source : alone.app.sources()) {
    TEST_ASSERT_NOT_EQUAL(0, std::strcmp("weather", source->id()));
  }
  alone.app.controller().setNetworkConnected(true);
  alone.runUntil(0);
  TEST_ASSERT_EQUAL_INT(0, alone.weather.requests);
  checkPanel(alone.display, "sim_band_weather_off_alone");

  // On (the default) and in no column: fetched for the band, as before.
  Rig fetched(two_on.c_str(), "band_on_alone", true, kBriefingMorning);
  fetched.weather.fixture = "muenchen_20261022.json";
  fetched.app.controller().setNetworkConnected(true);
  fetched.runUntil(0);
  TEST_ASSERT_EQUAL_INT(1, fetched.weather.requests);
}

void test_band_switch_is_checked() {
  // Not a switch: reported by the reader of the key; a widget with nothing
  // for the band says so.
  Rig bad(R"({"widgets": {"mvg": {"band": "yes"}}})", "band_bad");
  bool wrong_type = false;
  for (const auto& i : bad.issues) wrong_type = wrong_type || i.path == "widgets.mvg.band";
  TEST_ASSERT_TRUE(wrong_type);
  Rig calendar(R"({"widgets": {"calendar": {"band": false}}})", "band_calendar");
  bool nothing = false;
  for (const auto& i : calendar.issues) {
    nothing = nothing || (i.path == "widgets.calendar.band" &&
                          i.message.find("nothing for the band") != std::string::npos);
  }
  TEST_ASSERT_TRUE(nothing);
}

void test_removed_grid_setting_is_reported() {
  // "layout.grid" from older files: a message, the display as usual.
  std::string config = kBriefingConfig;
  config.replace(config.find('{'), 1, R"({"layout": {"grid": "equal"},)");
  Rig rig(config.c_str(), "briefing", true, kBriefingMorning);
  rig.weather.fixture = "muenchen_20261022.json";
  TEST_ASSERT_EQUAL_size_t(1, rig.issues.size());
  TEST_ASSERT_EQUAL_STRING("layout.grid", rig.issues[0].path.c_str());
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(0);
  checkPanel(rig.display, "sim_briefing_morning");
}

void test_briefing_not_set_up() {
  // Without station, calendar address and location: messages in the
  // columns, the band only greets.
  Rig rig(R"({"language": "en"})", "briefing_setup", false, kBriefingMorning);
  rig.app.controller().setNetworkConnected(true);
  rig.runUntil(2000);
  checkPanel(rig.display, "sim_briefing_not_set_up");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_normal_operation_and_refresh_types);
  RUN_TEST(test_short_outage_is_bridged_then_age_is_shown);
  RUN_TEST(test_no_connection_from_the_start);
  RUN_TEST(test_start_screen);
  RUN_TEST(test_station_name_from_the_api);
  RUN_TEST(test_missing_settings_and_unknown_widget);
  RUN_TEST(test_widget_in_a_column_it_does_not_fit);
  RUN_TEST(test_layout_follows_the_widgets);
  RUN_TEST(test_settings_page_gets_the_layouts);
  RUN_TEST(test_one_source_for_several_views);
  RUN_TEST(test_briefing_morning);
  RUN_TEST(test_access_notices);
  RUN_TEST(test_calendar_address_wrong);
  RUN_TEST(test_briefing_layouts);
  RUN_TEST(test_band_plus_two_refreshes_its_halves);
  RUN_TEST(test_band_switch_of_the_weather);
  RUN_TEST(test_band_switch_is_checked);
  RUN_TEST(test_removed_grid_setting_is_reported);
  RUN_TEST(test_briefing_not_set_up);
  return UNITY_END();
}
