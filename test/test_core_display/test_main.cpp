// Unit tests for the display controller and its parts (core/freshness.h,
// core/refresh.h, core/display_controller.h) with a simulated clock, a
// simulated panel and fake contents.

#include <unity.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/display_controller.h"
#include "core/fetch_cancel.h"
#include "core/freshness.h"
#include "core/refresh.h"
#include "core/syslog.h"
#include "hal/png_display.h"

using core::DataState;
using core::Freshness;

namespace {

constexpr int64_t kWall = 1792440000;  // 2026-10-19 20:00:00 UTC (second 0)

class FakeClock : public core::Clock {
 public:
  int64_t monotonicMs() const override { return ms; }
  int64_t wallTime() const override { return wall_valid ? kWall + ms / 1000 : 0; }
  void advance(int64_t delta_ms) { ms += delta_ms; }

  int64_t ms = 0;
  bool wall_valid = true;
};

class FakeContent : public core::Content {
 public:
  explicit FakeContent(bool fetches = true, bool set_up = true)
      : fetches_(fetches), set_up_(set_up) {}
  bool fetches() const override { return fetches_; }
  core::Schedule schedule() const override { return {60, 1, 10}; }
  bool isSetUp() const override { return set_up_; }
  bool fetch(int64_t wall_s) override {
    calls.push_back(wall_s);
    if (!succeed && !problem.empty()) core::noteFetchProblem("%s", problem.c_str());
    return succeed;
  }
  const char* logName() const override { return "test"; }

  bool succeed = true;
  std::string problem;
  std::vector<int64_t> calls;

 private:
  bool fetches_;
  bool set_up_;
};

// A source that takes `duration_ms` like a slow download (the calendar):
// it advances the clock in steps of 100 ms and asks core::fetchShouldStop()
// in between, like the body reader of core/http.cpp.
class SlowContent : public core::Content {
 public:
  SlowContent(FakeClock& clock, const std::vector<core::ScreenState>& frames)
      : clock_(clock), frames_(frames) {}
  bool fetches() const override { return true; }
  core::Schedule schedule() const override { return {60, 1, 10}; }
  bool fetch(int64_t) override {
    started_ms.push_back(clock_.ms);
    frames_before.push_back(frames_.size());
    for (int64_t t = 0; t < duration_ms; t += 100) {
      if (core::fetchShouldStop()) return false;
      clock_.advance(100);
    }
    return true;
  }
  const char* logName() const override { return "test"; }

  int64_t duration_ms = 5000;
  std::vector<int64_t> started_ms;
  std::vector<size_t> frames_before;  // frames drawn before each fetch

 private:
  FakeClock& clock_;
  const std::vector<core::ScreenState>& frames_;
};

// Renders a 16 x 8 frame: one 8 x 8 area per content, black if its state is
// kFresh, white otherwise; remembers the states it was given.
class FakeRenderer : public core::ScreenRenderer {
 public:
  core::FrameView render(const core::ScreenState& state) override {
    frame.assign(2 * 8, 0);
    for (size_t i = 0; i < state.contents.size() && i < 2; i++) {
      const bool black = state.contents[i].state == DataState::kFresh;
      for (int y = 0; y < 8; y++) frame[y * 2 + i] = black ? 0xFF : 0x00;
    }
    states.push_back(state);
    return {frame.data(), 16, 8, 2};
  }
  std::vector<hal::Rect> areas() const override { return {{0, 0, 8, 8}, {8, 0, 8, 8}}; }

