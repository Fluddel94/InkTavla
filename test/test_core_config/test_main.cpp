// Unit tests for reading the configuration file (core/config.h).

#include <unity.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "core/config.h"
#include "core/time.h"

using core::ConfigIssues;
using core::CoreConfig;

namespace {

// Parses text and returns the core settings; issues are collected.
CoreConfig parse(const std::string& json, ConfigIssues& issues) {
  JsonDocument doc;
  core::readConfig(json.data(), json.size(), doc, issues);
  return core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
}

bool hasIssue(const ConfigIssues& issues, const char* path) {
  for (const core::ConfigIssue& i : issues) {
    if (i.path == path) return true;
  }
  return false;
}

std::string readFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void assertDefaults(const CoreConfig& c) {
  TEST_ASSERT_EQUAL_INT(1, c.version);
  TEST_ASSERT_EQUAL_STRING(core::kDefaultTimeZone, c.time_zone.c_str());
  TEST_ASSERT_EQUAL_STRING("de", c.language.c_str());
  TEST_ASSERT_EQUAL_STRING("pool.ntp.org", c.ntp_server.c_str());
  TEST_ASSERT_EQUAL_INT(360, c.full_refresh_minutes);
  TEST_ASSERT_EQUAL_size_t(3, c.slots.size());
  TEST_ASSERT_EQUAL_STRING("mvg", c.slots[0].c_str());
  TEST_ASSERT_EQUAL_STRING("calendar", c.slots[1].c_str());
  TEST_ASSERT_EQUAL_STRING("weather", c.slots[2].c_str());
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_empty_object_gives_defaults_without_issues() {
  ConfigIssues issues;
  assertDefaults(parse("{}", issues));
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
}

void test_example_file_is_valid() {
  // docs/config.example.json is the example from docs/configuration.md.
  const std::string text = readFile("docs/config.example.json");
  TEST_ASSERT_FALSE(text.empty());
  ConfigIssues issues;
  JsonDocument doc;
  TEST_ASSERT_TRUE(core::readConfig(text.data(), text.size(), doc, issues));
  const CoreConfig c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  for (const core::ConfigIssue& i : issues) TEST_FAIL_MESSAGE((i.path + ": " + i.message).c_str());
  assertDefaults(c);
  TEST_ASSERT_FALSE(core::widgetSection(doc.as<JsonObjectConst>(), "mvg").isNull());
  TEST_ASSERT_FALSE(core::widgetSection(doc.as<JsonObjectConst>(), "weather").isNull());
  TEST_ASSERT_TRUE(core::widgetSection(doc.as<JsonObjectConst>(), "calendar").isNull());
}

void test_values_are_read() {
  ConfigIssues issues;
  const CoreConfig c = parse(R"({"time_zone":"UTC0","language":"en","ntp_server":"192.168.1.1",)"
                             R"("display":{"full_refresh_minutes":30},)"
                             R"("layout":{"slots":["weather","mvg","calendar"]}})",
                             issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_STRING("UTC0", c.time_zone.c_str());
  TEST_ASSERT_EQUAL_STRING("en", c.language.c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.1.1", c.ntp_server.c_str());
  TEST_ASSERT_EQUAL_INT(30, c.full_refresh_minutes);
  TEST_ASSERT_EQUAL_STRING("weather", c.slots[0].c_str());
}

void test_broken_json_gives_defaults_with_issue() {
  ConfigIssues issues;
  JsonDocument doc;
  const std::string text = R"({"language": "en",)";
  TEST_ASSERT_FALSE(core::readConfig(text.data(), text.size(), doc, issues));
  TEST_ASSERT_TRUE(doc.isNull());
  TEST_ASSERT_EQUAL_size_t(1, issues.size());
  assertDefaults(core::parseCoreConfig(doc.as<JsonObjectConst>(), issues));
}

void test_root_must_be_an_object() {
  ConfigIssues issues;
  assertDefaults(parse("[1, 2]", issues));
  TEST_ASSERT_TRUE(hasIssue(issues, ""));
}

void test_too_large_file_is_rejected() {
  std::string text = R"({"language":"en","padding":")";
  text.append(core::kMaxConfigBytes, 'x');
  text += "\"}";
  ConfigIssues issues;
  JsonDocument doc;
  TEST_ASSERT_FALSE(core::readConfig(text.data(), text.size(), doc, issues));
  TEST_ASSERT_TRUE(doc.isNull());
  TEST_ASSERT_TRUE(issues[0].message.find("larger") != std::string::npos);
}

void test_invalid_values_fall_back_with_issues() {
  ConfigIssues issues;
  const CoreConfig c =
      parse(R"({"time_zone":"Europe/Berlin","language":"fr","ntp_server":"pool ntp",)"
            R"("display":{"full_refresh_minutes":0},)"
            R"("layout":{"slots":["mvg"]}})",
            issues);
  assertDefaults(c);
  TEST_ASSERT_TRUE(hasIssue(issues, "time_zone"));
  TEST_ASSERT_TRUE(hasIssue(issues, "language"));
  TEST_ASSERT_TRUE(hasIssue(issues, "ntp_server"));
  TEST_ASSERT_TRUE(hasIssue(issues, "display.full_refresh_minutes"));
  TEST_ASSERT_TRUE(hasIssue(issues, "layout.slots"));  // 3 slots needed
}

void test_wrong_types_fall_back_with_issues() {
  ConfigIssues issues;
  const CoreConfig c = parse(R"({"time_zone":1,"display":{"full_refresh_minutes":"60"},)"
                             R"("layout":{"slots":["mvg","Weather!",3]}})",
                             issues);
  assertDefaults(c);
  TEST_ASSERT_TRUE(hasIssue(issues, "time_zone"));
  TEST_ASSERT_TRUE(hasIssue(issues, "display.full_refresh_minutes"));
  TEST_ASSERT_TRUE(hasIssue(issues, "layout.slots"));
}

void test_unknown_keys_are_reported() {
  ConfigIssues issues;
  assertDefaults(parse(R"({"langauge":"en","display":{"full_refresh":5}})", issues));
  TEST_ASSERT_EQUAL_size_t(2, issues.size());
  TEST_ASSERT_TRUE(hasIssue(issues, "langauge"));
  TEST_ASSERT_TRUE(hasIssue(issues, "display.full_refresh"));
}

void test_newer_version_is_reported() {
  ConfigIssues issues;
  const CoreConfig c = parse(R"({"version":2,"language":"en"})", issues);
  TEST_ASSERT_EQUAL_INT(2, c.version);
  TEST_ASSERT_EQUAL_STRING("en", c.language.c_str());
  TEST_ASSERT_TRUE(hasIssue(issues, "version"));
}

void test_helpers() {
  JsonDocument doc;
  deserializeJson(doc, R"({"a":"text","n":7,"b":true,"c":"two"})");
  JsonObjectConst o = doc.as<JsonObjectConst>();
  ConfigIssues issues;
  TEST_ASSERT_EQUAL_STRING("text", core::readString(o, "a", "x", 10, "w", issues).c_str());
  TEST_ASSERT_EQUAL_STRING("x", core::readString(o, "a", "x", 3, "w", issues).c_str());
  TEST_ASSERT_EQUAL_STRING("x", core::readString(o, "missing", "x", 10, "w", issues).c_str());
  TEST_ASSERT_EQUAL_INT(7, core::readInt(o, "n", 1, 0, 10, "w", issues));
  TEST_ASSERT_EQUAL_INT(1, core::readInt(o, "n", 1, 0, 5, "w", issues));
  TEST_ASSERT_TRUE(core::readBool(o, "b", false, "w", issues));
  TEST_ASSERT_FALSE(core::readBool(o, "a", false, "w", issues));
  TEST_ASSERT_EQUAL_INT(1, core::readChoice(o, "c", {"one", "two"}, 0, "w", issues));
  TEST_ASSERT_EQUAL_INT(0, core::readChoice(o, "a", {"one", "two"}, 0, "w", issues));
  TEST_ASSERT_EQUAL_size_t(4, issues.size());  // too long, out of range, not bool, no choice
  TEST_ASSERT_EQUAL_STRING("w.a", issues[0].path.c_str());
}

// Settings removed before 1.0 ("template", "grid") are unknown keys now:
// reported like a typing error, the rest is read as usual.
void test_removed_layout_settings_are_unknown() {
  JsonDocument doc;
  core::ConfigIssues issues;
  deserializeJson(doc, R"({"layout": {"template": "briefing", "grid": "equal",
                           "slots": ["mvg", "weather", "calendar"]}})");
  core::CoreConfig c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(2, issues.size());
  TEST_ASSERT_EQUAL_STRING("layout.template", issues[0].path.c_str());
  TEST_ASSERT_EQUAL_STRING("layout.grid", issues[1].path.c_str());
  TEST_ASSERT_EQUAL_STRING("unknown setting, ignored", issues[1].message.c_str());
  TEST_ASSERT_EQUAL_STRING("weather", c.slots[1].c_str());
}

void test_layout_variants() {
  JsonDocument doc;
  core::ConfigIssues issues;
  core::CoreConfig c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_TRUE(c.dividers);  // on by default
  deserializeJson(doc, R"({"layout": {"dividers": false}})");
  c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_FALSE(c.dividers);
  deserializeJson(doc, R"({"layout": {"dividers": "yes"}})");
  c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(1, issues.size());
  TEST_ASSERT_EQUAL_STRING("layout.dividers", issues[0].path.c_str());
  TEST_ASSERT_TRUE(c.dividers);
}

void test_layout_slots_two_or_three_entries() {
  JsonDocument doc;
  core::ConfigIssues issues;
  // Two entries are the layout band + 2, three band + 3.
  deserializeJson(doc, R"({"layout": {"slots": ["weather", "mvg"]}})");
  core::CoreConfig c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_size_t(2, c.slots.size());
  TEST_ASSERT_EQUAL_STRING("weather", c.slots[0].c_str());
  deserializeJson(doc, R"({"layout": {"slots": ["weather", "calendar", "mvg"]}})");
  c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_size_t(3, c.slots.size());
  // Any other number: reported, the standard is used.
  for (const char* text : {R"({"layout": {"slots": ["mvg"]}})", R"({"layout": {"slots": []}})",
                           R"({"layout": {"slots": ["mvg", "calendar", "weather", "mvg"]}})"}) {
    issues.clear();
    deserializeJson(doc, text);
    c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
    TEST_ASSERT_EQUAL_size_t(1, issues.size());
    TEST_ASSERT_EQUAL_STRING("layout.slots", issues[0].path.c_str());
    TEST_ASSERT_EQUAL_STRING("must have 2 or 3 entries", issues[0].message.c_str());
    TEST_ASSERT_EQUAL_size_t(3, c.slots.size());
    TEST_ASSERT_EQUAL_STRING("mvg", c.slots[0].c_str());
  }
  issues.clear();
  deserializeJson(doc, R"({"layout": {"slots": "mvg"}})");
  c = core::parseCoreConfig(doc.as<JsonObjectConst>(), issues);
  TEST_ASSERT_EQUAL_STRING("must be a list of widget IDs such as \"mvg\"",
                           issues[0].message.c_str());
}

void test_settings_page_opens_only_without_settings() {
  JsonDocument doc;
  ConfigIssues issues;
  TEST_ASSERT_TRUE(core::opensSettingsOnStart(false, false, doc.as<JsonVariantConst>()));
  TEST_ASSERT_TRUE(core::readConfig("{}\n", 3, doc, issues));
  TEST_ASSERT_TRUE(core::opensSettingsOnStart(true, true, doc.as<JsonVariantConst>()));
  const char* set_up = R"({"language": "en"})";
  TEST_ASSERT_TRUE(core::readConfig(set_up, strlen(set_up), doc, issues));
  TEST_ASSERT_FALSE(core::opensSettingsOnStart(true, true, doc.as<JsonVariantConst>()));
  // A broken file: its problems are shown, the page does not open by itself.
  const char* broken = "{\"language\": ";
  TEST_ASSERT_FALSE(core::readConfig(broken, strlen(broken), doc, issues));
  TEST_ASSERT_FALSE(core::opensSettingsOnStart(true, false, doc.as<JsonVariantConst>()));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_object_gives_defaults_without_issues);
  RUN_TEST(test_example_file_is_valid);
  RUN_TEST(test_values_are_read);
  RUN_TEST(test_broken_json_gives_defaults_with_issue);
  RUN_TEST(test_root_must_be_an_object);
  RUN_TEST(test_too_large_file_is_rejected);
  RUN_TEST(test_invalid_values_fall_back_with_issues);
  RUN_TEST(test_wrong_types_fall_back_with_issues);
  RUN_TEST(test_unknown_keys_are_reported);
  RUN_TEST(test_newer_version_is_reported);
  RUN_TEST(test_helpers);
  RUN_TEST(test_removed_layout_settings_are_unknown);
  RUN_TEST(test_layout_variants);
  RUN_TEST(test_layout_slots_two_or_three_entries);
  RUN_TEST(test_settings_page_opens_only_without_settings);
  return UNITY_END();
}
