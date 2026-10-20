// Unit tests for the MVG widget configuration (widgets/mvg/config.h).

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "widgets/mvg/config.h"

using core::ConfigIssues;
using widgets::mvg::Config;
using widgets::mvg::DirectionFilter;
using widgets::mvg::TimeFormat;
using widgets::mvg::TransportType;

namespace {

Config parse(const std::string& section, ConfigIssues& issues) {
  JsonDocument doc;
  deserializeJson(doc, section);
  return widgets::mvg::parseConfig(doc.as<JsonObjectConst>(), issues);
}

bool hasIssue(const ConfigIssues& issues, const char* path) {
  for (const core::ConfigIssue& i : issues) {
    if (i.path == path) return true;
  }
  return false;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_defaults() {
  ConfigIssues issues;
  const Config c = parse("{}", issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_TRUE(c.station.empty());
  TEST_ASSERT_EQUAL_size_t(6, c.transport_types.size());
  TEST_ASSERT_TRUE(c.lines.empty());
  TEST_ASSERT_TRUE(c.direction == DirectionFilter::kAll);
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kMinutes);
  TEST_ASSERT_EQUAL_STRING("SBAHN,UBAHN,TRAM,BUS,REGIONAL_BUS,BAHN",
                           widgets::mvg::transportTypesParam(c).c_str());
}

void test_missing_section_gives_defaults() {
  ConfigIssues issues;
  const Config c = widgets::mvg::parseConfig(JsonObjectConst(), issues);
  TEST_ASSERT_TRUE(c.station.empty());
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
}

void test_example_file_section() {
  std::ifstream file("docs/config.example.json", std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  JsonDocument doc;
  ConfigIssues issues;
  const std::string text = buffer.str();
  TEST_ASSERT_TRUE(core::readConfig(text.data(), text.size(), doc, issues));
  const Config c =
      widgets::mvg::parseConfig(core::widgetSection(doc.as<JsonObjectConst>(), "mvg"), issues);
  for (const core::ConfigIssue& i : issues) TEST_FAIL_MESSAGE((i.path + ": " + i.message).c_str());
  TEST_ASSERT_EQUAL_STRING("de:09162:10", c.station.c_str());
  TEST_ASSERT_EQUAL_STRING("Pasing", c.name.c_str());
  TEST_ASSERT_EQUAL_STRING("S3:B:S,RE80:R:Z", c.lines.toString().c_str());
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kMinutes);
  // With a line selection the request uses the types of the selected lines.
  TEST_ASSERT_EQUAL_STRING("SBAHN,BAHN", widgets::mvg::transportTypesParam(c).c_str());
}

void test_values_are_read() {
  ConfigIssues issues;
  const Config c =
      parse(R"({"station":"de:09182:43227","name":"Holzkirchen Bf",)"
            R"("transport_types":["BAHN","SBAHN","BAHN"],"direction":"R","time_format":"clock"})",
            issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_STRING("de:09182:43227", c.station.c_str());
  TEST_ASSERT_EQUAL_STRING("Holzkirchen Bf", c.name.c_str());
  TEST_ASSERT_TRUE(c.direction == DirectionFilter::kR);
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kClock);
  // Duplicates dropped, fixed order in the request.
  TEST_ASSERT_EQUAL_size_t(2, c.transport_types.size());
  TEST_ASSERT_EQUAL_STRING("SBAHN,BAHN", widgets::mvg::transportTypesParam(c).c_str());
}

void test_station_id_format() {
  TEST_ASSERT_TRUE(widgets::mvg::isValidStationId("de:09162:10"));
  TEST_ASSERT_TRUE(widgets::mvg::isValidStationId("de:09162:6"));
  TEST_ASSERT_FALSE(widgets::mvg::isValidStationId("Pasing"));
  TEST_ASSERT_FALSE(widgets::mvg::isValidStationId("de:09162:"));
  TEST_ASSERT_FALSE(widgets::mvg::isValidStationId("de::10"));
  TEST_ASSERT_FALSE(widgets::mvg::isValidStationId("DE:09162:10"));
  TEST_ASSERT_FALSE(widgets::mvg::isValidStationId("de:09162:10?x=1"));
}

void test_invalid_values_fall_back_with_issues() {
  ConfigIssues issues;
  const Config c =
      parse(R"({"station":"Pasing","transport_types":["SBAHN","FERRY"],"direction":"north",)"
            R"("time_format":"seconds","colour":"red"})",
            issues);
  TEST_ASSERT_TRUE(c.station.empty());
  TEST_ASSERT_EQUAL_size_t(6, c.transport_types.size());
  TEST_ASSERT_TRUE(c.direction == DirectionFilter::kAll);
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kMinutes);
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.station"));
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.transport_types"));
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.direction"));
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.time_format"));
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.colour"));
}

void test_empty_transport_types_fall_back() {
  ConfigIssues issues;
  const Config c = parse(R"({"transport_types":[]})", issues);
  TEST_ASSERT_EQUAL_size_t(6, c.transport_types.size());
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.transport_types"));
}

void test_lines_invalid_entries_are_dropped() {
  ConfigIssues issues;
  const Config c = parse(R"({"lines":["S2:H:S","S8:X:S","S3:B:S,S4:B:S",7,"re 80:b:z"]})", issues);
  TEST_ASSERT_EQUAL_STRING("S2:H:S,RE80:B:Z", c.lines.toString().c_str());
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.lines"));
}

void test_lines_limit() {
  std::string json = R"({"lines":[)";
  for (int i = 0; i < 20; i++)
    json += std::string(i ? "," : "") + "\"L" + std::to_string(i) + ":B:B\"";
  json += "]}";
  ConfigIssues issues;
  const Config c = parse(json, issues);
  TEST_ASSERT_EQUAL_INT(widgets::mvg::LineSelection::kMaxLines, c.lines.size());
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.lines"));
}

void test_lines_not_a_list() {
  ConfigIssues issues;
  const Config c = parse(R"({"lines":"S2:H:S"})", issues);
  TEST_ASSERT_TRUE(c.lines.empty());
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.lines"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_missing_section_gives_defaults);
  RUN_TEST(test_example_file_section);
  RUN_TEST(test_values_are_read);
  RUN_TEST(test_station_id_format);
  RUN_TEST(test_invalid_values_fall_back_with_issues);
  RUN_TEST(test_empty_transport_types_fall_back);
  RUN_TEST(test_lines_invalid_entries_are_dropped);
  RUN_TEST(test_lines_limit);
  RUN_TEST(test_lines_not_a_list);
  return UNITY_END();
}