  std::vector<uint8_t> frame;
  std::vector<core::ScreenState> states;
};

// Texts of the log entries of the area "test" after the running number seq.
std::vector<std::string> logged(uint32_t seq, core::LogLevel level = core::LogLevel::kInfo) {
  std::vector<std::string> result;
  core::LogEntry e;
  while (core::systemLog().next(seq, e)) {
    seq = e.seq;
    if (std::string(e.area) == "test" && e.level == level) result.push_back(e.text);
  }
  return result;
}

uint32_t lastSeq() {
  const std::vector<core::LogEntry> n = core::systemLog().newest(1);
  return n.empty() ? 0 : n[0].seq;
}

core::FrameView view(const std::vector<uint8_t>& data, uint16_t w, uint16_t h) {
  return {data.data(), w, h, static_cast<size_t>((w + 7) / 8)};
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- Freshness ----------------------------------------------------------------

void test_freshness_bridges_short_failures() {
  Freshness f;
  TEST_ASSERT_TRUE(f.state(0) == DataState::kWaiting);
  f.success(1000, kWall);
  TEST_ASSERT_TRUE(f.state(1000) == DataState::kFresh);
  f.failure(61000);
  f.failure(71000);  // counts from the first failure
  TEST_ASSERT_TRUE(f.state(120999) == DataState::kFresh);
  TEST_ASSERT_TRUE(f.state(121000) == DataState::kStale);
  TEST_ASSERT_EQUAL_INT64(kWall, f.lastSuccessWall());
  f.success(130000, kWall + 129);
  TEST_ASSERT_TRUE(f.state(130000) == DataState::kFresh);
  TEST_ASSERT_EQUAL_INT64(kWall + 129, f.lastSuccessWall());
}

void test_freshness_without_data() {
  Freshness f;
  f.failure(0);
  TEST_ASSERT_TRUE(f.state(59999) == DataState::kWaiting);
  TEST_ASSERT_TRUE(f.state(60000) == DataState::kNoData);
  f.success(70000, 0);  // clock not synchronised yet
  TEST_ASSERT_TRUE(f.state(70000) == DataState::kFresh);
  TEST_ASSERT_EQUAL_INT64(0, f.lastSuccessWall());
  f.setNotSetUp(true);
  TEST_ASSERT_TRUE(f.state(70000) == DataState::kNotSetUp);
}

// --- Refresh planning -----------------------------------------------------------

void test_region_differs_checks_only_the_region() {
  std::vector<uint8_t> a(3 * 4, 0), b(3 * 4, 0);  // 20 x 4
  b[1 * 3 + 1] = 0x01;                            // pixel x = 15, y = 1
  const auto va = view(a, 20, 4), vb = view(b, 20, 4);
  TEST_ASSERT_TRUE(core::regionDiffers(va, vb, {15, 1, 1, 1}));
  TEST_ASSERT_TRUE(core::regionDiffers(va, vb, {0, 0, 20, 4}));
  TEST_ASSERT_FALSE(core::regionDiffers(va, vb, {0, 0, 15, 4}));   // left of it
  TEST_ASSERT_FALSE(core::regionDiffers(va, vb, {16, 0, 4, 4}));   // right of it
  TEST_ASSERT_FALSE(core::regionDiffers(va, vb, {15, 2, 1, 2}));   // below
  TEST_ASSERT_FALSE(core::regionDiffers(va, vb, {30, 0, 10, 4}));  // outside
}

void test_planner_full_first_then_partial_then_full_after_interval() {
  core::RefreshPlanner planner(60);
  std::vector<uint8_t> a(2 * 8, 0), b(2 * 8, 0);
  b[0] = 0x80;  // area 0 changed
  const std::vector<hal::Rect> areas{{0, 0, 8, 8}, {8, 0, 8, 8}};

  core::RefreshPlan p = planner.plan({}, view(a, 16, 8), areas, 0);
  TEST_ASSERT_TRUE(p.mode == hal::Refresh::kFull);
  TEST_ASSERT_TRUE(p.needed());
  planner.done(p, 0);

  p = planner.plan(view(a, 16, 8), view(a, 16, 8), areas, 1000);
  TEST_ASSERT_FALSE(p.needed());  // nothing changed

  p = planner.plan(view(a, 16, 8), view(b, 16, 8), areas, 1000);
  TEST_ASSERT_TRUE(p.mode == hal::Refresh::kPartial);
  TEST_ASSERT_EQUAL_size_t(1, p.regions.size());
  TEST_ASSERT_EQUAL_INT(0, p.regions[0].x);
  planner.done(p, 1000);

  // After the interval the next change gives a full refresh.
  p = planner.plan(view(a, 16, 8), view(b, 16, 8), areas, 3600000);
  TEST_ASSERT_TRUE(p.mode == hal::Refresh::kFull);
  planner.done(p, 3600000);
  p = planner.plan(view(b, 16, 8), view(a, 16, 8), areas, 3601000);
  TEST_ASSERT_TRUE(p.mode == hal::Refresh::kPartial);
}

// --- Display controller ----------------------------------------------------------

void test_controller_first_step_fetches_and_shows_full() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  FakeContent header(false);
  c.addContent(a);
  c.addContent(header);

  c.step();
  TEST_ASSERT_EQUAL_size_t(1, a.calls.size());
  TEST_ASSERT_EQUAL_size_t(1, display.updates().size());
  TEST_ASSERT_TRUE(display.updates()[0].mode == hal::Refresh::kFull);
  TEST_ASSERT_TRUE(c.state().contents[0].state == DataState::kFresh);
  TEST_ASSERT_TRUE(c.state().contents[1].state == DataState::kFresh);  // nothing to fetch
  TEST_ASSERT_EQUAL_HEX8(0xFF, display.panel()[0]);
}

void test_controller_waits_until_second_1_and_skips_unchanged_frames() {
  FakeClock clock;
  clock.ms = 20000;  // 20:00:20
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.setNetworkConnected(true);  // else the offline grace period ends earlier

  c.step();
  // Next pass at 20:01:01 (fetch and redraw).
  const int64_t wait = c.step();
  TEST_ASSERT_EQUAL_INT64(41000, wait);
  clock.advance(wait);
  c.step();
  TEST_ASSERT_EQUAL_size_t(2, a.calls.size());
  TEST_ASSERT_EQUAL_INT64(kWall + 61, a.calls[1]);
  // Same frame: no further refresh of the panel.
  TEST_ASSERT_EQUAL_size_t(1, display.updates().size());
  TEST_ASSERT_EQUAL_size_t(2, renderer.states.size());  // drawn again, not shown
}

void test_controller_bridges_failures_then_shows_age() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.step();  // success at 20:00:00
  a.succeed = false;

