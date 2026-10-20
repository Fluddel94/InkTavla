// Entry point: provides the hardware parts (clock, panel) and runs the main
// loop of the display controller. Everything else is built from the
// configuration by widgets::App, the same code that runs in the PC
// simulation (test/test_app_simulation).

#include <Arduino.h>
#include <LittleFS.h>

#include <algorithm>

#include "core/config.h"
#include "core/display_controller.h"
#include "core/version.h"
#include "hal/trmnl_display.h"
#include "widgets/app.h"

namespace {

// Longest wait between two passes of the main loop.
constexpr int64_t kMaxWaitMs = 1000;

core::SystemClock clock_source;
hal::TrmnlDisplay display;
// No network yet: the MVG widget has no source and shows "not set up".
widgets::Services services;
widgets::App app(clock_source, display, services);

void printIssues(const core::ConfigIssues& issues) {
  for (const core::ConfigIssue& i : issues) {
    Serial.printf("Config: %s: %s\n", i.path.empty() ? "(file)" : i.path.c_str(),
                  i.message.c_str());
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // Do not block when no serial monitor is open.

  Serial.printf("Firmware %s\n", core::kFirmwareVersion);
  Serial.printf("Flash: %lu bytes, PSRAM: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()));

  core::ConfigIssues issues;
  JsonDocument config;
  // Partition label "littlefs" (partitions.csv); formatted on first start.
  if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
    core::loadConfigFile(config, issues);
  } else {
    issues.push_back({"", "file system not available, using defaults"});
  }
  app.configure(config.as<JsonObjectConst>(), issues);
  printIssues(issues);
}

void loop() {
  const int64_t wait = app.step();
  delay(static_cast<uint32_t>(std::min<int64_t>(std::max<int64_t>(wait, 1), kMaxWaitMs)));
}
