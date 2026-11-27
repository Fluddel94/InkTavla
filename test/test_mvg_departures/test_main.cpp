// Unit tests for reading and processing MVG departures
// (widgets/mvg/departures.h), one group per processing rule.
//
// Fixtures: test/fixtures/mvg/ (see the README there). parse_deps.json,
// pasing_deps.json and hk_deps.json come from the MVG departure display
// project; rules_*.json are hand-written cases for single rules; *_20261019.json
// are real responses captured on 2026-10-19.

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "widgets/mvg/departures.h"
#include "widgets/mvg/sections.h"

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

// --- Rule 4b: one real trip listed twice under different planned times -------------
// rules_replacement_duplicate.json (made-up station, case seen on 2026-10-26).

int countDestination(const std::vector<Departure>& d, const char* destination) {
  int count = 0;
  for (const Departure& row : d) count += row.destination == destination ? 1 : 0;
  return count;
}

void test_replacement_duplicate_is_one_row_with_earlier_planned_time() {
  const std::vector<Departure> d = parse("rules_replacement_duplicate.json");
  // 05:27 +24 and 05:47 +4 are both expected 05:51: one row, the earlier
  // planned time with its larger delay; plus the cancelled 05:47 and 06:07.
  TEST_ASSERT_EQUAL_INT(3, countDestination(d, "Mammendorf"));
  int merged = 0;
  for (const Departure& row : d) {
    if (row.destination != "Mammendorf" || row.cancelled) continue;
    if (row.planned == kRulesStart + 27 * 60) {
      merged++;
      TEST_ASSERT_EQUAL_INT(24, row.delay_minutes);
      TEST_ASSERT_TRUE(row.realtime);
    } else {
      TEST_ASSERT_EQUAL_INT64(kRulesStart + 67 * 60, row.planned);  // another train
    }
  }
  TEST_ASSERT_EQUAL_INT(1, merged);
}

void test_replacement_duplicate_keeps_the_cancelled_trip() {
  const std::vector<Departure> d = parse("rules_replacement_duplicate.json");
  int cancelled = 0;
  for (const Departure& row : d) {
    if (row.destination != "Mammendorf" || !row.cancelled) continue;
    cancelled++;
    TEST_ASSERT_EQUAL_INT64(kRulesStart + 47 * 60, row.planned);
    TEST_ASSERT_TRUE(row.warning);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(row.incidents.size()));
  }
  TEST_ASSERT_EQUAL_INT(1, cancelled);
}

void test_replacement_duplicate_other_trips_are_not_merged() {
  const std::vector<Departure> d = parse("rules_replacement_duplicate.json");
  // Same expected minute but different platform, destination, cancellation,
  // real-time state, direction or no platform (bus): every trip stays.
  TEST_ASSERT_EQUAL_INT(2, countDestination(d, "Wolfratshausen"));
  TEST_ASSERT_EQUAL_INT(1, countDestination(d,
                                            "Kreuzstra\xC3\x9F"
                                            "e"));
  TEST_ASSERT_EQUAL_INT(1, countDestination(d, "Geretsried"));
  TEST_ASSERT_EQUAL_INT(2, countDestination(d, "Herrsching"));
  TEST_ASSERT_EQUAL_INT(2, countDestination(d, "Holzkirchen"));
  TEST_ASSERT_EQUAL_INT(2, countDestination(d, "Flughafen M\xC3\xBCnchen"));
  TEST_ASSERT_EQUAL_INT(2, countDestination(d, "Giesing"));
  TEST_ASSERT_EQUAL_INT(15, static_cast<int>(d.size()));  // 16 records, one merged
}

