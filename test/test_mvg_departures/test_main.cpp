// Unit tests for reading and processing MVG departures
// (widgets/mvg/departures.h), one group per processing rule.
//
// Fixtures: test/fixtures/mvg/ (see the README there). parse_deps.json,
// pasing_deps.json and hk_deps.json come from the MVG departure display
// project; rules_*.json are hand-written cases for single rules.

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "widgets/mvg/departures.h"

using widgets::mvg::Departure;
using widgets::mvg::Direction;
using widgets::mvg::DirectionFilter;
using widgets::mvg::LineSelection;
using widgets::mvg::TransportType;

namespace {

// `pio test` runs the test program from the project directory.
void load(const char* fixture, JsonDocument& doc) {
  std::ifstream file(std::string("test/fixtures/mvg/") + fixture, std::ios::binary);
  TEST_ASSERT_TRUE_MESSAGE(file.is_open(), fixture);
  const DeserializationError error = widgets::mvg::readDepartures(file, doc);
  TEST_ASSERT_TRUE_MESSAGE(error == DeserializationError::Ok, error.c_str());
}

std::vector<Departure> parse(const char* fixture, const char* selection = "",
                             DirectionFilter filter = DirectionFilter::kAll,
                             size_t max_results = 100) {
  JsonDocument doc;
  load(fixture, doc);
  LineSelection lines;
  lines.parse(selection);
  return widgets::mvg::parseDepartures(doc, filter, lines, max_results);
}

// Compact description "line>destination" of all rows, joined with " | ".
// Flags: "!" warning, "x" cancelled.
std::string describe(const std::vector<Departure>& departures) {
  std::string text;
  for (const Departure& d : departures) {
    if (!text.empty()) text += " | ";
    text += d.line + ">" + d.destination;
    if (d.warning) text += "!";
    if (d.cancelled) text += "x";
  }
  return text;
}

void assertRows(const char* expected, const std::vector<Departure>& departures) {
  TEST_ASSERT_EQUAL_STRING(expected, describe(departures).c_str());
}

constexpr int64_t kRulesStart = 1792386000;  // 2026-10-19 05:00 UTC, rules_*.json

}  // namespace

void setUp() {}
void tearDown() {}

// --- Reading -----------------------------------------------------------------

void test_read_keeps_only_filtered_fields() {
  std::istringstream input(
      R"([{"label":"RE 80","destination":"Würzburg Hbf","plannedDepartureTime":1791030060000,)"
      R"("network":"ddb","divaId":"90M80","occupancy":"LOW","stopPointGlobalId":"de:09162:10:2:2",)"
      R"("infos":[{"type":"INFO","message":"Hinweis","network":"ddb"}],"messages":[]}])");
  JsonDocument doc;
  TEST_ASSERT_TRUE(widgets::mvg::readDepartures(input, doc) == DeserializationError::Ok);
  JsonObjectConst dep = doc[0];
  TEST_ASSERT_EQUAL_STRING("RE 80", dep["label"]);
  TEST_ASSERT_EQUAL_STRING("W\xC3\xBCrzburg Hbf", dep["destination"]);  // UTF-8
  TEST_ASSERT_TRUE(dep["plannedDepartureTime"].as<long long>() == 1791030060000LL);
  TEST_ASSERT_FALSE(dep["network"].is<const char*>());
  TEST_ASSERT_FALSE(dep["occupancy"].is<const char*>());
  TEST_ASSERT_FALSE(dep["stopPointGlobalId"].is<const char*>());
  TEST_ASSERT_TRUE(dep["messages"].isNull());
  TEST_ASSERT_EQUAL_STRING("INFO", dep["infos"][0]["type"]);
  TEST_ASSERT_EQUAL_STRING("Hinweis", dep["infos"][0]["message"]);
  TEST_ASSERT_TRUE(dep["infos"][0]["network"].isNull());
}

void test_read_reports_broken_json() {
  std::istringstream input(R"([{"label":"S2","destination":"Erd)");
  JsonDocument doc;
  TEST_ASSERT_FALSE(widgets::mvg::readDepartures(input, doc) == DeserializationError::Ok);
}