  // Failures every 10 s; the data counts as fresh for 60 s.
  while (clock.ms < 200000) {
    const int64_t wait = c.step();
    if (c.state().contents[0].state != DataState::kFresh) break;
    clock.advance(std::max<int64_t>(wait, 1));
  }
  TEST_ASSERT_TRUE(c.state().contents[0].state == DataState::kStale);
  TEST_ASSERT_EQUAL_INT64(kWall, c.state().contents[0].last_success_wall_s);
  // First failure at 20:00:01 (second 1), retries every 10 s, stale from
  // the retry 60 s after the first failure.
  TEST_ASSERT_EQUAL_INT64(61000, clock.ms);
  TEST_ASSERT_EQUAL_size_t(8, a.calls.size());
  // The change to stale was shown as a partial refresh.
  TEST_ASSERT_TRUE(display.updates().back().mode == hal::Refresh::kPartial);

  a.succeed = true;
  clock.advance(c.step());
  c.step();
  TEST_ASSERT_TRUE(c.state().contents[0].state == DataState::kFresh);
}

void test_controller_not_set_up_is_never_fetched() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a(true, false);
  c.addContent(a);
  for (int i = 0; i < 5; i++) clock.advance(c.step());
  TEST_ASSERT_EQUAL_size_t(0, a.calls.size());
  TEST_ASSERT_TRUE(c.state().contents[0].state == DataState::kNotSetUp);
}

