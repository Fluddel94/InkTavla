// Settings page: access (one-time code, session, time limit), saving the
// configuration and secrets, and the setup help of the widgets (station
// search, lines with example destinations, place search) with saved
// responses of the MVG API and Open-Meteo (2026-10-25).

#include <unity.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/secrets.h"
#include "core/settings_api.h"
#include "core/settings_session.h"
#include "widgets/app.h"
#include "widgets/mvg/setup.h"
#include "widgets/weather/setup.h"

using core::SettingsSession;

namespace {

constexpr int64_t kMinute = 60 * 1000;

// Predictable "random" numbers.
struct Counter {
  uint32_t next = 482913;
  uint32_t operator()() { return next++; }
};

class MemoryStore : public core::ConfigStore {
 public:
  bool write(const char* path, const std::string& data) override {
    if (fail_write) return false;
    files[path] = data;
    return true;
  }
  bool rename(const char* from, const char* to) override {
    if (files.count(from) == 0) return false;
    files[to] = files[from];
    files.erase(from);
    return true;
  }
  bool read(const char* path, std::string& data) override {
    if (files.count(path) == 0) return false;
    data = files[path];
    return true;
  }
  std::map<std::string, std::string> files;
  bool fail_write = false;
};

class MemorySecrets : public core::SecretStore {
 public:
  bool write(const char* name, const std::string& value) override {
    values[name] = value;
    return true;
  }
  bool erase(const char* name) override {
    values.erase(name);
    return true;
  }
  bool read(const char* name, std::string& value) override {
    if (values.count(name) == 0) return false;
    value = values[name];
    return true;
  }
  std::map<std::string, std::string> values;
};

JsonDocument fixture(const char* name, const JsonDocument* filter = nullptr) {
  std::ifstream file(std::string("test/fixtures/settings/") + name, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  JsonDocument doc;
  if (filter != nullptr) {
    deserializeJson(doc, buffer.str(), DeserializationOption::Filter(*filter));
  } else {
    deserializeJson(doc, buffer.str());
  }
  return doc;
}

JsonDocument parse(const std::string& json) {
  JsonDocument doc;
  deserializeJson(doc, json);
  return doc;
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- Access ------------------------------------------------------------------

void test_open_shows_a_six_digit_code() {
  Counter random;
  SettingsSession s(std::ref(random));
  TEST_ASSERT_FALSE(s.isOpen(0));
  s.open(1000);
  TEST_ASSERT_TRUE(s.isOpen(1000));
  TEST_ASSERT_EQUAL_STRING("482913", s.code().c_str());
  random.next = 7;
  s.open(2000);  // a new code each time
  TEST_ASSERT_EQUAL_STRING("000007", s.code().c_str());
}

void test_login_with_the_code() {
  Counter random;
  SettingsSession s(std::ref(random));
  s.open(0);
  std::string token;
  TEST_ASSERT_TRUE(s.login("111111", 1000, token) == SettingsSession::Login::kWrongCode);
  TEST_ASSERT_TRUE(token.empty());
  TEST_ASSERT_TRUE(s.login("482 913", 2000, token) == SettingsSession::Login::kOk);
  TEST_ASSERT_EQUAL_size_t(32, token.size());
  TEST_ASSERT_TRUE(s.authorized(token, 3000));
  TEST_ASSERT_FALSE(s.authorized("", 3000));
  TEST_ASSERT_FALSE(s.authorized(token.substr(0, 31) + "x", 3000));
  // A second login (other browser) replaces the first session.
  std::string second;
  TEST_ASSERT_TRUE(s.login("482913", 4000, second) == SettingsSession::Login::kOk);
  TEST_ASSERT_FALSE(s.authorized(token, 5000));
  TEST_ASSERT_TRUE(s.authorized(second, 5000));
}

void test_five_wrong_codes_close_the_page() {
  Counter random;
  SettingsSession s(std::ref(random));
  s.open(0);
  std::string token;
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_TRUE(s.login("000000", 1000, token) == SettingsSession::Login::kWrongCode);
  }
  TEST_ASSERT_TRUE(s.login("000000", 1000, token) == SettingsSession::Login::kClosed);
  TEST_ASSERT_FALSE(s.isOpen(1000));
  TEST_ASSERT_TRUE(s.login("482913", 1000, token) == SettingsSession::Login::kClosed);
}

void test_closes_30_minutes_after_the_last_action() {
  Counter random;
  SettingsSession s(std::ref(random));
  s.open(0);
  std::string token;
  s.login("482913", kMinute, token);  // a login counts as an action
  s.touch(20 * kMinute);
  TEST_ASSERT_TRUE(s.authorized(token, 49 * kMinute));
  TEST_ASSERT_EQUAL_INT64(50 * kMinute, s.closesAtMs());
  TEST_ASSERT_FALSE(s.isOpen(50 * kMinute));
  TEST_ASSERT_FALSE(s.authorized(token, 50 * kMinute));
  s.touch(51 * kMinute);  // does not open it again
  TEST_ASSERT_FALSE(s.isOpen(51 * kMinute));
}

void test_open_again_after_a_restart() {
  Counter random;
  SettingsSession s(std::ref(random));
  s.open(0);
  std::string token;
  s.login("482913", 0, token);
  const SettingsSession::Snapshot snap = s.snapshot(10 * kMinute);
  TEST_ASSERT_EQUAL_INT64(20 * kMinute, snap.remaining_ms);

  // New start: the clock begins at 0 again.
  SettingsSession after(std::ref(random));
  after.restore(snap, 0);
  TEST_ASSERT_TRUE(after.isOpen(0));
  TEST_ASSERT_EQUAL_STRING("482913", after.code().c_str());
  TEST_ASSERT_TRUE(after.authorized(token, 1000));
  TEST_ASSERT_FALSE(after.isOpen(20 * kMinute));

  // Invalid or used up: stays closed.
  SettingsSession other(std::ref(random));
  other.restore(SettingsSession::Snapshot{}, 0);
  TEST_ASSERT_FALSE(other.isOpen(0));
  TEST_ASSERT_EQUAL_UINT32(0, s.snapshot(31 * kMinute).magic);
}

void test_closing_time_on_the_display() {
  // Moves on the display only in steps of 5 minutes (each refresh flashes).
  TEST_ASSERT_EQUAL_INT64(30 * kMinute, SettingsSession::shownClose(0, 30 * kMinute));
  TEST_ASSERT_EQUAL_INT64(30 * kMinute, SettingsSession::shownClose(30 * kMinute, 34 * kMinute));
  TEST_ASSERT_EQUAL_INT64(35 * kMinute, SettingsSession::shownClose(30 * kMinute, 35 * kMinute));
  TEST_ASSERT_EQUAL_INT64(20 * kMinute, SettingsSession::shownClose(30 * kMinute, 20 * kMinute));
}

void test_token_from_the_cookie_header() {
  TEST_ASSERT_EQUAL_STRING("abc", SettingsSession::tokenFromCookie("inktavla_session=abc").c_str());
  TEST_ASSERT_EQUAL_STRING(
      "abc", SettingsSession::tokenFromCookie("x=1; inktavla_session=abc; y=2").c_str());
  TEST_ASSERT_EQUAL_STRING("", SettingsSession::tokenFromCookie("my_inktavla_session=abc").c_str());
  TEST_ASSERT_EQUAL_STRING("", SettingsSession::tokenFromCookie("").c_str());
}

// --- Saving ------------------------------------------------------------------

void test_save_a_valid_configuration_and_a_secret() {
  MemoryStore store;
  MemorySecrets secrets;
  const core::SettingsSaveResult r = core::saveSettings(
      R"({"config": {"language": "en", "widgets": {"weather": {"latitude": 47.8766,
        "longitude": 11.7018, "name": "Holzkirchen"}}},
        "secrets": {"calendar_url": "webcal://example.com/basic.ics"}})",
      store, widgets::validateConfig, &secrets);
  TEST_ASSERT_EQUAL_INT(200, r.status);
  TEST_ASSERT_TRUE(r.restart);
  TEST_ASSERT_TRUE(parse(r.json)["saved"].as<bool>());
  const JsonDocument saved = parse(store.files["/config.json"]);
  TEST_ASSERT_EQUAL_STRING("en", saved["language"]);
  TEST_ASSERT_EQUAL_STRING("Holzkirchen", saved["widgets"]["weather"]["name"]);
  TEST_ASSERT_EQUAL_size_t(0, store.files.count("/config.json.tmp"));
  TEST_ASSERT_EQUAL_STRING("webcal://example.com/basic.ics",
                           secrets.values["calendar_url"].c_str());
  // The secret is never part of the file.
  TEST_ASSERT_EQUAL_size_t(std::string::npos, store.files["/config.json"].find("example.com"));
}

