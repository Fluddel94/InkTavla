#include "core/display_controller.h"

#include <algorithm>
#include <ctime>

#include "core/time.h"

namespace core {

int64_t SystemClock::monotonicMs() const { return core::monotonicMs(); }

int64_t SystemClock::wallTime() const { return static_cast<int64_t>(std::time(nullptr)); }

DisplayController::DisplayController(Clock& clock, hal::Display& display, ScreenRenderer& renderer,
                                     int full_refresh_minutes)
    : clock_(clock), display_(display), renderer_(renderer), planner_(full_refresh_minutes) {
  Schedule redraw;
  redraw.interval_s = 60;
  redraw.offset_s = kRedrawSecond;
  redraw_task_ = scheduler_.add(redraw);
}

void DisplayController::addContent(Content& content) {
  Entry e{&content, Freshness{}, -1};
  if (!content.isSetUp()) {
    e.freshness.setNotSetUp(true);
  } else if (content.fetches()) {
    e.task = scheduler_.add(content.schedule());
  } else {
    e.freshness.success(0, 0);  // nothing to fetch: always up to date
  }
  entries_.push_back(e);
  force_redraw_ = true;
}

void DisplayController::setNetworkConnected(bool connected) {
  if (connected == network_connected_) return;
  network_connected_ = connected;
  network_changed_ms_ = clock_.monotonicMs();
}

bool DisplayController::networkShown(int64_t now_ms) const {
  return network_connected_ || now_ms - network_changed_ms_ < kOfflineGraceMs;
}

void DisplayController::requestUpdate() {
  for (const Entry& e : entries_) {
    if (e.task >= 0) scheduler_.requestNow(e.task);
  }
}

int64_t DisplayController::step() {
  const int64_t now_ms = clock_.monotonicMs();
  int64_t wall_s = clock_.wallTime();
  bool redraw_needed = force_redraw_;

  // The first valid time (NTP) shows the date and realigns the schedule.
  if (isValidUnixTime(wall_s) != wall_valid_) {
    wall_valid_ = isValidUnixTime(wall_s);
    redraw_needed = true;
  }

  for (Scheduler::TaskId id : scheduler_.due(now_ms)) {
    if (id == redraw_task_) {
      scheduler_.markSuccess(id, now_ms, wall_s);
      redraw_needed = true;
      continue;
    }
    for (Entry& e : entries_) {
      if (e.task != id) continue;
      // A fetch may take seconds (network); use the time after it.
      const bool ok = e.content->fetch(wall_s);
      const int64_t after_ms = clock_.monotonicMs();
      wall_s = clock_.wallTime();
      if (ok) {
        e.freshness.success(after_ms, wall_s);
        scheduler_.markSuccess(id, after_ms, wall_s);
      } else {
        e.freshness.failure(after_ms);
        scheduler_.markFailure(id, after_ms, wall_s);
      }
      redraw_needed = true;
    }
  }

  // A reconnection is shown at once, a lost connection after the grace
  // period.
  const int64_t end_ms = clock_.monotonicMs();
  if (networkShown(end_ms) != network_shown_) {
    network_shown_ = !network_shown_;
    redraw_needed = true;
  }
  if (redraw_needed) redraw(end_ms);
  int64_t wait = scheduler_.msUntilNextDue(clock_.monotonicMs());
  if (!network_connected_ && network_shown_) {
    // Wake up when the grace period ends.
    const int64_t until = network_changed_ms_ + kOfflineGraceMs - clock_.monotonicMs();
    wait = wait < 0 ? until : std::min(wait, std::max<int64_t>(until, 0));
  }
  return wait;
}

void DisplayController::buildState(int64_t now_ms, int64_t wall_s) {
  state_.wall_s = wall_s;
  state_.network_connected = network_shown_;
  state_.contents.clear();
  for (const Entry& e : entries_) {
    state_.contents.push_back({e.freshness.state(now_ms), e.freshness.lastSuccessWall()});
  }
}

void DisplayController::redraw(int64_t now_ms) {
  force_redraw_ = false;
  buildState(now_ms, clock_.wallTime());
  const FrameView frame = renderer_.render(state_);
  const RefreshPlan plan = planner_.plan(shown_view_, frame, renderer_.areas(), now_ms);
  if (!plan.needed()) return;
  display_.show(frame.data, frame.stride, plan.mode, plan.regions.data(), plan.regions.size());
  planner_.done(plan, now_ms);
  shown_.assign(frame.data, frame.data + frame.stride * frame.height);
  shown_view_ = {shown_.data(), frame.width, frame.height, frame.stride};
}

}  // namespace core