void test_read_fixture_and_convert_fields() {
  const std::vector<Departure> d = parse("parse_deps.json");
  TEST_ASSERT_EQUAL_STRING("S3", d[0].line.c_str());
  TEST_ASSERT_TRUE(d[0].planned == 1791030060);  // ms -> s
  TEST_ASSERT_TRUE(d[0].transport == TransportType::kSBahn);
  TEST_ASSERT_TRUE(d[0].direction == Direction::kH);
  TEST_ASSERT_EQUAL_INT(2, d[0].platform);
  TEST_ASSERT_TRUE(d[0].realtime);
  TEST_ASSERT_FALSE(d[0].bus);
  // "RB 55" -> "RB55", UTF-8 destination kept
  TEST_ASSERT_EQUAL_STRING("RB55", d[3].line.c_str());
  TEST_ASSERT_EQUAL_STRING("M\xC3\xBCnchen Hbf", d[3].destination.c_str());
  TEST_ASSERT_TRUE(d[3].transport == TransportType::kTrain);
  TEST_ASSERT_TRUE(d[3].direction == Direction::kR);
  // regional bus: bus frame, no platform
  TEST_ASSERT_EQUAL_STRING("361", d[6].line.c_str());
  TEST_ASSERT_TRUE(d[6].transport == TransportType::kRegionalBus);
  TEST_ASSERT_TRUE(d[6].bus);
  TEST_ASSERT_EQUAL_INT(widgets::mvg::kNoPlatform, d[6].platform);
}

// --- Rule 1: messages ----------------------------------------------------------

void test_messages_incident_warns_info_is_ignored() {
  const std::vector<Departure> d = parse("rules_messages.json");
  TEST_ASSERT_EQUAL_size_t(5, d.size());
  TEST_ASSERT_FALSE(d[0].warning);  // INFO only
  TEST_ASSERT_TRUE(d[1].warning);   // INFO + INCIDENT
}

void test_messages_missing_fields_and_product_fallback() {
  const std::vector<Departure> d = parse("rules_messages.json");
  TEST_ASSERT_FALSE(d[2].realtime);                          // no "realtime" field
  TEST_ASSERT_FALSE(d[2].warning);                           // no "infos" field
  TEST_ASSERT_TRUE(d[3].transport == TransportType::kTram);  // "product" fallback
  TEST_ASSERT_TRUE(d[4].transport == TransportType::kUnknown);
  TEST_ASSERT_FALSE(d[4].bus);
}

// --- Rules 2 and 3: duplicates and early termination ----------------------------

void test_early_termination_stopping_here_shows_new_destination() {
  const std::vector<Departure> d = parse("rules_early_termination.json");
  TEST_ASSERT_EQUAL_STRING("S2>Ostbahnhof!", describe({d[0]}).c_str());
}

void test_early_termination_cancelled_keeps_original_destination() {
  const std::vector<Departure> d = parse("rules_early_termination.json");
  TEST_ASSERT_EQUAL_STRING("S2>Petershausen!x", describe({d[1]}).c_str());
}

void test_duplicate_keeps_only_the_real_trip_with_warning() {
  const std::vector<Departure> d = parse("rules_early_termination.json");
  // The cancelled S3 Holzkirchen is replaced by the real trip to Deisenhofen;
  // the S3 of the opposite direction in the same minute is not touched.
  assertRows(
      "S2>Ostbahnhof! | S2>Petershausen!x | S3>Mammendorf | S3>Deisenhofen! | "
      "S7>Wolfratshausen!",
      d);
}

void test_early_termination_without_stop_name_keeps_destination() {
  const std::vector<Departure> d = parse("rules_early_termination.json");
  TEST_ASSERT_EQUAL_STRING("S7>Wolfratshausen!", describe({d.back()}).c_str());
}

// --- Rule 4: split trains and duplicates ----------------------------------------

void test_split_train_fixed_short_form_in_both_orders() {
  const std::vector<Departure> d = parse("rules_split_trains.json");
  TEST_ASSERT_EQUAL_STRING("S1>Flugh./Freising", describe({d[0]}).c_str());
  TEST_ASSERT_FALSE(d[0].multi_destination);
  TEST_ASSERT_EQUAL_STRING("S1>Flugh./Freising", describe({d[1]}).c_str());
}

void test_split_train_coupled_regional_trains_sorted_by_line() {
  const std::vector<Departure> d = parse("rules_split_trains.json");
  TEST_ASSERT_EQUAL_STRING("RB>Bayrischzell/Lenggries/Tegernsee", describe({d[2]}).c_str());
  TEST_ASSERT_TRUE(d[2].multi_destination);
  TEST_ASSERT_TRUE(d[2].transport == TransportType::kTrain);
  TEST_ASSERT_EQUAL_STRING("RE>W\xC3\xBCrzburg Hbf/Lichtenfels", describe({d[3]}).c_str());
}

void test_split_train_unknown_pairing_keeps_api_order() {
  const std::vector<Departure> d = parse("rules_split_trains.json");
  TEST_ASSERT_EQUAL_STRING("S6>Tutzing/Starnberg", describe({d[4]}).c_str());
  TEST_ASSERT_TRUE(d[4].multi_destination);
}