void test_problems_are_returned_and_nothing_is_saved() {
  MemoryStore store;
  store.files["/config.json"] = "{\"language\": \"de\"}";
  MemorySecrets secrets;
  const std::string body =
      R"({"config": {"display": {"full_refresh_minutes": 0}, "langauge": "en"}})";
  core::SettingsSaveResult r = core::saveSettings(body, store, widgets::validateConfig, &secrets);
  TEST_ASSERT_EQUAL_INT(422, r.status);
  TEST_ASSERT_FALSE(r.restart);
  const JsonDocument reply = parse(r.json);
  TEST_ASSERT_FALSE(reply["saved"].as<bool>());
  TEST_ASSERT_EQUAL_size_t(2, reply["issues"].size());  // the value and the typing error
  bool refresh = false;
  for (JsonObjectConst i : reply["issues"].as<JsonArrayConst>()) {
    refresh = refresh || i["path"] == "display.full_refresh_minutes";
    TEST_ASSERT_FALSE(i["blocking"].as<bool>());
  }
  TEST_ASSERT_TRUE(refresh);
  TEST_ASSERT_EQUAL_STRING("{\"language\": \"de\"}", store.files["/config.json"].c_str());

  // "Save anyway": invalid values fall back to their defaults, like over USB.
  r = core::saveSettings(R"({"force": true, "config": {"display": {"full_refresh_minutes": 0}}})",
                         store, widgets::validateConfig, &secrets);
  TEST_ASSERT_EQUAL_INT(200, r.status);
  TEST_ASSERT_TRUE(r.restart);
}

