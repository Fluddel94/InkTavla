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
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kClock);
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
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kClock);
  TEST_ASSERT_TRUE(c.show_incidents);  // disruption text on by default
  // With a line selection the request uses the types of the selected lines.
  TEST_ASSERT_EQUAL_STRING("SBAHN,BAHN", widgets::mvg::transportTypesParam(c).c_str());
}

void test_values_are_read() {
  ConfigIssues issues;
  const Config c =
      parse(R"({"station":"de:09182:43227","name":"Holzkirchen Bf","note":"Bahnhof",)"
            R"("transport_types":["BAHN","SBAHN","BAHN"],"direction":"R","time_format":"minutes"})",
            issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_STRING("de:09182:43227", c.station.c_str());
  TEST_ASSERT_EQUAL_STRING("Holzkirchen Bf", c.name.c_str());
  TEST_ASSERT_EQUAL_STRING("Bahnhof", c.note.c_str());
  TEST_ASSERT_TRUE(c.direction == DirectionFilter::kR);
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kMinutes);
  // Duplicates dropped, fixed order in the request.
  TEST_ASSERT_EQUAL_size_t(2, c.transport_types.size());
  TEST_ASSERT_EQUAL_STRING("SBAHN,BAHN", widgets::mvg::transportTypesParam(c).c_str());
}

void test_incidents_switch() {
  ConfigIssues issues;
  TEST_ASSERT_FALSE(parse(R"({"station":"de:09162:10","incidents":false})", issues).show_incidents);
  TEST_ASSERT_TRUE(parse(R"({"station":"de:09162:10","incidents":true})", issues).show_incidents);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  // A wrong type is reported and keeps the default (on).
  const Config c = parse(R"({"station":"de:09162:10","incidents":"no"})", issues);
  TEST_ASSERT_TRUE(c.show_incidents);
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.incidents"));
}