void test_split_train_duplicate_destination_gives_one_row_with_warning() {
  const std::vector<Departure> d = parse("rules_split_trains.json");
  TEST_ASSERT_EQUAL_STRING("S4>Geltendorf!", describe({d[5]}).c_str());
  TEST_ASSERT_FALSE(d[5].multi_destination);
}

void test_split_train_not_combined() {
  const std::vector<Departure> d = parse("rules_split_trains.json");
  // partial cancellation, different platforms, buses, rail without platform,
  // different lines outside a group
  assertRows(
      "S1>Flugh./Freising | S1>Flugh./Freising | RB>Bayrischzell/Lenggries/Tegernsee | "
      "RE>W\xC3\xBCrzburg Hbf/Lichtenfels | S6>Tutzing/Starnberg | S4>Geltendorf! | "
      "S8>Herrsching | S8>We\xC3\x9Fling"
      "x | S8>Flughafen M\xC3\xBCnchen | S8>Ostbahnhof | "
      "56>F\xC3\xBCrstenried West | 56>Forstenried | S20>Deisenhofen | "
      "S20>H\xC3\xB6llriegelskreuth | RB6>Garmisch-Partenkirchen | RE5>Lindau-Insel",
      d);
}

// --- Rule 5: delay of combined trips --------------------------------------------

void test_combined_delay_is_smallest_realtime_delay() {
  const std::vector<Departure> d = parse("rules_split_trains.json");
  TEST_ASSERT_EQUAL_INT(1, d[0].delay_minutes);  // S1: 3 and 1
  TEST_ASSERT_TRUE(d[0].realtime);
  TEST_ASSERT_EQUAL_INT(2, d[2].delay_minutes);  // RB: 2 and 4, 5 without real time
  TEST_ASSERT_TRUE(d[2].realtime);
}

void test_combined_delay_without_realtime_keeps_first_part() {
  const std::vector<Departure> d = parse("rules_split_trains.json");
  TEST_ASSERT_EQUAL_INT(3, d[3].delay_minutes);  // RE: first part 3, second 0
  TEST_ASSERT_FALSE(d[3].realtime);
}

// --- Rule 6: rail replacement services and line selection ------------------------
// Cases from the MVG departure display (fixture parse_deps.json).

void test_sev_labels_and_flags() {
  const std::vector<Departure> d = parse("parse_deps.json");
  // S-Bahn replacement: BUS with label "S3"
  TEST_ASSERT_EQUAL_STRING("SEV>Deisenhofen", describe({d[4]}).c_str());
  TEST_ASSERT_TRUE(d[4].replacement && d[4].bus);
  TEST_ASSERT_TRUE(d[4].transport == TransportType::kBus);
  // train replacement: BAHN with train number as label
  TEST_ASSERT_EQUAL_STRING("SEV>Rosenheim", describe({d[5]}).c_str());
  TEST_ASSERT_TRUE(d[5].replacement && d[5].bus);
  TEST_ASSERT_TRUE(d[5].transport == TransportType::kTrain);
}

void test_selection_none_shows_all_lines() {
  const std::vector<Departure> d = parse("parse_deps.json", "", DirectionFilter::kAll, 8);
  assertRows(
      "S3>Holzkirchen | S3>Mammendorf | RB>Bayrischzell/Lenggries/Tegernsee | "
      "RB55>M\xC3\xBCnchen Hbf | SEV>Deisenhofen | SEV>Rosenheim | 361>Schliersee, Bahnhof | "
      "S3>Holzkirchen",
      d);
  TEST_ASSERT_EQUAL_INT(0, d[2].delay_minutes);
}

void test_selection_line_and_direction() {
  assertRows("S3>Holzkirchen | S3>Holzkirchen", parse("parse_deps.json", "S3:H:S"));
}

void test_selection_both_directions_includes_sbahn_replacement() {
  assertRows("S3>Holzkirchen | S3>Mammendorf | SEV>Deisenhofen | S3>Holzkirchen",
             parse("parse_deps.json", "S3:B:S"));
}

void test_selection_combined_with_direction_filter() {
  TEST_ASSERT_EQUAL_size_t(0, parse("parse_deps.json", "S3:H:S", DirectionFilter::kR).size());
}

void test_selection_coupled_train_part_shows_whole_row_and_train_replacement() {
  // RB56 selected: the whole RB55/56/57 row plus the train replacement
  // service, but not RB55 on its own.
  assertRows("RB>Bayrischzell/Lenggries/Tegernsee | SEV>Rosenheim",
             parse("parse_deps.json", "RB56:H:Z"));
}