void test_controller_one_failing_content_does_not_stop_others() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent bad, good;
  bad.succeed = false;
  c.addContent(bad);
  c.addContent(good);
  for (int i = 0; i < 30; i++) clock.advance(std::max<int64_t>(c.step(), 1));
  TEST_ASSERT_TRUE(c.state().contents[0].state == DataState::kNoData);
  TEST_ASSERT_TRUE(c.state().contents[1].state == DataState::kFresh);
  TEST_ASSERT_TRUE(good.calls.size() >= 2);
  TEST_ASSERT_TRUE(bad.calls.size() > good.calls.size());  // retried every 10 s
}

void test_controller_redraws_on_network_change_and_request() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.step();
  // Before the first connection the network counts as connected for the
  // grace period (no Wi-Fi symbol struck through while starting).
  TEST_ASSERT_TRUE(renderer.states.back().network_connected);
  const size_t fetched = a.calls.size();
  // Connecting fetches at once instead of waiting for the next retry.
  c.setNetworkConnected(true);
  c.step();
  TEST_ASSERT_EQUAL_size_t(fetched + 1, a.calls.size());
  TEST_ASSERT_TRUE(renderer.states.back().network_connected);
  // Reporting the same state again does not fetch again.
  c.setNetworkConnected(true);
  c.step();
  TEST_ASSERT_EQUAL_size_t(fetched + 1, a.calls.size());
  c.requestUpdate();
  c.step();
  TEST_ASSERT_EQUAL_size_t(fetched + 2, a.calls.size());
}

void test_controller_shows_a_lost_connection_after_the_grace_period() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.setNetworkConnected(true);
  c.step();
  clock.advance(5000);
  c.setNetworkConnected(false);
  c.step();
  TEST_ASSERT_TRUE(renderer.states.back().network_connected);  // short dropout: not shown
  // The controller wakes up at the end of the grace period by itself.
  const int64_t wait = c.step();
  TEST_ASSERT_TRUE(wait <= core::DisplayController::kOfflineGraceMs);
  clock.advance(core::DisplayController::kOfflineGraceMs - 1);
  c.step();
  TEST_ASSERT_TRUE(c.state().network_connected);
  clock.advance(1);
  const size_t drawn = renderer.states.size();
  c.step();
  TEST_ASSERT_EQUAL_size_t(drawn + 1, renderer.states.size());
  TEST_ASSERT_FALSE(renderer.states.back().network_connected);
  // A reconnection is shown at once.
  clock.advance(1000);
  c.setNetworkConnected(true);
  c.step();
  TEST_ASSERT_TRUE(renderer.states.back().network_connected);
  // A dropout shorter than the grace period is never shown.
  c.setNetworkConnected(false);
  clock.advance(30000);
  c.step();
  c.setNetworkConnected(true);
  clock.advance(40000);
  c.step();
  TEST_ASSERT_TRUE(renderer.states.back().network_connected);
}

void test_controller_counts_the_grace_period_from_the_start() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.step();
  clock.advance(59000);
  c.step();
  TEST_ASSERT_TRUE(c.state().network_connected);
  clock.advance(1000);
  c.step();
  TEST_ASSERT_FALSE(c.state().network_connected);
}

void test_controller_start_screen_waits_for_the_network_and_first_results() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  FakeContent b;
  FakeContent header(false);
  c.addContent(a);
  c.addContent(b);
  c.addContent(header);
  c.enableStartScreen();
  a.succeed = false;
  b.succeed = false;

  // Failures without a network are no result: the start screen stays, also
  // after the time limit.
  c.step();
  TEST_ASSERT_TRUE(c.state().starting);
  TEST_ASSERT_FALSE(c.state().network_up);
  clock.advance(60000);
  c.step();
  TEST_ASSERT_TRUE(c.starting());

  // Connected: a's data and b's failure are both results.
  a.succeed = true;
  const size_t before = display.updates().size();
  const size_t drawn = renderer.states.size();
  c.setNetworkConnected(true);
  c.step();
  // Drawn once more before the fetches: "loading data" instead of "waiting
  // for Wi-Fi".
  TEST_ASSERT_TRUE(renderer.states.size() >= drawn + 2);
  TEST_ASSERT_TRUE(renderer.states[drawn].starting);
  TEST_ASSERT_TRUE(renderer.states[drawn].network_up);
  TEST_ASSERT_FALSE(c.starting());
  TEST_ASSERT_FALSE(c.state().starting);
  TEST_ASSERT_TRUE(c.state().network_up);
  TEST_ASSERT_EQUAL_size_t(before + 1, display.updates().size());
  TEST_ASSERT_TRUE(display.updates().back().mode == hal::Refresh::kFull);
}

