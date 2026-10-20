// Unit tests for the MVG line helpers and the line selection
// (widgets/mvg/line_selection.h).

#include <unity.h>

#include <string>

#include "widgets/mvg/line_icons.h"
#include "widgets/mvg/line_selection.h"

using widgets::mvg::Direction;
using widgets::mvg::LineSelection;
using widgets::mvg::TransportType;

void setUp() {}
void tearDown() {}

void test_line_key_removes_spaces_and_upper_cases() {
  TEST_ASSERT_EQUAL_STRING("RE80", widgets::mvg::lineKey("RE 80").c_str());
  TEST_ASSERT_EQUAL_STRING("LUFTHANSAEXPRESSBUS",
                           widgets::mvg::lineKey("Lufthansa Express Bus").c_str());
  TEST_ASSERT_EQUAL_STRING("", widgets::mvg::lineKey(nullptr).c_str());
}

void test_line_direction_from_line_id() {
  TEST_ASSERT_EQUAL_CHAR('H', static_cast<char>(widgets::mvg::lineDirection("ddb:90M72:A:H:j26")));
  TEST_ASSERT_EQUAL_CHAR('R', static_cast<char>(widgets::mvg::lineDirection("ddb:92M05:P:R:j26")));
  TEST_ASSERT_EQUAL_CHAR('?', static_cast<char>(widgets::mvg::lineDirection("xyz")));
  TEST_ASSERT_EQUAL_CHAR('?', static_cast<char>(widgets::mvg::lineDirection(nullptr)));
}

void test_transport_type_mapping_round_trip() {
  const char* names[] = {"SBAHN", "UBAHN", "TRAM", "BUS", "REGIONAL_BUS", "BAHN"};
  for (const char* name : names) {
    const TransportType type = widgets::mvg::transportTypeFromApi(name);
    TEST_ASSERT_TRUE_MESSAGE(type != TransportType::kUnknown, name);
    TEST_ASSERT_EQUAL_STRING(name, widgets::mvg::transportTypeApiName(type));
  }
  TEST_ASSERT_TRUE(widgets::mvg::transportTypeFromApi("SCHIFF") == TransportType::kUnknown);
  TEST_ASSERT_TRUE(widgets::mvg::transportTypeFromApi(nullptr) == TransportType::kUnknown);
  TEST_ASSERT_NULL(widgets::mvg::transportTypeApiName(TransportType::kUnknown));
}

void test_selection_parse_normalises_and_drops_invalid_and_duplicates() {
  LineSelection s;
  s.parse("S2:H:S,re 80:B:Z,S2:R:S,X:Q:S,Y:H:?,,U6:R");
  TEST_ASSERT_EQUAL_STRING("S2:H:S,RE80:B:Z", s.toString().c_str());
  TEST_ASSERT_EQUAL_INT(2, s.size());
}

void test_selection_matches_direction() {
  LineSelection s;
  s.parse("S2:H:S,RE80:B:Z");
  TEST_ASSERT_TRUE(s.matches("S2", Direction::kH));
  TEST_ASSERT_FALSE(s.matches("S2", Direction::kR));
  TEST_ASSERT_TRUE(s.matches("RE80", Direction::kR));
  TEST_ASSERT_TRUE(s.matches("RE80", Direction::kUnknown));  // only B matches unknown
  TEST_ASSERT_FALSE(s.matches("S2", Direction::kUnknown));
  TEST_ASSERT_FALSE(s.matches("S3", Direction::kH));
}

void test_selection_transport_types_in_fixed_order() {
  LineSelection s;
  s.parse("RE80:B:Z,S2:H:S,361:H:R");
  TEST_ASSERT_EQUAL_STRING("SBAHN,REGIONAL_BUS,BAHN", s.transportTypes().c_str());
  TEST_ASSERT_TRUE(s.hasType(TransportType::kTrain));
  TEST_ASSERT_FALSE(s.hasType(TransportType::kTram));
}

void test_selection_is_limited() {
  std::string many;
  for (int i = 0; i < 20; i++) many += "L" + std::to_string(i) + ":B:B,";
  LineSelection s;
  s.parse(many);
  TEST_ASSERT_EQUAL_INT(LineSelection::kMaxLines, s.size());
}

void test_selection_empty() {
  LineSelection s;
  s.parse("S2:H:S");
  s.parse("");
  TEST_ASSERT_TRUE(s.empty());
  TEST_ASSERT_EQUAL_STRING("", s.transportTypes().c_str());
  TEST_ASSERT_EQUAL_STRING("", s.toString().c_str());
}

void test_line_icons_from_the_departure_display() {
  const widgets::mvg::LineIcon* s1 = widgets::mvg::findLineIcon("S1");
  TEST_ASSERT_NOT_NULL(s1);
  TEST_ASSERT_EQUAL_UINT8(36, s1->width);
  TEST_ASSERT_EQUAL_UINT8(28, widgets::mvg::findLineIcon("U6")->width);
  TEST_ASSERT_EQUAL_UINT8(30, widgets::mvg::findLineIcon("19")->width);  // tram
  TEST_ASSERT_NOT_NULL(widgets::mvg::findLineIcon("N17"));               // night tram
  TEST_ASSERT_NOT_NULL(widgets::mvg::findLineIcon("S20"));
  TEST_ASSERT_NULL(widgets::mvg::findLineIcon("RB55"));  // generated frame
  TEST_ASSERT_NULL(widgets::mvg::findLineIcon("SEV"));
  TEST_ASSERT_NULL(widgets::mvg::findLineIcon("s1"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_line_key_removes_spaces_and_upper_cases);
  RUN_TEST(test_line_direction_from_line_id);
  RUN_TEST(test_transport_type_mapping_round_trip);
  RUN_TEST(test_selection_parse_normalises_and_drops_invalid_and_duplicates);
  RUN_TEST(test_selection_matches_direction);
  RUN_TEST(test_selection_transport_types_in_fixed_order);
  RUN_TEST(test_selection_is_limited);
  RUN_TEST(test_selection_empty);
  RUN_TEST(test_line_icons_from_the_departure_display);
  return UNITY_END();
}
