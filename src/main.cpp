// Entry point: wires the layers together. For now it only reports the
// firmware version and the memory sizes, which is the first check on new
// hardware (16 MB flash, 8 MB PSRAM expected).

#include <Arduino.h>

#include "core/version.h"

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // Do not block when no serial monitor is open.

  Serial.printf("Firmware %s\n", core::kFirmwareVersion);
  Serial.printf("Flash: %lu bytes, PSRAM: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()));
}

void loop() { delay(1000); }