void test_controller_start_screen_ends_with_a_wifi_notice_or_the_time_limit() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a(true, false);  // not set up: nothing to wait for
  c.addContent(a);
  c.enableStartScreen();
  c.step();
  TEST_ASSERT_TRUE(c.starting());  // no network yet
  core::AccessNotice notice;
  notice.mode = core::AccessMode::kWifiNotConnected;
  c.setAccessNotice(notice);
  c.step();
  TEST_ASSERT_FALSE(c.starting());

  // Time limit: 20 s after the start, or 15 s after a later first
  // connection.
  TEST_ASSERT_EQUAL_INT64(20000, core::DisplayController::kStartMaxMs);
  TEST_ASSERT_EQUAL_INT64(15000, core::DisplayController::kStartAfterConnectMs);
}

void test_controller_without_wall_clock_uses_plain_intervals() {
  FakeClock clock;
  clock.wall_valid = false;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.step();
  TEST_ASSERT_EQUAL_INT64(60000, c.step());
  // NTP arrives: redraw at once.
  const size_t drawn = renderer.states.size();
  clock.wall_valid = true;
  clock.advance(1000);
  c.step();
  TEST_ASSERT_EQUAL_size_t(drawn + 1, renderer.states.size());
}

void test_controller_logs_failures_once_and_the_recovery() {
  FakeClock clock;
  core::setLogClock([&] { return clock.ms; }, [&] { return clock.wallTime(); });
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.setNetworkConnected(true);
  const uint32_t seq = lastSeq();
  c.step();
  a.succeed = false;
  a.problem = "HTTP 503";
  // Retries every 10 s for 10 minutes: one warning only.
  while (clock.ms < 600000) clock.advance(std::max<int64_t>(c.step(), 1));
  std::vector<std::string> w = logged(seq, core::LogLevel::kWarning);
  TEST_ASSERT_EQUAL_size_t(1, w.size());
  TEST_ASSERT_EQUAL_STRING("fetch failed: HTTP 503 (0.0 s)", w[0].c_str());
  // Another reason is logged again.
  a.problem = "timed out after 0 bytes";
  while (clock.ms < 700000) clock.advance(std::max<int64_t>(c.step(), 1));
  TEST_ASSERT_EQUAL_size_t(2, logged(seq, core::LogLevel::kWarning).size());
  // After 30 minutes once as an error.
  while (clock.ms < 2000000) clock.advance(std::max<int64_t>(c.step(), 1));
  const std::vector<std::string> e = logged(seq, core::LogLevel::kError);
  TEST_ASSERT_EQUAL_size_t(1, e.size());
  TEST_ASSERT_EQUAL_STRING("failing for 30 min (181 attempts): timed out after 0 bytes",
                           e[0].c_str());
  const core::SourceReport r = c.sourceReports()[0];
  TEST_ASSERT_EQUAL_STRING("test", r.name.c_str());
  TEST_ASSERT_TRUE(r.failures_in_row > 180);
  TEST_ASSERT_EQUAL_STRING("timed out after 0 bytes", r.problem.c_str());
  TEST_ASSERT_TRUE(r.state == DataState::kStale);
  // Works again: one line with the count and the time.
  a.succeed = true;
  const unsigned failures = r.failures_in_row;
  clock.advance(std::max<int64_t>(c.step(), 1));
  c.step();
  const std::vector<std::string> i = logged(seq);
  TEST_ASSERT_FALSE(i.empty());
  char expected[96];
  std::snprintf(expected, sizeof(expected),
                "works again after %u failed attempts (0:33:", failures);
  TEST_ASSERT_EQUAL_STRING_LEN(expected, i.back().c_str(), std::strlen(expected));
  TEST_ASSERT_EQUAL(0, c.sourceReports()[0].failures_in_row);
  // The published copy for the settings page is the same.
  TEST_ASSERT_EQUAL(1, core::publishedSourceReports().size());
  core::setLogClock(nullptr, nullptr);
}