void test_station_id_format() {
  TEST_ASSERT_TRUE(widgets::mvg::isValidStationId("de:09162:10"));
  TEST_ASSERT_TRUE(widgets::mvg::isValidStationId("de:09162:5"));
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
  TEST_ASSERT_TRUE(c.time_format == TimeFormat::kClock);
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

void test_sections_are_read() {
  ConfigIssues issues;
  const Config c = parse(R"({"sections":[{"name":"Stadt","lines":["S3:H:S","19:H:T"]},)"
                         R"({"name":"Raus","lines":["S3:R:S","14:H:T","RB6:H:Z"]}]})",
                         issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_size_t(2, c.sections.size());
  TEST_ASSERT_EQUAL_STRING("Stadt", c.sections[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("S3:H:S,19:H:T", c.sections[0].lines.toString().c_str());
  TEST_ASSERT_EQUAL_STRING("Raus", c.sections[1].name.c_str());
  TEST_ASSERT_EQUAL_STRING("S3:R:S,14:H:T,RB6:H:Z", c.sections[1].lines.toString().c_str());
  // Request: types of both sections in the fixed order.
  TEST_ASSERT_EQUAL_STRING("SBAHN,TRAM,BAHN", widgets::mvg::transportTypesParam(c).c_str());
}

void test_sections_replace_lines_and_direction() {
  ConfigIssues issues;
  const Config c = parse(R"({"lines":["S8:B:S"],"direction":"H","sections":[)"
                         R"({"name":"A","lines":["S3:H:S"]},{"name":"B","lines":["S3:R:S"]}]})",
                         issues);
  TEST_ASSERT_EQUAL_size_t(2, c.sections.size());
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.lines"));
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.direction"));
  TEST_ASSERT_EQUAL_STRING("SBAHN", widgets::mvg::transportTypesParam(c).c_str());
}

void test_sections_with_stations_of_their_own() {
  ConfigIssues issues;
  const Config c = parse(R"({"station":"de:09162:5","sections":[)"
                         R"({"name":"Ostbahnhof","lines":["S3:H:S"],"note":"Bahnhof"},)"
                         R"({"name":"Leuchtenbergring","station":"de:09162:900",)"
                         R"("note":"4 min zu Fuß"}]})",
                         issues);
  TEST_ASSERT_EQUAL_size_t(0, issues.size());
  TEST_ASSERT_EQUAL_size_t(2, c.sections.size());
  TEST_ASSERT_EQUAL_STRING("Bahnhof", c.sections[0].note.c_str());
  TEST_ASSERT_EQUAL_STRING("", c.sections[0].station.c_str());
  TEST_ASSERT_EQUAL_STRING("de:09162:5", c.sectionStation(0).c_str());
  TEST_ASSERT_EQUAL_STRING("de:09162:900", c.sectionStation(1).c_str());
  TEST_ASSERT_TRUE(c.sections[1].lines.empty());  // all lines of that station
  TEST_ASSERT_TRUE(c.hasStation());
  TEST_ASSERT_TRUE(c.hasOwnStations());
  // A section without lines needs all means of transport.
  TEST_ASSERT_EQUAL_STRING("SBAHN,UBAHN,TRAM,BUS,REGIONAL_BUS,BAHN",
                           widgets::mvg::transportTypesParam(c).c_str());

  // Without the widget's station every section needs one of its own.
  ConfigIssues more;
  const Config d = parse(R"({"sections":[{"name":"A","station":"de:09162:5"},)"
                         R"({"name":"B","station":"de:09162:900"}]})",
                         more);
  TEST_ASSERT_EQUAL_size_t(0, more.size());
  TEST_ASSERT_TRUE(d.hasStation());
  const Config e = parse(R"({"sections":[{"name":"A","station":"de:09162:5"},)"
                         R"({"name":"B","lines":["S3:R:S"]}]})",
                         more);
  TEST_ASSERT_FALSE(e.hasStation());
  TEST_ASSERT_FALSE(Config().hasStation());
}

void test_invalid_section_station_gives_one_list() {
  ConfigIssues issues;
  const Config c = parse(R"({"station":"de:09162:5","sections":[{"name":"A","station":"Pasing"},)"
                         R"({"name":"B","lines":["S3:R:S"]}]})",
                         issues);
  TEST_ASSERT_TRUE(c.sections.empty());
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.sections[0].station"));
  ConfigIssues long_note;
  parse(R"({"station":"de:09162:5","sections":[{"name":"A","lines":["S3:H:S"],)"
        R"("note":"Ein viel zu langer Zusatztext"},{"name":"B","lines":["S3:R:S"]}]})",
        long_note);
  TEST_ASSERT_TRUE(hasIssue(long_note, "widgets.mvg.sections[0].note"));
}

void test_invalid_sections_give_one_list() {
  const char* cases[][2] = {
      {R"({"sections":{"name":"A"}})", "widgets.mvg.sections"},
      {R"({"sections":[{"name":"A","lines":["S3:H:S"]}]})", "widgets.mvg.sections"},
      {R"({"sections":[{"name":"A","lines":["S3:H:S"]},{"name":"B","lines":["S3:R:S"]},)"
       R"({"name":"C","lines":["S4:R:S"]}]})",
       "widgets.mvg.sections"},
      {R"({"sections":[{"name":"","lines":["S3:H:S"]},{"name":"B","lines":["S3:R:S"]}]})",
       "widgets.mvg.sections[0].name"},
      {R"({"sections":[{"name":"A","lines":[]},{"name":"B","lines":["S3:R:S"]}]})",
       "widgets.mvg.sections[0].lines"},
      {R"({"sections":[{"name":"A","lines":["S3:H:S"]},{"name":"B","lines":["S3:X:S"]}]})",
       "widgets.mvg.sections[1].lines"},
      {R"({"sections":[{"name":"A","lines":["S3:H:S"]},"B"]})", "widgets.mvg.sections[1]"},
      {R"({"sections":[{"name":"Sehr langer Abschnittsname!","lines":["S3:H:S"]},)"
       R"({"name":"B","lines":["S3:R:S"]}]})",
       "widgets.mvg.sections[0].name"},
  };
  for (const auto& test : cases) {
    ConfigIssues issues;
    const Config c = parse(test[0], issues);
    TEST_ASSERT_TRUE_MESSAGE(c.sections.empty(), test[0]);
    TEST_ASSERT_TRUE_MESSAGE(hasIssue(issues, test[1]), test[0]);
  }
  // Unknown keys inside a section are only reported.
  ConfigIssues issues;
  const Config c = parse(R"({"sections":[{"name":"A","lines":["S3:H:S"],"colour":1},)"
                         R"({"name":"B","lines":["S3:R:S"]}]})",
                         issues);
  TEST_ASSERT_EQUAL_size_t(2, c.sections.size());
  TEST_ASSERT_TRUE(hasIssue(issues, "widgets.mvg.sections[0].colour"));
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
  RUN_TEST(test_sections_are_read);
  RUN_TEST(test_sections_replace_lines_and_direction);
  RUN_TEST(test_incidents_switch);
  RUN_TEST(test_invalid_sections_give_one_list);
  RUN_TEST(test_sections_with_stations_of_their_own);
  RUN_TEST(test_invalid_section_station_gives_one_list);
  return UNITY_END();
}
