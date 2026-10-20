// Unit tests for the time helpers (core/time.h).

#include <unity.h>

#include "core/time.h"

using core::LocalTime;

namespace {

constexpr int64_t kNoon = 1792411200;  // 2026-10-19 12:00:00 UTC = 14:00 CEST

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

void test_valid_unix_time() {
  TEST_ASSERT_FALSE(core::isValidUnixTime(0));
  TEST_ASSERT_FALSE(core::isValidUnixTime(1700000000));  // 2023
  TEST_ASSERT_TRUE(core::isValidUnixTime(kNoon));
}

void test_monotonic_clock_does_not_go_back() {
  const int64_t a = core::monotonicMs();
  const int64_t b = core::monotonicMs();
  TEST_ASSERT_TRUE(b >= a);
}

void test_time_zone_syntax() {
  const char* valid[] = {"CET-1CEST,M3.5.0,M10.5.0/3",
                         "UTC0",
                         "GMT0BST,M3.5.0/1,M10.5.0",
                         "EST5EDT,M3.2.0,M11.1.0",
                         "<+0530>-5:30",
                         "AEST-10AEDT,M10.1.0,M4.1.0/3",
                         "NZST-12NZDT,M9.5.0,M4.1.0/3",
                         "WET0WEST,J60/1,J300/2"};
  for (const char* tz : valid) TEST_ASSERT_TRUE_MESSAGE(core::isValidTimeZone(tz), tz);
  const char* invalid[] = {"",
                           "Europe/Berlin",
                           "CET",
                           "CE-1",
                           "CET-1CEST,",
                           "CET-1CEST,M3.5.0",
                           "CET-1CEST,M13.5.0,M10.5.0",
                           "CET-25",
                           "<AB>-1"};
  for (const char* tz : invalid) TEST_ASSERT_FALSE_MESSAGE(core::isValidTimeZone(tz), tz);
  TEST_ASSERT_FALSE(core::isValidTimeZone(nullptr));
}

void test_local_time_summer() {
  LocalTime t;
  TEST_ASSERT_TRUE(core::toLocalTime(kNoon, t));
  TEST_ASSERT_EQUAL_INT(2026, t.year);
  TEST_ASSERT_EQUAL_INT(10, t.month);
  TEST_ASSERT_EQUAL_INT(19, t.day);
  TEST_ASSERT_EQUAL_INT(14, t.hour);
  TEST_ASSERT_EQUAL_INT(0, t.minute);
  TEST_ASSERT_EQUAL_INT(1, t.weekday);  // Monday
  TEST_ASSERT_TRUE(t.dst);
}

void test_switch_to_winter_time_2026_10_25() {
  LocalTime t;
  core::toLocalTime(1792889999, t);  // 00:59:59 UTC
  TEST_ASSERT_EQUAL_INT(2, t.hour);
  TEST_ASSERT_EQUAL_INT(59, t.minute);
  TEST_ASSERT_TRUE(t.dst);
  core::toLocalTime(1792890000, t);  // 01:00:00 UTC: 03:00 CEST -> 02:00 CET
  TEST_ASSERT_EQUAL_INT(2, t.hour);
  TEST_ASSERT_EQUAL_INT(0, t.minute);
  TEST_ASSERT_FALSE(t.dst);
}

void test_switch_to_summer_time_2026_03_29() {
  LocalTime t;
  core::toLocalTime(1774745999, t);  // 00:59:59 UTC = 01:59:59 CET
  TEST_ASSERT_EQUAL_INT(1, t.hour);
  TEST_ASSERT_FALSE(t.dst);
  core::toLocalTime(1774746000, t);  // 01:00:00 UTC = 03:00 CEST
  TEST_ASSERT_EQUAL_INT(3, t.hour);
  TEST_ASSERT_TRUE(t.dst);
}

void test_other_time_zone_and_invalid_fallback() {
  TEST_ASSERT_TRUE(core::setTimeZone("UTC0"));
  LocalTime t;
  core::toLocalTime(kNoon, t);
  TEST_ASSERT_EQUAL_INT(12, t.hour);
  TEST_ASSERT_FALSE(core::setTimeZone("Europe/Berlin"));  // falls back to the default
  core::toLocalTime(kNoon, t);
  TEST_ASSERT_EQUAL_INT(14, t.hour);
}

void test_minutes_until_counts_minute_values() {
  TEST_ASSERT_TRUE(core::minutesUntil(1792411679, 1792411740) == 2);  // 12:07:59 -> 12:09:00
  TEST_ASSERT_TRUE(core::minutesUntil(kNoon, kNoon + 59) == 0);
  TEST_ASSERT_TRUE(core::minutesUntil(kNoon + 59, kNoon) == 0);
  TEST_ASSERT_TRUE(core::minutesUntil(kNoon, kNoon - 1) == -1);
  TEST_ASSERT_TRUE(core::minutesUntil(kNoon, kNoon + 3600) == 60);
}

void test_minutes_until_across_time_change() {
  // 02:50 CEST -> 02:10 CET is 20 minutes, not -40.
  TEST_ASSERT_TRUE(core::minutesUntil(1792889999 - 599, 1792890000 + 600) == 20);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid_unix_time);
  RUN_TEST(test_monotonic_clock_does_not_go_back);
  RUN_TEST(test_time_zone_syntax);
  RUN_TEST(test_local_time_summer);
  RUN_TEST(test_switch_to_winter_time_2026_10_25);
  RUN_TEST(test_switch_to_summer_time_2026_03_29);
  RUN_TEST(test_other_time_zone_and_invalid_fallback);
  RUN_TEST(test_minutes_until_counts_minute_values);
  RUN_TEST(test_minutes_until_across_time_change);
  return UNITY_END();
}