void test_a_bad_secret_is_never_saved() {
  MemoryStore store;
  MemorySecrets secrets;
  secrets.values["calendar_url"] = "https://example.com/old.ics";
  core::SettingsSaveResult r = core::saveSettings(
      R"({"force": true, "config": {}, "secrets": {"calendar_url": "http://example.com/a.ics"}})",
      store, widgets::validateConfig, &secrets);
  TEST_ASSERT_EQUAL_INT(422, r.status);
  const JsonDocument reply = parse(r.json);
  TEST_ASSERT_EQUAL_STRING("secrets.calendar_url", reply["issues"][0]["path"]);
  TEST_ASSERT_TRUE(reply["issues"][0]["blocking"].as<bool>());
  TEST_ASSERT_EQUAL_size_t(0, store.files.count("/config.json"));
  // The message never repeats the value.
  TEST_ASSERT_EQUAL_size_t(std::string::npos, r.json.find("example.com"));
  TEST_ASSERT_EQUAL_STRING("https://example.com/old.ics", secrets.values["calendar_url"].c_str());

  // null removes it.
  r = core::saveSettings(R"({"config": {}, "secrets": {"calendar_url": null}})", store,
                         widgets::validateConfig, &secrets);
  TEST_ASSERT_EQUAL_INT(200, r.status);
  TEST_ASSERT_EQUAL_size_t(0, secrets.values.count("calendar_url"));
}

void test_bad_requests_and_write_errors() {
  MemoryStore store;
  MemorySecrets secrets;
  TEST_ASSERT_EQUAL_INT(400, core::saveSettings("[1]", store, nullptr, &secrets).status);
  TEST_ASSERT_EQUAL_INT(400, core::saveSettings("{", store, nullptr, &secrets).status);
  TEST_ASSERT_EQUAL_INT(400, core::saveSettings("{\"x\": 1}", store, nullptr, &secrets).status);
  store.fail_write = true;
  TEST_ASSERT_EQUAL_INT(500,
                        core::saveSettings("{\"config\": {}}", store, nullptr, &secrets).status);
}

