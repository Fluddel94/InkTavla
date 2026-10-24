#if defined(ARDUINO)

#include "hal/trmnl_display.h"

#include <Arduino.h>

#include <algorithm>

// Pins of the EE04 driver board and register values of the UC8179 as used by
// Seeed_GFX (Seeed Studio, FreeBSD license) for this kit; register meanings
// from the UC8179 datasheet.

namespace hal {

namespace {

// GPIO numbers of the XIAO ESP32-S3 Plus.
constexpr int kPinSck = 7;      // D8
constexpr int kPinMosi = 9;     // D10
constexpr int kPinCs = 44;      // D7
constexpr int kPinDc = 10;      // D16
constexpr int kPinBusy = 4;     // D3, low while the controller is busy
constexpr int kPinReset = 38;   // D11
constexpr int kPinEnable = 43;  // D6, panel power

constexpr uint32_t kSpiHz = 10000000;

// UC8179 commands.
constexpr uint8_t kPanelSetting = 0x00;
constexpr uint8_t kPowerSetting = 0x01;
constexpr uint8_t kPowerOff = 0x02;
constexpr uint8_t kPowerOn = 0x04;
constexpr uint8_t kBoosterSoftStart = 0x06;
constexpr uint8_t kOldData = 0x10;
constexpr uint8_t kRefresh = 0x12;
constexpr uint8_t kNewData = 0x13;
constexpr uint8_t kDualSpi = 0x15;
constexpr uint8_t kVcomDataInterval = 0x50;
constexpr uint8_t kTcon = 0x60;
constexpr uint8_t kResolution = 0x61;
constexpr uint8_t kPartialWindow = 0x90;
constexpr uint8_t kPartialIn = 0x91;
constexpr uint8_t kPartialOut = 0x92;
constexpr uint8_t kCascade = 0xE0;
constexpr uint8_t kForceTemperature = 0xE5;

// Busy timeouts; a full refresh takes ~4 s at room temperature, longer
// when cold.
constexpr uint32_t kPowerTimeoutMs = 2000;
constexpr uint32_t kFullTimeoutMs = 20000;
constexpr uint32_t kPartialTimeoutMs = 8000;

}  // namespace

void TrmnlDisplay::begin() {
  pinMode(kPinEnable, OUTPUT);
  digitalWrite(kPinEnable, HIGH);
  pinMode(kPinCs, OUTPUT);
  digitalWrite(kPinCs, HIGH);
  pinMode(kPinDc, OUTPUT);
  pinMode(kPinReset, OUTPUT);
  digitalWrite(kPinReset, HIGH);
  pinMode(kPinBusy, INPUT);
  spi_.begin(kPinSck, -1, kPinMosi, -1);
  started_ = true;
}

void TrmnlDisplay::reset() {
  digitalWrite(kPinReset, LOW);
  delay(10);
  digitalWrite(kPinReset, HIGH);
  delay(10);
  waitWhileBusy("reset", kPowerTimeoutMs);
}

void TrmnlDisplay::command(uint8_t cmd) {
  spi_.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(kPinDc, LOW);
  digitalWrite(kPinCs, LOW);
  spi_.write(cmd);
  digitalWrite(kPinCs, HIGH);
  spi_.endTransaction();
}

void TrmnlDisplay::data(const uint8_t* bytes, size_t len) {
  spi_.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(kPinDc, HIGH);
  digitalWrite(kPinCs, LOW);
  spi_.writeBytes(bytes, len);
  digitalWrite(kPinCs, HIGH);
  spi_.endTransaction();
}

void TrmnlDisplay::command(uint8_t cmd, std::initializer_list<uint8_t> bytes) {
  command(cmd);
  data(bytes.begin(), bytes.size());
}

void TrmnlDisplay::sendRows(const uint8_t* frame, size_t stride, int bx0, int bx1, int y0, int y1) {
  uint8_t row[kWidth / 8];
  const int n = bx1 - bx0;
  for (int y = y0; y < y1; y++) {
    const uint8_t* src = frame + static_cast<size_t>(y) * stride + bx0;
    for (int i = 0; i < n; i++) row[i] = static_cast<uint8_t>(~src[i]);
    data(row, n);
  }
}

bool TrmnlDisplay::waitWhileBusy(const char* what, uint32_t timeout_ms) {
  delay(1);  // BUSY needs a moment to go low after a command.
  const uint32_t start = millis();
  while (digitalRead(kPinBusy) == LOW) {
    if (millis() - start > timeout_ms) {
      Serial.printf("Display: timeout while waiting for %s\n", what);
      ready_ = false;  // Reset and start over with the next update.
      return false;
    }
    delay(5);
  }
  return true;
}

void TrmnlDisplay::powerOn() {
  command(kPowerOn);
  waitWhileBusy("power on", kPowerTimeoutMs);
}

void TrmnlDisplay::powerOff() {
  command(kPowerOff);
  waitWhileBusy("power off", kPowerTimeoutMs);
}

void TrmnlDisplay::fullRefresh(const uint8_t* frame, size_t stride) {
  const bool first = !ready_;
  if (first) reset();
  command(kPowerSetting, {0x07, 0x07, 0x3F, 0x3F});  // VGH/VGL +-20 V, VDH/VDL +-15 V
  command(kBoosterSoftStart, {0x17, 0x17, 0x28, 0x17});
  command(kPanelSetting, {0x1F});  // black/white, waveform from OTP
  command(kResolution, {kWidth >> 8, kWidth & 0xFF, kHeight >> 8, kHeight & 0xFF});
  command(kDualSpi, {0x00});
  // Border white, copy new data to old after a refresh, set bit = white.
  command(kVcomDataInterval, {0x29, 0x07});
  command(kTcon, {0x22});
  command(kCascade, {0x00});  // Temperature from the sensor again (see partial).
  const int bytes = kWidth / 8;
  // Afterwards the controller keeps the old plane up to date by itself.
  if (first) {
    command(kOldData);
    sendRows(frame, stride, 0, bytes, 0, kHeight);
  }
  command(kNewData);
  sendRows(frame, stride, 0, bytes, 0, kHeight);
  ready_ = true;  // cleared by any timeout below
  powerOn();
  command(kRefresh);
  waitWhileBusy("full refresh", kFullTimeoutMs);
  powerOff();
}

void TrmnlDisplay::partialRefresh(const uint8_t* frame, size_t stride, const Rect& r) {
  const int x0 = std::max(r.x, 0) & ~7;
  const int x1 = std::min(r.x + r.w, static_cast<int>(kWidth));
  const int y0 = std::max(r.y, 0);
  const int y1 = std::min(r.y + r.h, static_cast<int>(kHeight));
  if (x0 >= x1 || y0 >= y1) return;
  const int xe = (x1 - 1) | 7;  // last pixel of the last byte
  const int ye = y1 - 1;
  command(kPartialIn);
  command(kPartialWindow, {static_cast<uint8_t>(x0 >> 8), static_cast<uint8_t>(x0 & 0xFF),
                           static_cast<uint8_t>(xe >> 8), static_cast<uint8_t>(xe & 0xFF),
                           static_cast<uint8_t>(y0 >> 8), static_cast<uint8_t>(y0 & 0xFF),
                           static_cast<uint8_t>(ye >> 8), static_cast<uint8_t>(ye & 0xFF),
                           0x01});  // refresh inside the window only
  command(kNewData);
  sendRows(frame, stride, x0 / 8, xe / 8 + 1, y0, y1);
  command(kRefresh);
  waitWhileBusy("partial refresh", kPartialTimeoutMs);
  command(kPartialOut);
}

void TrmnlDisplay::show(const uint8_t* frame, size_t stride, Refresh mode, const Rect* regions,
                        size_t region_count) {
  if (!started_) begin();
  const uint32_t start = millis();
  // A partial refresh needs a known panel content.
  if (mode == Refresh::kFull || !ready_) {
    fullRefresh(frame, stride);
    Serial.printf("Display: full refresh in %lu ms\n",
                  static_cast<unsigned long>(millis() - start));
    return;
  }
  // A high forced temperature selects the short waveform from the OTP.
  command(kCascade, {0x02});
  command(kForceTemperature, {0x6E});
  // Border floating, so it does not flash.
  command(kVcomDataInterval, {0xA9, 0x07});
  powerOn();
  for (size_t i = 0; i < region_count && ready_; i++) partialRefresh(frame, stride, regions[i]);
  powerOff();
  Serial.printf("Display: partial refresh of %u region(s) in %lu ms",
                static_cast<unsigned>(region_count), static_cast<unsigned long>(millis() - start));
  for (size_t i = 0; i < region_count; i++) {
    Serial.printf(" [%d,%d %dx%d]", regions[i].x, regions[i].y, regions[i].w, regions[i].h);
  }
  Serial.println();
}

}  // namespace hal

#endif
