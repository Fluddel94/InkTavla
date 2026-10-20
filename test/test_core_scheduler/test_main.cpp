// Unit tests for the due-based scheduler (core/scheduler.h).

#include <unity.h>

#include <vector>

#include "core/scheduler.h"

using core::Schedule;
using core::Scheduler;

namespace {

constexpr int64_t kWall = 1792440000;  // 2026-10-19 20:00:00 UTC (second 0)

std::vector<int> ids(const std::vector<Scheduler::TaskId>& due) {
  return std::vector<int>(due.begin(), due.end());
}

Schedule everyMinuteAtSecond1() {
  Schedule s;
  s.interval_s = 60;
  s.offset_s = 1;
  return s;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_new_tasks_are_due_immediately_in_order() {
  Scheduler s;
  s.add(Schedule{});
  s.add(Schedule{});
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(s.due(0).size()));
  TEST_ASSERT_EQUAL_INT(0, s.due(0)[0]);
  TEST_ASSERT_EQUAL_INT(1, s.due(0)[1]);
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == 0);
}

void test_plain_interval_counts_from_last_run() {
  Scheduler s;
  Schedule weather;
  weather.interval_s = 1800;
  const auto id = s.add(weather);
  s.markSuccess(id, 5000, kWall);
  TEST_ASSERT_TRUE(s.due(5000 + 1799999).empty());
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(s.due(5000 + 1800000).size()));
  TEST_ASSERT_TRUE(s.msUntilNextDue(5000) == 1800000);
}

void test_aligned_task_is_due_at_second_1_of_the_next_minute() {
  Scheduler s;
  const auto id = s.add(everyMinuteAtSecond1());
  // Success at 20:00:05 -> next slot 20:01:01, i.e. 56 s later.
  s.markSuccess(id, 100000, kWall + 5);
  TEST_ASSERT_TRUE(s.msUntilNextDue(100000) == 56000);
  TEST_ASSERT_TRUE(s.due(100000 + 55999).empty());
  TEST_ASSERT_FALSE(s.due(100000 + 56000).empty());
}

void test_aligned_task_success_exactly_on_slot_goes_to_next_slot() {
  Scheduler s;
  const auto id = s.add(everyMinuteAtSecond1());
  s.markSuccess(id, 0, kWall + 1);  // 20:00:01
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == 60000);
}

void test_aligned_task_without_wall_clock_uses_plain_interval() {
  Scheduler s;
  const auto id = s.add(everyMinuteAtSecond1());
  s.markSuccess(id, 0, 0);  // no NTP time yet
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == 60000);
  s.markSuccess(id, 0, 1000);  // implausible time (1970)
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == 60000);
}

void test_missed_slot_is_caught_up_once() {
  Scheduler s;
  const auto id = s.add(everyMinuteAtSecond1());
  s.markSuccess(id, 0, kWall + 5);  // due at 56 s
  // The device was busy for 3 minutes: due once, not three times.
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(s.due(236000).size()));
  s.markSuccess(id, 236000, kWall + 241);  // 20:04:01 -> 20:05:01
  TEST_ASSERT_TRUE(s.msUntilNextDue(236000) == 60000);
}

void test_failure_retries_after_retry_delay() {
  Scheduler s;
  const auto id = s.add(everyMinuteAtSecond1());
  s.markFailure(id, 0, kWall + 1);
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == 10000);
  TEST_ASSERT_EQUAL_UINT32(1, s.failures(id));
  s.markFailure(id, 10000, kWall + 11);
  TEST_ASSERT_EQUAL_UINT32(2, s.failures(id));
  s.markSuccess(id, 20000, kWall + 21);
  TEST_ASSERT_EQUAL_UINT32(0, s.failures(id));
}

void test_retry_never_later_than_next_regular_slot() {
  Scheduler s;
  const auto id = s.add(everyMinuteAtSecond1());
  s.markFailure(id, 0, kWall + 55);  // next slot 20:01:01 is 6 s away
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == 6000);
}

void test_request_now_makes_task_due() {
  Scheduler s;
  Schedule calendar;
  calendar.interval_s = 900;
  const auto a = s.add(calendar);
  const auto b = s.add(calendar);
  s.markSuccess(a, 0, kWall);
  s.markSuccess(b, 0, kWall);
  TEST_ASSERT_TRUE(s.due(1000).empty());
  s.requestNow(b);
  const std::vector<int> due = ids(s.due(1000));
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(due.size()));
  TEST_ASSERT_EQUAL_INT(b, due[0]);
  TEST_ASSERT_TRUE(s.msUntilNextDue(1000) == 0);
}

void test_next_due_is_the_earliest_task() {
  Scheduler s;
  Schedule slow;
  slow.interval_s = 1800;
  const auto weather = s.add(slow);
  const auto mvg = s.add(everyMinuteAtSecond1());
  s.markSuccess(weather, 0, kWall);
  s.markSuccess(mvg, 0, kWall + 30);
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == 31000);
  TEST_ASSERT_TRUE(s.msUntilNextDue(40000) == 0);  // overdue
}

void test_age_of_last_success() {
  Scheduler s;
  const auto id = s.add(Schedule{});
  TEST_ASSERT_FALSE(s.hasSucceeded(id));
  TEST_ASSERT_TRUE(s.msSinceSuccess(id, 1000) == -1);
  s.markSuccess(id, 1000, kWall);
  s.markFailure(id, 61000, kWall + 60);
  TEST_ASSERT_TRUE(s.hasSucceeded(id));
  TEST_ASSERT_TRUE(s.msSinceSuccess(id, 71000) == 70000);
}

void test_empty_scheduler_and_invalid_ids() {
  Scheduler s;
  TEST_ASSERT_TRUE(s.msUntilNextDue(0) == -1);
  s.markSuccess(3, 0, kWall);  // ignored
  s.requestNow(-1);            // ignored
  TEST_ASSERT_EQUAL_UINT32(0, s.failures(7));
  TEST_ASSERT_FALSE(s.hasSucceeded(7));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_new_tasks_are_due_immediately_in_order);
  RUN_TEST(test_plain_interval_counts_from_last_run);
  RUN_TEST(test_aligned_task_is_due_at_second_1_of_the_next_minute);
  RUN_TEST(test_aligned_task_success_exactly_on_slot_goes_to_next_slot);
  RUN_TEST(test_aligned_task_without_wall_clock_uses_plain_interval);
  RUN_TEST(test_missed_slot_is_caught_up_once);
  RUN_TEST(test_failure_retries_after_retry_delay);
  RUN_TEST(test_retry_never_later_than_next_regular_slot);
  RUN_TEST(test_request_now_makes_task_due);
  RUN_TEST(test_next_due_is_the_earliest_task);
  RUN_TEST(test_age_of_last_success);
  RUN_TEST(test_empty_scheduler_and_invalid_ids);
  return UNITY_END();
}