void test_controller_logs_a_summary_every_hour() {
  FakeClock clock;
  core::setLogClock([&] { return clock.ms; }, [&] { return clock.wallTime(); });
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  c.setNetworkConnected(true);
  int hooks = 0;
  c.setSummaryHook([&] { hooks++; });
  const uint32_t seq = lastSeq();
  while (clock.ms < core::DisplayController::kLogSummaryMs + 1000) {
    clock.advance(std::max<int64_t>(c.step(), 1));
  }
  const std::vector<std::string> i = logged(seq);
  TEST_ASSERT_EQUAL_size_t(1, i.size());
  TEST_ASSERT_EQUAL_STRING("last hour: 61 ok, 0 failed, avg 0.0, max 0.0 s", i[0].c_str());
  TEST_ASSERT_EQUAL(1, hooks);
  core::setLogClock(nullptr, nullptr);
}

void test_controller_system_page_closes_after_two_minutes() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a;
  c.addContent(a);
  clock.advance(c.step());
  const int64_t opened = clock.ms;
  c.showSystemPage(true);
  c.step();
  TEST_ASSERT_TRUE(renderer.states.back().system_page);
  TEST_ASSERT_TRUE(display.updates().back().mode == hal::Refresh::kFull);
  while (clock.ms < 200000) {
    const int64_t wait = c.step();
    if (!c.systemPageShown()) break;
    clock.advance(std::max<int64_t>(wait, 1));
  }
  TEST_ASSERT_FALSE(c.systemPageShown());
  TEST_ASSERT_EQUAL_INT64(opened + core::DisplayController::kSystemPageMs, clock.ms);
  TEST_ASSERT_FALSE(renderer.states.back().system_page);
  TEST_ASSERT_TRUE(display.updates().back().mode == hal::Refresh::kFull);
}

// A button pressed during a slow fetch stops it within a few hundred
// milliseconds; the fetch counts neither as success nor as failure, the
// page asked for by the button is drawn first, then the fetch is repeated.
void test_controller_stops_a_slow_fetch_for_a_button() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  SlowContent slow(clock, renderer.states);
  c.addContent(slow);
  int64_t press_ms = -1;  // no press
  core::setFetchCancelCheck([&] { return press_ms >= 0 && clock.ms >= press_ms; });

  clock.advance(c.step());  // first fetch: 5 s, not stopped
  TEST_ASSERT_EQUAL(1, slow.started_ms.size());
  TEST_ASSERT_EQUAL_INT64(5000, slow.started_ms.size() == 1 ? clock.ms - slow.started_ms[0] : 0);

  // The next fetch: a press 300 ms after its start (latched by the
  // interrupt; this simulated fetch started after the step's own clock).
  slow.started_ms.clear();
  slow.frames_before.clear();
  const uint32_t seq = lastSeq();
  while (slow.started_ms.empty()) {
    press_ms = clock.ms + 300;
    const int64_t wait = c.step();
    if (!slow.started_ms.empty()) {
      TEST_ASSERT_EQUAL_INT64(0, wait);  // the press is handled at once
      break;
    }
    clock.advance(std::max<int64_t>(wait, 1));
  }
  const int64_t answered_ms = clock.ms - press_ms;
  TEST_ASSERT_TRUE(answered_ms >= 0 && answered_ms < 1000);
  const core::SourceReport report = c.sourceReports()[0];
  TEST_ASSERT_EQUAL_UINT32(1, report.successes);
  TEST_ASSERT_EQUAL_UINT32(0, report.failures);
  TEST_ASSERT_EQUAL(0, logged(seq, core::LogLevel::kWarning).size());
  const std::vector<std::string> info = logged(seq);
  TEST_ASSERT_EQUAL(1, info.size());
  TEST_ASSERT_EQUAL_STRING("fetch stopped for a button after 0.3 s", info[0].c_str());

  // Handled (main loop: KEY2): the system page comes before the repeated fetch.
  press_ms = -1;
  c.showSystemPage(true);
  const size_t frames = renderer.states.size();
  c.step();
  TEST_ASSERT_EQUAL(2, slow.started_ms.size());
  TEST_ASSERT_EQUAL(frames + 1, slow.frames_before[1]);
  TEST_ASSERT_TRUE(renderer.states[frames].system_page);
  TEST_ASSERT_EQUAL_UINT32(2, c.sourceReports()[0].successes);
  TEST_ASSERT_EQUAL_UINT32(0, c.sourceReports()[0].failures);
  core::setFetchCancelCheck(nullptr);
}

