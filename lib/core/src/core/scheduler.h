#pragma once

#include <cstdint>
#include <vector>

#include "core/time.h"

namespace core {

// When a task (e.g. a widget's data fetch) needs to run.
struct Schedule {
  // Regular interval in seconds (> 0).
  uint32_t interval_s = 60;

  // Alignment to the wall clock: if >= 0, the task is due at the wall-clock
  // times t (UTC seconds) with (t - offset_s) % interval_s == 0, e.g.
  // interval 60 and offset 1 = second 1 of every minute. -1 = no alignment,
  // the interval counts from the last attempt. Without a valid wall clock
  // (before NTP) aligned tasks fall back to plain intervals.
  int32_t offset_s = -1;

  // Delay before another attempt after a failure, in seconds. A retry never
  // comes later than the next regular slot.
  uint32_t retry_s = 10;
};

// Due-based scheduler.
//
// The scheduler only decides *when* tasks are due; it never runs anything
// itself and has no clock of its own. The caller passes the monotonic time
// (milliseconds since boot) and the wall-clock time (Unix seconds, 0 if not
// known yet), runs the due tasks and reports the result. This keeps the
// logic testable on the PC and allows deep sleep later: the time until the
// next due task is the sleep duration.
class Scheduler {
 public:
  using TaskId = int;

  // Adds a task; it is due immediately. Returns its ID (0, 1, ...).
  TaskId add(const Schedule& schedule);

  int size() const { return static_cast<int>(tasks_.size()); }

  // Tasks due at this time, in the order they were added. A due time that
  // has passed counts as due (>=), so a missed slot is caught up once.
  std::vector<TaskId> due(int64_t now_ms) const;

  // Reports the result of a run and schedules the next one.
  void markSuccess(TaskId id, int64_t now_ms, int64_t wall_s);
  void markFailure(TaskId id, int64_t now_ms, int64_t wall_s);

  // Makes a task due immediately (e.g. manual refresh with a button).
  void requestNow(TaskId id);

  // Milliseconds until the next task is due, 0 if one is due already,
  // -1 if there are no tasks.
  int64_t msUntilNextDue(int64_t now_ms) const;

  // Status for age hints ("as of 14:32").
  bool hasSucceeded(TaskId id) const;
  // Milliseconds since the last success, -1 if there was none.
  int64_t msSinceSuccess(TaskId id, int64_t now_ms) const;
  // Failures since the last success.
  uint32_t failures(TaskId id) const;

 private:
  struct Task {
    Schedule schedule;
    int64_t next_due_ms = 0;
    int64_t last_success_ms = 0;
    bool succeeded = false;
    bool due_now = true;  // added or requested: due regardless of time
    uint32_t failures = 0;
  };

  bool valid(TaskId id) const { return id >= 0 && id < size(); }
  // Monotonic time of the next regular slot after now.
  int64_t nextSlotMs(const Task& task, int64_t now_ms, int64_t wall_s) const;

  std::vector<Task> tasks_;
};

}  // namespace core
