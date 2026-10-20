// Unit tests for the display controller and its parts (core/freshness.h,
// core/refresh.h, core/display_controller.h) with a simulated clock, a
// simulated panel and fake contents.

#include <unity.h>

#include <algorithm>
#include <string>
#include <vector>

#include "core/display_controller.h"
#include "core/freshness.h"
#include "core/refresh.h"
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
    return succeed;
  }

  bool succeed = true;
  std::vector<int64_t> calls;

 private:
  bool fetches_;
  bool set_up_;
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
  const size_t drawn = renderer.states.size();
  c.setNetworkConnected(true);
  c.step();
  TEST_ASSERT_EQUAL_size_t(drawn + 1, renderer.states.size());
  TEST_ASSERT_TRUE(renderer.states.back().network_connected);
  c.requestUpdate();
  c.step();
  TEST_ASSERT_EQUAL_size_t(2, a.calls.size());
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
  RUN_TEST(test_controller_without_wall_clock_uses_plain_intervals);
  return UNITY_END();
}
