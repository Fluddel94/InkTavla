// Buttons: debouncing, short and long presses (rules of the MVG departure
// display), the key table and when the screen shows the Wi-Fi notice.

#include <unity.h>

#include "core/access_notice.h"
#include "core/buttons.h"
#include "core/keys.h"
#include "core/refresh.h"

using core::ButtonEvent;
using core::ButtonLogic;
using core::ButtonSample;

namespace {

ButtonSample pressed(int64_t at_ms) {
  ButtonSample s;
  s.press_latched = true;
  s.press_at_ms = at_ms;
  s.pressed_now = true;
  s.last_edge_ms = at_ms;
  return s;
}

ButtonSample held(int64_t since_ms) {
  ButtonSample s;
  s.pressed_now = true;
  s.last_edge_ms = since_ms;
  return s;
}

ButtonSample released(int64_t at_ms) {
  ButtonSample s;
  s.last_edge_ms = at_ms;
  return s;
}

// The interrupt of hal/trmnl_buttons.cpp on the PC: records the edges of
// one button, sample() is read by the main loop.
struct EdgeSim {
  bool latched = false;
  int64_t press_at_ms = 0;
  int64_t last_edge_ms = -100000;
  bool last_pressed = false;
  bool level = false;  // level of the pin (pressed = true)

  // An edge; read = level the interrupt reads (while bouncing it can differ
  // from the new level of the pin).
  void edge(int64_t now_ms, bool new_level, bool read) {
    level = new_level;
    if (hal::acceptPressEdge(read, now_ms, last_edge_ms, last_pressed)) {
      latched = true;
      press_at_ms = now_ms;
    }
    last_edge_ms = now_ms;
    last_pressed = read;
  }
  void edge(int64_t now_ms, bool new_level) { edge(now_ms, new_level, new_level); }

