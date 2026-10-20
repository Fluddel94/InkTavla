#include "core/scheduler.h"

#include <algorithm>

namespace core {

Scheduler::TaskId Scheduler::add(const Schedule& schedule) {
  Task task;
  task.schedule = schedule;
  if (task.schedule.interval_s == 0) task.schedule.interval_s = 1;
  if (task.schedule.offset_s >= static_cast<int32_t>(task.schedule.interval_s)) {
    task.schedule.offset_s %= static_cast<int32_t>(task.schedule.interval_s);
  }
  tasks_.push_back(task);
  return size() - 1;
}

std::vector<Scheduler::TaskId> Scheduler::due(int64_t now_ms) const {
  std::vector<TaskId> result;
  for (TaskId id = 0; id < size(); id++) {
    const Task& t = tasks_[id];
    if (t.due_now || now_ms >= t.next_due_ms) result.push_back(id);
  }
  return result;
}

int64_t Scheduler::nextSlotMs(const Task& task, int64_t now_ms, int64_t wall_s) const {
  const int64_t interval = task.schedule.interval_s;
  if (task.schedule.offset_s < 0 || wall_s < kMinValidWallTime) {
    return now_ms + interval * 1000;
  }
  // Next wall-clock slot strictly after wall_s. wall_s is truncated to whole
  // seconds, so the computed delay errs on the late side, never early.
  const int64_t offset = task.schedule.offset_s;
  const int64_t slot = ((wall_s - offset) / interval + 1) * interval + offset;
  return now_ms + (slot - wall_s) * 1000;
}

void Scheduler::markSuccess(TaskId id, int64_t now_ms, int64_t wall_s) {
  if (!valid(id)) return;
  Task& t = tasks_[id];
  t.succeeded = true;
  t.last_success_ms = now_ms;
  t.failures = 0;
  t.due_now = false;
  t.next_due_ms = nextSlotMs(t, now_ms, wall_s);
}

void Scheduler::markFailure(TaskId id, int64_t now_ms, int64_t wall_s) {
  if (!valid(id)) return;
  Task& t = tasks_[id];
  t.failures++;
  t.due_now = false;
  const int64_t retry = now_ms + static_cast<int64_t>(t.schedule.retry_s) * 1000;
  t.next_due_ms = std::min(retry, nextSlotMs(t, now_ms, wall_s));
}

void Scheduler::requestNow(TaskId id) {
  if (valid(id)) tasks_[id].due_now = true;
}

int64_t Scheduler::msUntilNextDue(int64_t now_ms) const {
  if (tasks_.empty()) return -1;
  int64_t wait = INT64_MAX;
  for (const Task& t : tasks_) {
    if (t.due_now) return 0;
    wait = std::min(wait, t.next_due_ms - now_ms);
  }
  return std::max<int64_t>(wait, 0);
}

bool Scheduler::hasSucceeded(TaskId id) const { return valid(id) && tasks_[id].succeeded; }

int64_t Scheduler::msSinceSuccess(TaskId id, int64_t now_ms) const {
  if (!hasSucceeded(id)) return -1;
  return now_ms - tasks_[id].last_success_ms;
}

uint32_t Scheduler::failures(TaskId id) const { return valid(id) ? tasks_[id].failures : 0; }

}  // namespace core