void test_reset_and_stored_configuration() {
  MemoryStore store;
  std::string problem;
  TEST_ASSERT_EQUAL_STRING("{}", core::storedConfig(store, &problem).c_str());
  TEST_ASSERT_EQUAL_STRING("no configuration file", problem.c_str());
  store.files["/config.json"] = "{\"language\": \"en\"}";
  TEST_ASSERT_EQUAL_STRING("{\"language\": \"en\"}", core::storedConfig(store).c_str());
  store.files["/config.json"] = "{broken";
  TEST_ASSERT_EQUAL_STRING("{}", core::storedConfig(store).c_str());
  const core::SettingsSaveResult r = core::resetSettings(store);
  TEST_ASSERT_TRUE(r.restart);
  TEST_ASSERT_EQUAL_STRING("{}\n", store.files["/config.json"].c_str());
}

// --- Setup help of the widgets ----------------------------------------------------

void test_mvg_station_search() {
  const JsonDocument filter = widgets::mvg::locationsFilter();
  const JsonDocument doc = fixture("mvg_locations_pasing_20261025.json", &filter);
  const JsonDocument out = parse(widgets::mvg::stationsJson(doc.as<JsonArrayConst>()));
  TEST_ASSERT_EQUAL_size_t(7, out.size());  // points of interest are left out
  TEST_ASSERT_EQUAL_STRING("de:09162:10", out[0]["id"]);
  TEST_ASSERT_EQUAL_STRING("Pasing", out[0]["name"]);
  TEST_ASSERT_EQUAL_STRING("München", out[0]["place"]);
  TEST_ASSERT_EQUAL_STRING("STBZ", out[0]["types"]);
  TEST_ASSERT_EQUAL_STRING("Gräfelfing", out[6]["place"]);

  // Outside Munich the place tells the stations apart.
  const JsonDocument holz = fixture("mvg_locations_holzkirchen_20261025.json", &filter);
  const JsonDocument h = parse(widgets::mvg::stationsJson(holz.as<JsonArrayConst>(), 3));
  TEST_ASSERT_EQUAL_size_t(3, h.size());
  TEST_ASSERT_EQUAL_STRING("Holzkirchen (Kr Miesbach)", h[0]["place"]);
  TEST_ASSERT_EQUAL_STRING("Alling (Obb.)", h[1]["place"]);
}

void test_mvg_lines_with_example_destinations() {
  widgets::mvg::LineCatalog catalog;
  const JsonDocument lines = fixture("mvg_lines_pasing_20261025.json");
  catalog.addLines(lines.as<JsonArrayConst>());
  // 43 entries without the two replacement services, "RE 96" and "RE96" are
  // one line.
  TEST_ASSERT_EQUAL_size_t(40, catalog.size());

  std::ifstream file("test/fixtures/mvg/pasing_20261019.json", std::ios::binary);
  JsonDocument deps;
  const JsonDocument filter = widgets::mvg::sampleDeparturesFilter();
  deserializeJson(deps, file, DeserializationOption::Filter(filter));
  catalog.addDepartures(deps.as<JsonArrayConst>());

  const JsonDocument out = parse(catalog.toJson());
  // S-Bahn first, by number: S2, S3, S4, ...
  TEST_ASSERT_EQUAL_STRING("S2", out[0]["key"]);
  TEST_ASSERT_EQUAL_STRING("S", out[0]["type"]);
  TEST_ASSERT_EQUAL_STRING("S3", out[1]["key"]);
  JsonObjectConst s4;
  JsonObjectConst re80;
  JsonObjectConst n19;
  JsonObjectConst tram14;
  for (JsonObjectConst l : out.as<JsonArrayConst>()) {
    const std::string key = l["key"];
    if (key == "S4") s4 = l;
    if (key == "RE80") re80 = l;
    if (key == "N19") n19 = l;
    if (key == "14") tram14 = l;
  }
  TEST_ASSERT_FALSE(s4.isNull());
  TEST_ASSERT_EQUAL_STRING("Trudering", s4["H"][0]);
  TEST_ASSERT_TRUE(s4["H"].size() <= widgets::mvg::LineCatalog::kDestinations);
  TEST_ASSERT_EQUAL_STRING("RE 80", re80["label"]);
  TEST_ASSERT_EQUAL_STRING("Z", re80["type"]);
  TEST_ASSERT_EQUAL_STRING("T", n19["type"]);
  // Night tram after the day trams.
  size_t i14 = 0;
  size_t iN19 = 0;
  for (size_t i = 0; i < out.size(); i++) {
    if (out[i]["key"] == "14") i14 = i;
    if (out[i]["key"] == "N19") iN19 = i;
  }
  TEST_ASSERT_TRUE(i14 < iN19);
  TEST_ASSERT_FALSE(tram14.isNull());
  // No replacement services, no collective labels.
  TEST_ASSERT_EQUAL_size_t(std::string::npos, catalog.toJson().find("SEV"));
  TEST_ASSERT_EQUAL_size_t(std::string::npos, catalog.toJson().find('/'));
}