  ButtonSample sample() {
    ButtonSample s;
    s.press_latched = latched;
    s.press_at_ms = press_at_ms;
    s.last_edge_ms = last_edge_ms;
    s.pressed_now = level;
    latched = false;
    return s;
  }
};

// Runs the main loop every 10 ms from from_ms to to_ms; counts short presses.
int shortPresses(ButtonLogic& b, EdgeSim& pin, int64_t from_ms, int64_t to_ms) {
  int n = 0;
  for (int64_t t = from_ms; t <= to_ms; t += 10) {
    if (b.update(t, pin.sample()) == ButtonEvent::kShort) n++;
  }
  return n;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_edge_rule_of_the_interrupt() {
  // A press counts only after the button was released for 30 ms.
  TEST_ASSERT_TRUE(hal::acceptPressEdge(true, 1000, 900, false));
  TEST_ASSERT_TRUE(hal::acceptPressEdge(true, 1030, 1000, false));
  TEST_ASSERT_FALSE(hal::acceptPressEdge(true, 1029, 1000, false));   // bouncing
  TEST_ASSERT_FALSE(hal::acceptPressEdge(false, 2000, 1000, false));  // release
  // The previous edge read "pressed": no new press, however long ago.
  TEST_ASSERT_FALSE(hal::acceptPressEdge(true, 6000, 1000, true));
}

void test_long_hold_of_a_key_without_long_press_counts_once() {
  // Bug of 25.10.2026: KEY3 held for 5 s opened the settings page and closed
  // it again. When released, the first bouncing edge read "pressed"; the
  // previous edge was 5 s ago, so it counted as a second press.
  ButtonLogic b(false);
  EdgeSim pin;
  pin.edge(1000, true);
  TEST_ASSERT_EQUAL(1, shortPresses(b, pin, 1000, 6000));
  pin.edge(6000, false, true);  // released, interrupt still reads "pressed"
  pin.edge(6002, true);
  pin.edge(6004, false);
  TEST_ASSERT_EQUAL(0, shortPresses(b, pin, 6010, 8000));
  // The next real press still counts.
  pin.edge(9000, true);
  pin.edge(9150, false);
  TEST_ASSERT_EQUAL(1, shortPresses(b, pin, 9000, 9500));
}

void test_bouncing_press_and_release_count_once() {
  // Bouncing at the press (the interrupt reads "released" once) and at the
  // release after a long hold.
  ButtonLogic b(false);
  EdgeSim pin;
  pin.edge(1000, true);
  pin.edge(1003, false);
  pin.edge(1005, true);
  TEST_ASSERT_EQUAL(1, shortPresses(b, pin, 1010, 2990));
  pin.edge(3000, false, true);
  pin.edge(3001, true, false);
  pin.edge(3004, false);
  TEST_ASSERT_EQUAL(0, shortPresses(b, pin, 3010, 4000));
}

void test_short_press_on_release() {
  ButtonLogic b;
  TEST_ASSERT_TRUE(b.update(1000, pressed(1000)) == ButtonEvent::kNone);
  TEST_ASSERT_TRUE(b.update(1200, held(1000)) == ButtonEvent::kNone);
  // Released, but the pin is still bouncing.
  TEST_ASSERT_TRUE(b.update(1310, released(1300)) == ButtonEvent::kNone);
  TEST_ASSERT_TRUE(b.update(1330, released(1300)) == ButtonEvent::kShort);
  TEST_ASSERT_TRUE(b.update(1400, released(1300)) == ButtonEvent::kNone);
}

void test_press_during_a_blocked_loop_is_not_lost() {
  // Pressed and released while the panel refreshed for 4 s: evaluated
  // afterwards from the latched press.
  ButtonLogic b;
  ButtonSample s = released(1200);
  s.press_latched = true;
  s.press_at_ms = 1000;
  TEST_ASSERT_TRUE(b.update(5000, s) == ButtonEvent::kShort);
}

void test_long_press_while_held() {
  ButtonLogic b;
  b.update(1000, pressed(1000));
  TEST_ASSERT_TRUE(b.update(5999, held(1000)) == ButtonEvent::kNone);
  TEST_ASSERT_TRUE(b.update(6000, held(1000)) == ButtonEvent::kLong);
  TEST_ASSERT_TRUE(b.update(7000, held(1000)) == ButtonEvent::kNone);  // only once
  // No short press after a long one.
  TEST_ASSERT_TRUE(b.update(8000, released(7900)) == ButtonEvent::kNone);
}

void test_without_long_press_reported_at_once() {
  ButtonLogic b(false);
  TEST_ASSERT_TRUE(b.update(1000, pressed(1000)) == ButtonEvent::kShort);
  TEST_ASSERT_TRUE(b.update(1100, held(1000)) == ButtonEvent::kNone);
}

void test_min_interval_and_old_presses() {
  ButtonLogic b(false);
  TEST_ASSERT_TRUE(b.update(1000, pressed(1000)) == ButtonEvent::kShort);
  // Second press 400 ms later: ignored (minimum interval 500 ms).
  TEST_ASSERT_TRUE(b.update(1400, pressed(1400)) == ButtonEvent::kNone);
  TEST_ASSERT_TRUE(b.update(1600, pressed(1600)) == ButtonEvent::kShort);
  // A press latched more than 15 s ago is dropped.
  TEST_ASSERT_TRUE(b.update(20000, pressed(4000)) == ButtonEvent::kNone);
}

void test_key_table() {
  TEST_ASSERT_TRUE(core::keyAction(0, ButtonEvent::kShort) == core::KeyAction::kRefresh);
  TEST_ASSERT_TRUE(core::keyAction(1, ButtonEvent::kShort) == core::KeyAction::kNone);
  TEST_ASSERT_TRUE(core::keyAction(2, ButtonEvent::kShort) == core::KeyAction::kSettings);
  TEST_ASSERT_TRUE(core::keyAction(2, ButtonEvent::kLong) == core::KeyAction::kNone);
  TEST_ASSERT_TRUE(core::keyAction(3, ButtonEvent::kShort) == core::KeyAction::kNone);
  TEST_ASSERT_FALSE(core::keyHasLongPress(0));
  TEST_ASSERT_FALSE(core::keyHasLongPress(2));
}

void test_wifi_notice() {
  using core::AccessMode;
  constexpr int64_t kDelay = core::kWifiNoticeDelayMs;
  // No network saved: at once.
  TEST_ASSERT_TRUE(core::wifiNoticeMode(false, false, 0) == AccessMode::kWifiNotSetUp);
  // Saved network: only after 10 min without a connection since the start.
  TEST_ASSERT_TRUE(core::wifiNoticeMode(true, false, kDelay - 1) == AccessMode::kNone);
  TEST_ASSERT_TRUE(core::wifiNoticeMode(true, false, kDelay) == AccessMode::kWifiNotConnected);
  // Connected once: a later loss shows only the offline hints.
  TEST_ASSERT_TRUE(core::wifiNoticeMode(true, true, 5 * kDelay) == AccessMode::kNone);
}

void test_requested_full_refresh() {
  uint8_t a[100] = {};
  const core::FrameView frame{a, 80, 10, 10};
  core::RefreshPlanner planner(60);
  core::RefreshPlan plan = planner.plan({}, frame, {{0, 0, 80, 10}}, 0);
  planner.done(plan, 0);
  // Nothing changed: no refresh, unless a full one was requested.
  TEST_ASSERT_FALSE(planner.plan(frame, frame, {{0, 0, 80, 10}}, 1000).needed());
  planner.requestFull();
  plan = planner.plan(frame, frame, {{0, 0, 80, 10}}, 1000);
  TEST_ASSERT_TRUE(plan.mode == hal::Refresh::kFull);
  planner.done(plan, 1000);
  TEST_ASSERT_FALSE(planner.plan(frame, frame, {{0, 0, 80, 10}}, 2000).needed());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_edge_rule_of_the_interrupt);
  RUN_TEST(test_long_hold_of_a_key_without_long_press_counts_once);
  RUN_TEST(test_bouncing_press_and_release_count_once);
  RUN_TEST(test_short_press_on_release);
  RUN_TEST(test_press_during_a_blocked_loop_is_not_lost);
  RUN_TEST(test_long_press_while_held);
  RUN_TEST(test_without_long_press_reported_at_once);
  RUN_TEST(test_min_interval_and_old_presses);
  RUN_TEST(test_key_table);
  RUN_TEST(test_wifi_notice);
  RUN_TEST(test_requested_full_refresh);
  return UNITY_END();
}
