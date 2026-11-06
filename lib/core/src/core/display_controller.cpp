#include "core/display_controller.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "core/syslog.h"
#include "core/time.h"

namespace core {

namespace {

// "0.9 s", "12.3 s"
std::string seconds(uint64_t ms) {
  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "%u.%u s", static_cast<unsigned>(ms / 1000),
                static_cast<unsigned>(ms % 1000 / 100));
  return buffer;
}

uint32_t clampMs(int64_t ms) {
  return static_cast<uint32_t>(std::min<int64_t>(std::max<int64_t>(ms, 0), UINT32_MAX));
}

}  // namespace

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
  e.report.name = content.logName();
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
  // The start screen tells whether it waits for Wi-Fi or for the data.
  if (starting_) start_redraw_ = true;
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

void DisplayController::showSystemPage(bool show) {
  if (show == system_page_) return;
  system_page_ = show;
  system_page_ms_ = clock_.monotonicMs();
  planner_.requestFull();  // the whole screen changes
  force_redraw_ = true;
}

std::vector<SourceReport> DisplayController::sourceReports() const {
  const int64_t now_ms = clock_.monotonicMs();
  std::vector<SourceReport> result;
  for (const Entry& e : entries_) {
    SourceReport r = e.report;
    r.state = e.freshness.state(now_ms);
    result.push_back(r);
  }
  return result;
}

void DisplayController::fetch(Entry& e, Scheduler::TaskId id, int64_t& wall_s) {
  const int64_t before_ms = clock_.monotonicMs();
  takeFetchProblem();  // left over from a request outside the loop
  const bool ok = e.content->fetch(wall_s);
  const int64_t after_ms = clock_.monotonicMs();
  wall_s = clock_.wallTime();
  const char* name = e.content->logName();
  SourceReport& r = e.report;
  r.last_duration_ms = clampMs(after_ms - before_ms);
  // Without a network a failure is no result yet (start screen).
  if (ok || network_connected_) e.has_result = true;
  if (ok) {
    takeFetchProblem();
    r.successes++;
    e.period_successes++;
    e.period_ms += r.last_duration_ms;
    e.period_max_ms = std::max(e.period_max_ms, r.last_duration_ms);
    if (r.failures_in_row > 0) {
      logInfo(name, "works again after %u failed attempt%s (%s)",
              static_cast<unsigned>(r.failures_in_row), r.failures_in_row == 1 ? "" : "s",
              formatUptime((after_ms - e.failing_since_ms) / 1000).c_str());
    }
    r.failures_in_row = 0;
    r.last_success_wall_s = isValidUnixTime(wall_s) ? wall_s : 0;
    e.long_failure_logged = false;
    e.logged_problem.clear();
    e.freshness.success(after_ms, wall_s);
    scheduler_.markSuccess(id, after_ms, wall_s);
    return;
  }
  std::string problem = takeFetchProblem();
  if (!network_connected_) {
    problem = "no network";
  } else if (problem.empty()) {
    problem = "failed";
  }
  r.failures++;
  e.period_failures++;
  if (r.failures_in_row == 0) e.failing_since_ms = before_ms;
  r.failures_in_row++;
  r.problem = problem;
  r.last_failure_wall_s = isValidUnixTime(wall_s) ? wall_s : 0;
  // A failing source is retried every few seconds: log the first failure
  // and a change of the reason, count the rest.
  if (r.failures_in_row == 1 || problem != e.logged_problem) {
    logWarning(name, "fetch failed: %s (%s)", problem.c_str(), seconds(r.last_duration_ms).c_str());
    e.logged_problem = problem;
  }
  if (!e.long_failure_logged && after_ms - e.failing_since_ms >= kLongFailureMs) {
    logError(name, "failing for %d min (%u attempts): %s",
             static_cast<int>((after_ms - e.failing_since_ms) / 60000),
             static_cast<unsigned>(r.failures_in_row), problem.c_str());
    e.long_failure_logged = true;
  }
  e.freshness.failure(after_ms);
  scheduler_.markFailure(id, after_ms, wall_s);
}

void DisplayController::logSummary() {
  for (Entry& e : entries_) {
    const uint32_t total = e.period_successes + e.period_failures;
    if (total == 0) continue;
    logInfo(e.content->logName(), "last hour: %u ok, %u failed, avg %s, max %s",
            static_cast<unsigned>(e.period_successes), static_cast<unsigned>(e.period_failures),
            seconds(e.period_successes == 0 ? 0 : e.period_ms / e.period_successes).c_str(),
            seconds(e.period_max_ms).c_str());
    e.period_successes = 0;
    e.period_failures = 0;
    e.period_ms = 0;
    e.period_max_ms = 0;
  }
  if (summary_hook_) summary_hook_();
}

int64_t DisplayController::step() {
  const int64_t now_ms = clock_.monotonicMs();
  int64_t wall_s = clock_.wallTime();
  // The start screen, if shown already, shows a new network state before
  // the fetches, which may take seconds.
  if (starting_ && start_redraw_ && !shown_.empty()) {
    start_redraw_ = false;
    redraw(now_ms);
  }
  start_redraw_ = false;
  bool redraw_needed = force_redraw_;

  // The first valid time (NTP) shows the date and realigns the schedule.
  if (isValidUnixTime(wall_s) != wall_valid_) {
    wall_valid_ = isValidUnixTime(wall_s);
    if (wall_valid_) logInfo("time", "clock set (NTP)");
    redraw_needed = true;
  }

  if (system_page_ && now_ms - system_page_ms_ >= kSystemPageMs) {
    showSystemPage(false);
    redraw_needed = true;
  }
  if (now_ms >= next_summary_ms_) {
    logSummary();
    while (next_summary_ms_ <= now_ms) next_summary_ms_ += kLogSummaryMs;
  }

  for (Scheduler::TaskId id : scheduler_.due(now_ms)) {
    if (id == redraw_task_) {
      scheduler_.markSuccess(id, now_ms, wall_s);
      redraw_needed = true;
      continue;
    }
    for (Entry& e : entries_) {
      if (e.task != id) continue;
      // A fetch may take seconds (network); fetch() uses the time after it.
      fetch(e, id, wall_s);
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
  const int64_t now_after = clock_.monotonicMs();
  int64_t until = next_summary_ms_ - now_after;
  if (system_page_) until = std::min(until, system_page_ms_ + kSystemPageMs - now_after);
  wait = wait < 0 ? std::max<int64_t>(until, 0) : std::min(wait, std::max<int64_t>(until, 0));
  publishSourceReports(sourceReports());
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
  state_.system_page = system_page_;
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