void test_mvg_setup_urls() {
  TEST_ASSERT_EQUAL_STRING("https://www.mvg.de/api/bgw-pt/v3/locations?query=M%C3%BCnchen%20Hbf",
                           widgets::mvg::locationsUrl("München Hbf").c_str());
  TEST_ASSERT_EQUAL_STRING("https://www.mvg.de/api/bgw-pt/v3/lines/de%3A09162%3A10",
                           widgets::mvg::linesUrl("de:09162:10").c_str());
  TEST_ASSERT_EQUAL_STRING(
      "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=de%3A09162%3A10&limit=100"
      "&offsetInMinutes=120&transportTypes=SBAHN,UBAHN,TRAM,BUS,REGIONAL_BUS,BAHN",
      widgets::mvg::sampleDeparturesUrl("de:09162:10", 120).c_str());
}

void test_weather_place_search() {
  const JsonDocument filter = widgets::weather::placeSearchFilter();
  const JsonDocument doc = fixture("openmeteo_geocoding_holzkirchen_20261025.json", &filter);
  const JsonDocument out = parse(widgets::weather::placesJson(doc.as<JsonObjectConst>()));
  TEST_ASSERT_EQUAL_size_t(5, out.size());
  TEST_ASSERT_EQUAL_STRING("Holzkirchen", out[1]["name"]);
  TEST_ASSERT_EQUAL_STRING("Bayern, Landkreis Miesbach", out[1]["region"]);
  TEST_ASSERT_EQUAL_INT(478766, static_cast<int>(out[1]["lat"].as<double>() * 10000 + 0.5));
  TEST_ASSERT_EQUAL_INT(117018, static_cast<int>(out[1]["lon"].as<double>() * 10000 + 0.5));
  TEST_ASSERT_EQUAL_STRING(
      "https://geocoding-api.open-meteo.com/v1/search?name=Holzkirchen&count=8&language=de"
      "&format=json",
      widgets::weather::placeSearchUrl("Holzkirchen", "de").c_str());
  // Nothing found: an empty list.
  TEST_ASSERT_EQUAL_STRING("[]",
                           widgets::weather::placesJson(parse("{}").as<JsonObjectConst>()).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_open_shows_a_six_digit_code);
  RUN_TEST(test_login_with_the_code);
  RUN_TEST(test_five_wrong_codes_close_the_page);
  RUN_TEST(test_closes_30_minutes_after_the_last_action);
  RUN_TEST(test_open_again_after_a_restart);
  RUN_TEST(test_closing_time_on_the_display);
  RUN_TEST(test_token_from_the_cookie_header);
  RUN_TEST(test_save_a_valid_configuration_and_a_secret);
  RUN_TEST(test_problems_are_returned_and_nothing_is_saved);
  RUN_TEST(test_a_bad_secret_is_never_saved);
  RUN_TEST(test_bad_requests_and_write_errors);
  RUN_TEST(test_reset_and_stored_configuration);
  RUN_TEST(test_mvg_station_search);
  RUN_TEST(test_mvg_lines_with_example_destinations);
  RUN_TEST(test_mvg_setup_urls);
  RUN_TEST(test_weather_place_search);
  return UNITY_END();
}
