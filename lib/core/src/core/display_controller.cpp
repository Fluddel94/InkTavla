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
  Entry e{&content, Freshness{}, -1, true};
  if (!content.isSetUp()) {
    e.freshness.setNotSetUp(true);
  } else if (content.fetches()) {
    e.task = scheduler_.add(content.schedule());
    e.has_result = false;
  } else {
    e.freshness.success(0, 0);  // nothing to fetch: always up to date
  }
  entries_.push_back(e);
  force_redraw_ = true;
}

void DisplayController::enableStartScreen() {
  starting_ = true;
  force_redraw_ = true;
}

void DisplayController::setNetworkConnected(bool connected) {
  if (connected == network_connected_) return;
  network_connected_ = connected;
  network_changed_ms_ = clock_.monotonicMs();
  if (connected && first_connected_ms_ < 0) first_connected_ms_ = network_changed_ms_;
  // Attempts without a connection have failed; fetch again at once instead
  // of waiting for the next retry.
  if (connected) requestUpdate();
}

bool DisplayController::networkShown(int64_t now_ms) const {
  return network_connected_ || now_ms - network_changed_ms_ < kOfflineGraceMs;
}

void DisplayController::requestUpdate() {
  for (const Entry& e : entries_) {
    if (e.task >= 0) scheduler_.requestNow(e.task);
  }
}

void DisplayController::requestFullRefresh() {
  planner_.requestFull();
  force_redraw_ = true;
}

void DisplayController::setAccessNotice(const AccessNotice& notice) {
  if (notice == notice_) return;
  notice_ = notice;
  force_redraw_ = true;
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
      // Without a network a failure is no result yet (start screen).
      if (ok || network_connected_) e.has_result = true;
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
  if (checkStartEnd(end_ms)) redraw_needed = true;
  if (redraw_needed) redraw(end_ms);
  int64_t wait = scheduler_.msUntilNextDue(clock_.monotonicMs());
  const int64_t deadline = startDeadlineMs();
  if (starting_ && deadline >= 0) {
    const int64_t until = std::max<int64_t>(deadline - clock_.monotonicMs(), 0);
    wait = wait < 0 ? until : std::min(wait, until);
  }
  if (!network_connected_ && network_shown_) {
    // Wake up when the grace period ends.
    const int64_t until = network_changed_ms_ + kOfflineGraceMs - clock_.monotonicMs();
    wait = wait < 0 ? until : std::min(wait, std::max<int64_t>(until, 0));
  }
  return wait;
}

int64_t DisplayController::startDeadlineMs() const {
  if (first_connected_ms_ < 0) return -1;
  return std::max(kStartMaxMs, first_connected_ms_ + kStartAfterConnectMs);
}

bool DisplayController::checkStartEnd(int64_t now_ms) {
  if (!starting_) return false;
  bool done =
      notice_.mode == AccessMode::kWifiNotSetUp || notice_.mode == AccessMode::kWifiNotConnected;
  if (!done && network_connected_) {
    done = std::all_of(entries_.begin(), entries_.end(),
                       [](const Entry& e) { return e.has_result; }) ||
           now_ms >= startDeadlineMs();
  }
  if (!done) return false;
  starting_ = false;
  planner_.requestFull();  // the whole screen changes
  return true;
}

void DisplayController::buildState(int64_t now_ms, int64_t wall_s) {
  state_.wall_s = wall_s;
  state_.network_connected = network_shown_;
  state_.notice = notice_;
  state_.starting = starting_;
  state_.network_up = network_connected_;
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
