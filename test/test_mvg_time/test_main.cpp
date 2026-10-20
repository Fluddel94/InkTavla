// Unit tests for the time column rules of the MVG departure list
// (widgets/mvg/time_display.h).

#include <unity.h>

#include "core/time.h"
#include "widgets/mvg/time_display.h"

using widgets::mvg::Departure;
using widgets::mvg::TimeDisplay;
using widgets::mvg::TimeFormat;

namespace {

constexpr int64_t kNow = 1792411679;      // 2026-10-19 12:07:59 UTC = 14:07:59 CEST
constexpr int64_t kPlanned = 1792411740;  // 12:09:00 UTC = 14:09 CEST

Departure departure(int delay, bool cancelled = false, bool warning = false) {
  Departure d;
  d.line = "S3";
  d.destination = "Holzkirchen";
  d.planned = kPlanned;
  d.delay_minutes = delay;
  d.cancelled = cancelled;
  d.warning = warning;
  return d;
}

TimeDisplay clock(const Departure& d) {
  return widgets::mvg::timeDisplay(d, TimeFormat::kClock, kNow);
}
TimeDisplay minutes(const Departure& d, int64_t now = kNow) {
  return widgets::mvg::timeDisplay(d, TimeFormat::kMinutes, now);
}

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

void test_early_departure_rule() {
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::effectiveDelay(-1));
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::effectiveDelay(-2));
  TEST_ASSERT_EQUAL_INT(-3, widgets::mvg::effectiveDelay(-3));
  TEST_ASSERT_EQUAL_INT(0, widgets::mvg::effectiveDelay(0));
  TEST_ASSERT_EQUAL_INT(5, widgets::mvg::effectiveDelay(5));
}

void test_clock_shows_planned_local_time() {
  const TimeDisplay t = clock(departure(0));
  TEST_ASSERT_TRUE(t.kind == TimeDisplay::Kind::kClock);
  TEST_ASSERT_EQUAL_INT(14, t.hour);
  TEST_ASSERT_EQUAL_INT(9, t.minute);
  TEST_ASSERT_FALSE(t.show_delay);  // never "+0"
}

void test_clock_delay_text() {
  TimeDisplay t = clock(departure(3));
  TEST_ASSERT_TRUE(t.show_delay);
  TEST_ASSERT_EQUAL_INT(3, t.delay);
  TEST_ASSERT_EQUAL_INT(9, t.minute);  // planned time, not planned + delay
  TEST_ASSERT_FALSE(clock(departure(-2)).show_delay);
  t = clock(departure(-4));
  TEST_ASSERT_TRUE(t.show_delay);
  TEST_ASSERT_EQUAL_INT(-4, t.delay);
}

void test_clock_delay_is_limited() {
  TEST_ASSERT_EQUAL_INT(99, clock(departure(140)).delay);
  TEST_ASSERT_EQUAL_INT(-99, clock(departure(-120)).delay);
}

void test_minutes_until_expected_departure() {
  TimeDisplay t = minutes(departure(0));
  TEST_ASSERT_TRUE(t.kind == TimeDisplay::Kind::kMinutes);
  TEST_ASSERT_EQUAL_INT(2, t.minutes);  // 14:07 -> 14:09
  TEST_ASSERT_FALSE(t.show_delay);      // no delay text in minutes mode
  t = minutes(departure(4));
  TEST_ASSERT_EQUAL_INT(6, t.minutes);
  TEST_ASSERT_FALSE(t.show_delay);
  TEST_ASSERT_EQUAL_INT(2, minutes(departure(-1)).minutes);  // early rule first
}

void test_minutes_now() {
  TEST_ASSERT_TRUE(minutes(departure(0), kPlanned + 30).kind == TimeDisplay::Kind::kNow);
  TEST_ASSERT_TRUE(minutes(departure(0), kPlanned + 600).kind == TimeDisplay::Kind::kNow);
  TEST_ASSERT_TRUE(minutes(departure(-4), kNow).kind == TimeDisplay::Kind::kNow);  // 14:05
}

void test_minutes_from_60_show_clock_time() {
  TimeDisplay t = minutes(departure(0), kPlanned - 59 * 60);
  TEST_ASSERT_TRUE(t.kind == TimeDisplay::Kind::kMinutes);
  TEST_ASSERT_EQUAL_INT(59, t.minutes);
  t = minutes(departure(0), kPlanned - 60 * 60);  // exactly 60 minutes
  TEST_ASSERT_TRUE(t.kind == TimeDisplay::Kind::kClock);
  t = minutes(departure(3), kPlanned - 60 * 60);
  TEST_ASSERT_TRUE(t.kind == TimeDisplay::Kind::kClock);
  TEST_ASSERT_EQUAL_INT(14, t.hour);
  TEST_ASSERT_TRUE(t.show_delay);  // clock time again, with delay
}

void test_minutes_without_valid_time_falls_back_to_clock() {
  const TimeDisplay t = minutes(departure(2), 1000);
  TEST_ASSERT_TRUE(t.kind == TimeDisplay::Kind::kClock);
  TEST_ASSERT_EQUAL_INT(14, t.hour);
  TEST_ASSERT_TRUE(t.show_delay);
}

void test_cancelled_is_struck_with_warning_in_time_column() {
  TimeDisplay t = clock(departure(5, true, true));
  TEST_ASSERT_TRUE(t.struck);
  TEST_ASSERT_TRUE(t.warning_in_time_column);
  TEST_ASSERT_FALSE(t.warning_after_destination);  // not twice
  TEST_ASSERT_FALSE(t.show_delay);
  t = minutes(departure(0, true));
  TEST_ASSERT_TRUE(t.kind == TimeDisplay::Kind::kMinutes);
  TEST_ASSERT_TRUE(t.struck);
  TEST_ASSERT_TRUE(t.warning_in_time_column);  // also without a message
}

void test_warning_after_destination() {
  TEST_ASSERT_TRUE(clock(departure(0, false, true)).warning_after_destination);
  TEST_ASSERT_FALSE(clock(departure(0)).warning_after_destination);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_early_departure_rule);
  RUN_TEST(test_clock_shows_planned_local_time);
  RUN_TEST(test_clock_delay_text);
  RUN_TEST(test_clock_delay_is_limited);
  RUN_TEST(test_minutes_until_expected_departure);
  RUN_TEST(test_minutes_now);
  RUN_TEST(test_minutes_from_60_show_clock_time);
  RUN_TEST(test_minutes_without_valid_time_falls_back_to_clock);
  RUN_TEST(test_cancelled_is_struck_with_warning_in_time_column);
  RUN_TEST(test_warning_after_destination);
  return UNITY_END();
}
