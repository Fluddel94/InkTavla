// Entry point: provides the hardware parts (clock, panel, Wi-Fi, USB serial
// line) and runs the main loop of the display controller. Everything else is
// built from the configuration by widgets::App, the same code that runs in
// the PC simulation (test/test_app_simulation).

#include <Arduino.h>
#include <LittleFS.h>

#include <algorithm>
#include <string>

#include "core/config.h"
#include "core/display_controller.h"
#include "core/secrets.h"
#include "core/serial_link.h"
#include "core/time.h"
#include "core/version.h"
#include "core/wifi.h"
#include "hal/trmnl_display.h"
#include "widgets/app.h"
#include "widgets/calendar/api.h"
#include "widgets/mvg/api.h"
#include "widgets/weather/api.h"

namespace {

// Longest wait between two passes of the main loop (Wi-Fi checks).
constexpr int64_t kMaxWaitMs = 250;

core::SystemClock clock_source;
hal::TrmnlDisplay display;
core::Wifi wifi;
widgets::mvg::ApiSource mvg_source;
widgets::weather::ApiSource weather_source;
widgets::calendar::ApiSource calendar_source;
widgets::App app(clock_source, display, [] {
  widgets::Services s;
  s.mvg = &mvg_source;
  s.weather = &weather_source;
  s.calendar = &calendar_source;
  return s;
}());
bool time_sync_started = false;

void printIssues(const core::ConfigIssues& issues) {
  for (const core::ConfigIssue& i : issues) {
    Serial.printf("Config: %s: %s\n", i.path.empty() ? "(file)" : i.path.c_str(),
                  i.message.c_str());
  }
}

std::string info() {
  return std::string("firmware=") + core::kFirmwareVersion +
         " heap=" + std::to_string(ESP.getFreeHeap()) +
         " psram=" + std::to_string(ESP.getFreePsram()) +
         " wifi=" + (wifi.connected() ? "connected" : "disconnected") +
         // Secrets only as "set", never their value.
         " calendar_url=" + (calendar_source.configured() ? "set" : "unset");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // Do not block when no serial monitor is open.

  Serial.printf("Firmware %s\n", core::kFirmwareVersion);
  Serial.printf("Flash: %lu bytes, PSRAM: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()));

  calendar_source.begin();  // before the widgets ask whether it is set up

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

  core::SerialLinkOptions link;
  link.firmware_version = core::kFirmwareVersion;
  link.validate = widgets::validateConfig;
  link.info = info;
  core::startSerialLink(wifi, link);
  wifi.begin();
}

void loop() {
  wifi.loop();
  const bool connected = wifi.connected();
  if (connected && !time_sync_started) {
    core::startTimeSync(app.config().time_zone.c_str(), app.config().ntp_server.c_str());
    time_sync_started = true;
  }
  app.controller().setNetworkConnected(connected);
  const int64_t wait = app.step();
  delay(static_cast<uint32_t>(std::min<int64_t>(std::max<int64_t>(wait, 1), kMaxWaitMs)));
}
