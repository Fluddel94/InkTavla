#if defined(ARDUINO)

#include "hal/trmnl_buttons.h"

#include <Arduino.h>
#include <esp_timer.h>

namespace hal {

namespace {

constexpr uint8_t kPins[TrmnlButtons::kCount] = {2, 3, 5};  // KEY1, KEY2, KEY3

// Written by the interrupt, read with interrupts disabled.
struct Latch {
  volatile bool latched = false;
  volatile int64_t press_at_ms = 0;
  volatile int64_t last_edge_ms = 0;
  volatile bool last_pressed = false;  // level read at the last edge
};
Latch latches[TrmnlButtons::kCount];

// 64-bit milliseconds (millis() wraps after 49 days).
inline int64_t IRAM_ATTR nowMs() { return esp_timer_get_time() / 1000; }

// Called on every edge (press and release).
void IRAM_ATTR onEdge(void* arg) {
  Latch& l = latches[reinterpret_cast<uintptr_t>(arg)];
  const uint8_t pin = kPins[reinterpret_cast<uintptr_t>(arg)];
  const int64_t now = nowMs();
  const bool pressed = digitalRead(pin) == LOW;
  if (acceptPressEdge(pressed, now, l.last_edge_ms, l.last_pressed)) {
    l.latched = true;
    l.press_at_ms = now;
  }
  l.last_edge_ms = now;
  l.last_pressed = pressed;
}

}  // namespace

void TrmnlButtons::begin() {
  for (size_t i = 0; i < kCount; i++) {
    pinMode(kPins[i], INPUT_PULLUP);
    latches[i].last_pressed = digitalRead(kPins[i]) == LOW;  // held at boot
    attachInterruptArg(digitalPinToInterrupt(kPins[i]), onEdge, reinterpret_cast<void*>(i), CHANGE);
  }
}

ButtonSample TrmnlButtons::sample(size_t i) {
  ButtonSample s;
  if (i >= kCount) return s;
  noInterrupts();
  s.press_latched = latches[i].latched;
  s.press_at_ms = latches[i].press_at_ms;
  s.last_edge_ms = latches[i].last_edge_ms;
  latches[i].latched = false;
  interrupts();
  s.pressed_now = digitalRead(kPins[i]) == LOW;
  return s;
}

bool TrmnlButtons::pressWaiting() const {
  for (const Latch& l : latches) {
    if (l.latched) return true;  // a single bool: no lock needed
  }
  return false;
}

}  // namespace hal

#endif