void test_replacement_duplicate_result_stays_sorted() {
  const std::vector<Departure> d = parse("rules_replacement_duplicate.json");
  for (size_t i = 1; i < d.size(); i++) TEST_ASSERT_TRUE(d[i - 1].expected() <= d[i].expected());
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

// --- Real data (reduced captures 2026-10-03, without times and platforms)
// ------------------------------------

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

// --- Real data (complete captures 2026-10-19, around 22:30 local time) --------
// During the capture, RB 57 towards München Hbf only ran to Schaftlach.

void test_real_early_termination_still_stopping_here() {
  // Gmund is before Schaftlach: the train still stops here, the API marks it
  // as not cancelled, so the new destination is shown.
  const std::vector<Departure> d = parse("gmund_20261019.json");
  TEST_ASSERT_EQUAL_STRING("RB57>Schaftlach!", describe({d[0]}).c_str());
}

void test_real_early_termination_cancelled_here() {
  // Holzkirchen is after Schaftlach: cancelled here, original destination.
  const std::vector<Departure> d = parse("holzkirchen_20261019.json");
  TEST_ASSERT_EQUAL_STRING("RB57>M\xC3\xBCnchen Hbf!x", describe({d[0]}).c_str());
}

void test_real_coupled_train_with_one_part_cancelled() {
  // RB 55/56/57 coupled, only the RB 57 part is cancelled: it stays a row of
  // its own; RB 55/56 are combined (same destination -> one destination) and
  // get the real-time delay of RB 55, as RB 56 has no real-time data.
  const std::vector<Departure> d = parse("holzkirchen_20261019.json");
  TEST_ASSERT_EQUAL_STRING("RB>M\xC3\xBCnchen Hbf", describe({d[2]}).c_str());
  TEST_ASSERT_TRUE(d[2].planned == d[0].planned);
  TEST_ASSERT_EQUAL_INT(6, d[2].delay_minutes);
  TEST_ASSERT_TRUE(d[2].realtime);
  TEST_ASSERT_FALSE(d[2].multi_destination);
  TEST_ASSERT_EQUAL_INT(4, d[2].platform);
}

void test_real_s1_split_train() {
  const std::vector<Departure> d =
      parse("feldmoching_20261019.json", "S1:B:S", DirectionFilter::kAll, 8);
  assertRows(
      "S1>Leuchtenbergring | S1>Flugh./Freising! | S1>Leuchtenbergring | S1>Flugh./Freising | "
      "S1>Leuchtenbergring | S1>Flugh./Freising | S1>Leuchtenbergring | S1>Leuchtenbergring",
      d);
  TEST_ASSERT_EQUAL_INT(3, d[1].delay_minutes);
}

void test_real_coupled_regional_trains() {
  const std::vector<Departure> d = parse("schaftlach_20261019.json", "", DirectionFilter::kAll, 14);
  TEST_ASSERT_EQUAL_STRING("RB>M\xC3\xBCnchen Hbf", describe({d[8]}).c_str());
  TEST_ASSERT_FALSE(d[8].multi_destination);
  TEST_ASSERT_EQUAL_STRING("RB>Lenggries/Tegernsee", describe({d[11]}).c_str());
  TEST_ASSERT_TRUE(d[11].multi_destination);
}

void test_real_train_replacement_buses() {
  // Replacement buses for regional trains now also come as BUS: with the
  // line as label ("RB56", Schaftlach) or with an empty label (Pasing).
  const std::vector<Departure> s = parse("schaftlach_20261019.json");
  TEST_ASSERT_EQUAL_STRING("SEV>Holzkirchen", describe({s[0]}).c_str());
  TEST_ASSERT_TRUE(s[0].replacement && s[0].bus);
  TEST_ASSERT_TRUE(s[0].transport == TransportType::kBus);
  // The label matches the line selection directly.
  const std::vector<Departure> rb56 = parse("schaftlach_20261019.json", "RB56:H:Z");
  TEST_ASSERT_EQUAL_STRING("SEV>Lenggries", describe({rb56[0]}).c_str());

  bool found = false;
  for (const Departure& d : parse("pasing_20261019.json")) {
    if (d.replacement) {
      TEST_ASSERT_EQUAL_STRING("SEV>Augsburg Hbf", describe({d}).c_str());
      found = true;
    }
  }
  TEST_ASSERT_TRUE(found);

  // Without a label the replaced line is unknown: shown as soon as any
  // regional train is selected, like replacement buses of type BAHN.
  found = false;
  for (const Departure& d : parse("pasing_20261019.json", "RE80:B:Z")) {
    if (d.replacement) found = d.destination == "Augsburg Hbf";
  }
  TEST_ASSERT_TRUE(found);
  for (const Departure& d : parse("pasing_20261019.json", "S3:B:S")) {
    TEST_ASSERT_FALSE(d.replacement && d.destination == "Augsburg Hbf");
  }
}

void test_real_full_response_gives_same_result_as_reduced() {
  // holzkirchen_full_20261019.json is the unmodified response with all fields.
  const std::vector<Departure> full = parse("holzkirchen_full_20261019.json");
  const std::vector<Departure> reduced = parse("holzkirchen_20261019.json");
  TEST_ASSERT_EQUAL_size_t(reduced.size(), full.size());
  TEST_ASSERT_EQUAL_STRING(describe(reduced).c_str(), describe(full).c_str());
  for (size_t i = 0; i < full.size(); i++) {
    TEST_ASSERT_TRUE(full[i].expected() == reduced[i].expected());
    TEST_ASSERT_EQUAL_INT(reduced[i].platform, full[i].platform);
  }
}

void test_real_results_are_sorted() {
  const char* files[] = {"gmund_20261019.json",
                         "holzkirchen_20261019.json",
                         "donnersbergerbruecke_20261019.json",
                         "feldmoching_20261019.json",
                         "schaftlach_20261019.json",
                         "pasing_20261019.json"};
  for (const char* file : files) {
    const std::vector<Departure> d = parse(file);
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(0, d.size(), file);
    for (size_t i = 1; i < d.size(); i++) {
      TEST_ASSERT_TRUE_MESSAGE(d[i - 1].expected() <= d[i].expected(), file);
    }
  }
}

// --- Sections (two freely named directions) ----------------------------------

void test_sections_split_one_response() {
  JsonDocument doc;
  load("pasing_20261019.json", doc);
  widgets::mvg::Config config;
  config.sections.resize(2);
  config.sections[0].name = "Stadt";
  config.sections[0].lines.parse("S4:H:S,19:H:T");
  config.sections[1].name = "Raus";
  config.sections[1].lines.parse("S4:R:S,14:H:T");
  const auto sections = widgets::mvg::departureSections(doc, config, 3);
  TEST_ASSERT_EQUAL_size_t(2, sections.size());
  TEST_ASSERT_EQUAL_STRING("Stadt", sections[0].name.c_str());
  assertRows("S4>Trudering | 19>Berg am Laim Bf. | S4>Trudering", sections[0].departures);
  TEST_ASSERT_EQUAL_STRING("Raus", sections[1].name.c_str());
  // Tram 14 has the same direction code "H" as tram 19 but leaves the city.
  assertRows("14>Gondrellplatz | S4>Geltendorf! | 14>Gondrellplatz", sections[1].departures);
}

void test_without_sections_one_list() {
  JsonDocument doc;
  load("pasing_20261019.json", doc);
  widgets::mvg::Config config;
  config.lines.parse("S4:B:S");
  config.direction = DirectionFilter::kR;
  const auto sections = widgets::mvg::departureSections(doc, config, 2);
  TEST_ASSERT_EQUAL_size_t(1, sections.size());
  TEST_ASSERT_TRUE(sections[0].name.empty());
  assertRows("S4>Geltendorf! | S4>Geltendorf", sections[0].departures);
}

void test_departed_trips_are_dropped_before_drawing() {
  JsonDocument doc;
  load("pasing_20261019.json", doc);
  widgets::mvg::Config config;
  config.lines.parse("S4:H:S");
  const auto sections = widgets::mvg::departureSections(doc, config, 3);
  assertRows("S4>Trudering | S4>Trudering | S4>Ostbahnhof", sections[0].departures);
  const int64_t first = sections[0].departures[0].expected();  // 22:27
  const int64_t second = sections[0].departures[1].expected();
  // Shown until one minute after the departure.
  TEST_ASSERT_EQUAL_size_t(
      3, widgets::mvg::withoutDeparted(sections, first + 60)[0].departures.size());
  TEST_ASSERT_EQUAL_size_t(
      2, widgets::mvg::withoutDeparted(sections, first + 61)[0].departures.size());
  TEST_ASSERT_EQUAL_size_t(
      1, widgets::mvg::withoutDeparted(sections, second + 61)[0].departures.size());
  // Without a valid clock nothing is dropped.
  TEST_ASSERT_EQUAL_size_t(3, widgets::mvg::withoutDeparted(sections, 0)[0].departures.size());
}

// --- Disruption messages -----------------------------------------------------

std::string describe(const std::vector<widgets::mvg::Incident>& incidents) {
  std::string out;
  for (const widgets::mvg::Incident& i : incidents) {
    if (!out.empty()) out += " | ";
    for (const std::string& l : i.lines) out += l + " ";
    out += i.message;
  }
  return out;
}

// All messages of the departures, every text once (as the view merges them).
std::vector<widgets::mvg::Incident> mergedIncidents(const std::vector<Departure>& departures) {
  std::vector<widgets::mvg::Incident> result;
  for (const Departure& d : departures) {
    for (const widgets::mvg::DepartureIncident& m : d.incidents) {
      widgets::mvg::mergeIncidents(result, {{m.message, {m.line}}});
    }
  }
  return result;
}

void test_incidents_from_real_data() {
  // All lines: every text once with its lines.
  const std::vector<Departure> all = parse("pasing_20261019.json");
  TEST_ASSERT_EQUAL_STRING(
      "S6 S4 S8 RB6 Verspätung eines vorausfahrenden Zuges | S6 Technische Störung am Zug | "
      "RE70 Bauarbeiten",
      describe(mergedIncidents(all)).c_str());
  // Only the selected lines: the other trips are not parsed at all.
  TEST_ASSERT_EQUAL_STRING(
      "S6 Verspätung eines vorausfahrenden Zuges | S6 Technische Störung am Zug",
      describe(mergedIncidents(parse("pasing_20261019.json", "S6:B:S"))).c_str());
  // EARLY_TERMINATION and INFO are no disruption messages.
  TEST_ASSERT_EQUAL_size_t(0, mergedIncidents(parse("gmund_20261019.json")).size());
}

// Every departure keeps the messages of its own trip, so that the view can
// show the messages of the departures it draws (and only those).
void test_incidents_belong_to_their_trip() {
  const std::vector<Departure> d = parse("rules_incidents.json");
  TEST_ASSERT_EQUAL_size_t(11, d.size());
  TEST_ASSERT_TRUE(d[0].cancelled);
  TEST_ASSERT_EQUAL_size_t(1, d[0].incidents.size());
  TEST_ASSERT_EQUAL_STRING("defektes Stellwerk", d[0].incidents[0].message.c_str());
  TEST_ASSERT_EQUAL_STRING("S2", d[0].incidents[0].line.c_str());
  TEST_ASSERT_EQUAL_STRING("Technische Störung am Zug", d[1].incidents[0].message.c_str());
  TEST_ASSERT_EQUAL_STRING("S3", d[1].incidents[0].line.c_str());
  TEST_ASSERT_EQUAL_size_t(0, d[4].incidents.size());  // S2 14 min: none
  // Only the selected lines are parsed.
  const std::vector<Departure> s2 = parse("rules_incidents.json", "S2:B:S");
  TEST_ASSERT_EQUAL_size_t(10, s2.size());
  TEST_ASSERT_EQUAL_size_t(3, mergedIncidents(s2).size());
}

// The text of a message is cleaned like in Incident, INFO messages are not
// kept.
void test_incident_text_is_cleaned() {
  const std::vector<Departure> d = parse("rules_messages.json");
  TEST_ASSERT_EQUAL_size_t(0, d[0].incidents.size());  // INFO only
  TEST_ASSERT_EQUAL_size_t(1, d[1].incidents.size());
  TEST_ASSERT_EQUAL_STRING("Verspätungen wegen einer Signalstörung",
                           d[1].incidents[0].message.c_str());  // trailing dot dropped
}

void test_incidents_merged_and_lines_sorted() {
  // Trailing dots are dropped, so both texts are the same.
  std::vector<widgets::mvg::Incident> a;
  widgets::mvg::mergeIncidents(a, {{"Bauarbeiten.", {"S8"}}});
  widgets::mvg::mergeIncidents(a, {{"Bauarbeiten", {"S3", "S8"}}, {"Signalstörung", {"19"}}});
  TEST_ASSERT_EQUAL_STRING("S8 S3 Bauarbeiten | 19 Signalstörung", describe(a).c_str());
  a.push_back({"x", {"U6", "RB6", "S20", "SEV", "54"}});
  const std::vector<std::string> sorted = widgets::mvg::incidentLines(a);
  std::string text;
  for (const std::string& l : sorted) text += l + " ";
  TEST_ASSERT_EQUAL_STRING("RB6 S3 S8 S20 SEV U6 19 54 ", text.c_str());
  // A number with too many digits does not overflow (UBSan in native_asan).
  std::vector<widgets::mvg::Incident> odd;
  odd.push_back({"y", {"S12345678901234567890123", "S2"}});
  const std::vector<std::string> odd_sorted = widgets::mvg::incidentLines(odd);
  TEST_ASSERT_EQUAL_STRING("S2", odd_sorted[0].c_str());
  // Long texts are cut at a character boundary, at most kMaxIncidents texts.
  std::vector<widgets::mvg::Incident> many;
  std::vector<widgets::mvg::Incident> more;
  for (int i = 0; i < 10; i++) more.push_back({std::to_string(i) + std::string(200, 'x'), {"S1"}});
  widgets::mvg::mergeIncidents(many, more);
  TEST_ASSERT_EQUAL_size_t(widgets::mvg::kMaxIncidents, many.size());
  TEST_ASSERT_EQUAL_size_t(widgets::mvg::kMaxIncidentLength, many[0].message.size());
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
  RUN_TEST(test_replacement_duplicate_is_one_row_with_earlier_planned_time);
  RUN_TEST(test_replacement_duplicate_keeps_the_cancelled_trip);
  RUN_TEST(test_replacement_duplicate_other_trips_are_not_merged);
  RUN_TEST(test_replacement_duplicate_result_stays_sorted);
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
  RUN_TEST(test_real_early_termination_still_stopping_here);
  RUN_TEST(test_real_early_termination_cancelled_here);
  RUN_TEST(test_real_coupled_train_with_one_part_cancelled);
  RUN_TEST(test_real_s1_split_train);
  RUN_TEST(test_real_coupled_regional_trains);
  RUN_TEST(test_real_train_replacement_buses);
  RUN_TEST(test_real_full_response_gives_same_result_as_reduced);
  RUN_TEST(test_real_results_are_sorted);
  RUN_TEST(test_sections_split_one_response);
  RUN_TEST(test_without_sections_one_list);
  RUN_TEST(test_departed_trips_are_dropped_before_drawing);
  RUN_TEST(test_incidents_from_real_data);
  RUN_TEST(test_incidents_merged_and_lines_sorted);
  RUN_TEST(test_incidents_belong_to_their_trip);
  RUN_TEST(test_incident_text_is_cleaned);
  return UNITY_END();
}