// While a press waits, no fetch starts (the next one waits for the next pass).
void test_controller_starts_no_fetch_while_a_press_waits() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  FakeContent a, b;
  c.addContent(a);
  c.addContent(b);
  bool waiting = true;
  core::setFetchCancelCheck([&] { return waiting; });
  TEST_ASSERT_EQUAL_INT64(0, c.step());
  TEST_ASSERT_EQUAL(0, a.calls.size());
  TEST_ASSERT_EQUAL(0, b.calls.size());
  waiting = false;
  c.step();
  TEST_ASSERT_EQUAL(1, a.calls.size());
  TEST_ASSERT_EQUAL(1, b.calls.size());
  core::setFetchCancelCheck(nullptr);
}

// A successful but slow fetch is logged with its duration.
void test_controller_logs_a_slow_fetch() {
  FakeClock clock;
  hal::PngDisplay display(16, 8);
  FakeRenderer renderer;
  core::DisplayController c(clock, display, renderer, 60);
  SlowContent slow(clock, renderer.states);
  slow.duration_ms = core::DisplayController::kSlowFetchMs + 200;
  c.addContent(slow);
  const uint32_t seq = lastSeq();
  c.step();
  const std::vector<std::string> info = logged(seq);
  TEST_ASSERT_EQUAL(1, info.size());
  TEST_ASSERT_EQUAL_STRING("slow fetch: 5.2 s", info[0].c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_freshness_bridges_short_failures);
  RUN_TEST(test_freshness_without_data);
  RUN_TEST(test_region_differs_checks_only_the_region);
  RUN_TEST(test_planner_full_first_then_partial_then_full_after_interval);
  RUN_TEST(test_controller_first_step_fetches_and_shows_full);
  RUN_TEST(test_controller_waits_until_second_1_and_skips_unchanged_frames);
  RUN_TEST(test_controller_bridges_failures_then_shows_age);
  RUN_TEST(test_controller_not_set_up_is_never_fetched);
  RUN_TEST(test_controller_one_failing_content_does_not_stop_others);
  RUN_TEST(test_controller_redraws_on_network_change_and_request);
  RUN_TEST(test_controller_shows_a_lost_connection_after_the_grace_period);
  RUN_TEST(test_controller_counts_the_grace_period_from_the_start);
  RUN_TEST(test_controller_start_screen_waits_for_the_network_and_first_results);
  RUN_TEST(test_controller_start_screen_ends_with_a_wifi_notice_or_the_time_limit);
  RUN_TEST(test_controller_without_wall_clock_uses_plain_intervals);
  RUN_TEST(test_controller_logs_failures_once_and_the_recovery);
  RUN_TEST(test_controller_logs_a_summary_every_hour);
  RUN_TEST(test_controller_system_page_closes_after_two_minutes);
  RUN_TEST(test_controller_stops_a_slow_fetch_for_a_button);
  RUN_TEST(test_controller_starts_no_fetch_while_a_press_waits);
  RUN_TEST(test_controller_logs_a_slow_fetch);
  return UNITY_END();
}