void test_selection_rb55_both_directions() {
  assertRows(
      "RB>Bayrischzell/Lenggries/Tegernsee | RB55>M\xC3\xBC"
      "nchen Hbf | SEV>Rosenheim | "
      "RB55>Schliersee",
      parse("parse_deps.json", "RB55:B:Z"));
}

void test_selection_regional_bus_without_train_replacement() {
  const std::vector<Departure> d = parse("parse_deps.json", "361:H:R");
  assertRows("361>Schliersee, Bahnhof", d);
  TEST_ASSERT_TRUE(d[0].bus);
}

// --- Rule 7 (new): sorting -----------------------------------------------------

void test_sorted_by_planned_plus_delay_stable() {
  const std::vector<Departure> d = parse("rules_sorting.json");
  // S3 +0 (5 late) = +5, S4 +2, S6 +4 (1 early) = +3, S8 +5 (tie with S3)
  assertRows("S4>Trudering | S6>Zorneding | S3>Holzkirchen | S8>Flughafen M\xC3\xBCnchen", d);
  TEST_ASSERT_TRUE(d[2].planned == kRulesStart);
  TEST_ASSERT_TRUE(d[2].expected() == kRulesStart + 5 * 60);
}

void test_limit_applies_after_sorting() {
  assertRows("S4>Trudering | S6>Zorneding",
             parse("rules_sorting.json", "", DirectionFilter::kAll, 2));
}

// --- Real data (reduced captures 2026-10-03) ------------------------------------

void test_real_data_holzkirchen() {
  const std::vector<Departure> d = parse("hk_deps.json");
  assertRows(
      "RB55>Bayrischzell | RB55>Schliersee | SEV>Deisenhofen | SEV>Rosenheim | "
      "S3>Mammendorf",
      d);
  assertRows("SEV>Deisenhofen | S3>Mammendorf", parse("hk_deps.json", "S3:R:S"));
}

void test_real_data_pasing() {
  TEST_ASSERT_EQUAL_size_t(66, parse("pasing_deps.json").size());
  TEST_ASSERT_EQUAL_size_t(39, parse("pasing_deps.json", "", DirectionFilter::kH).size());
  TEST_ASSERT_EQUAL_size_t(27, parse("pasing_deps.json", "", DirectionFilter::kR).size());
  for (const Departure& d : parse("pasing_deps.json", "RE80:B:Z,RE89:B:Z")) {
    TEST_ASSERT_TRUE(d.line == "RE80" || d.line == "RE89");
    TEST_ASSERT_TRUE(d.transport == TransportType::kTrain);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_read_keeps_only_filtered_fields);
  RUN_TEST(test_read_reports_broken_json);
  RUN_TEST(test_read_fixture_and_convert_fields);
  RUN_TEST(test_messages_incident_warns_info_is_ignored);
  RUN_TEST(test_messages_missing_fields_and_product_fallback);
  RUN_TEST(test_early_termination_stopping_here_shows_new_destination);
  RUN_TEST(test_early_termination_cancelled_keeps_original_destination);
  RUN_TEST(test_duplicate_keeps_only_the_real_trip_with_warning);
  RUN_TEST(test_early_termination_without_stop_name_keeps_destination);
  RUN_TEST(test_split_train_fixed_short_form_in_both_orders);
  RUN_TEST(test_split_train_coupled_regional_trains_sorted_by_line);
  RUN_TEST(test_split_train_unknown_pairing_keeps_api_order);
  RUN_TEST(test_split_train_duplicate_destination_gives_one_row_with_warning);
  RUN_TEST(test_split_train_not_combined);
  RUN_TEST(test_combined_delay_is_smallest_realtime_delay);
  RUN_TEST(test_combined_delay_without_realtime_keeps_first_part);
  RUN_TEST(test_sev_labels_and_flags);
  RUN_TEST(test_selection_none_shows_all_lines);
  RUN_TEST(test_selection_line_and_direction);
  RUN_TEST(test_selection_both_directions_includes_sbahn_replacement);
  RUN_TEST(test_selection_combined_with_direction_filter);
  RUN_TEST(test_selection_coupled_train_part_shows_whole_row_and_train_replacement);
  RUN_TEST(test_selection_rb55_both_directions);
  RUN_TEST(test_selection_regional_bus_without_train_replacement);
  RUN_TEST(test_sorted_by_planned_plus_delay_stable);
  RUN_TEST(test_limit_applies_after_sorting);
  RUN_TEST(test_real_data_holzkirchen);
  RUN_TEST(test_real_data_pasing);
  return UNITY_END();
}
