// Entry point: provides the hardware parts (clock, panel, Wi-Fi, USB serial
// line) and runs the main loop of the display controller. Everything else is
// built from the configuration by widgets::App, the same code that runs in
// the PC simulation (test/test_app_simulation).

#include <Arduino.h>
#include <LittleFS.h>

#include <algorithm>
#include <string>

#include "core/config.h"
#include "core/config_command.h"
#include "core/display_controller.h"
#include "core/keys.h"
#include "core/secrets.h"
#include "core/serial_link.h"
#include "core/time.h"
#include "core/version.h"
#include "core/wifi.h"
#include "hal/trmnl_buttons.h"
#include "hal/trmnl_display.h"
#include "widgets/app.h"
#include "widgets/registry.h"

namespace {

// Longest wait between two passes of the main loop (Wi-Fi checks).
constexpr int64_t kMaxWaitMs = 250;

core::SystemClock clock_source;
hal::TrmnlDisplay display;
core::Wifi wifi;
hal::TrmnlButtons buttons;
core::ButtonLogic button_logic[core::kKeyCount];
// The APIs of the widgets (widgets/registry.cpp) and the app; created in
// setup(), because the calendar API reads NVS (not possible before).
widgets::Services services;
widgets::App* app = nullptr;
bool time_sync_started = false;
bool connected_since_start = false;
core::AccessMode wifi_notice = core::AccessMode::kNone;

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
         " calendar_url=" +
         (services.calendar != nullptr && services.calendar->configured() ? "set" : "unset");
}

void handleButtons() {
  const int64_t now = core::monotonicMs();
  for (size_t i = 0; i < core::kKeyCount; i++) {
    const core::KeyAction action =
        core::keyAction(i, button_logic[i].update(now, buttons.sample(i)));
    if (action == core::KeyAction::kNone) continue;
    Serial.printf("Key %u: %s\n", static_cast<unsigned>(i + 1), core::keyActionName(action));
    switch (action) {
      case core::KeyAction::kRefresh:
        app->controller().requestUpdate();
        app->controller().requestFullRefresh();
        break;
      case core::KeyAction::kSettings:
        Serial.println("Not available yet.");
        break;
      case core::KeyAction::kNone:
        break;
    }
  }
}

// Without Wi-Fi the whole screen shows how to set it up over USB (see
// core::wifiNoticeMode()).
void updateWifiNotice(bool connected) {
  if (connected) connected_since_start = true;
  const core::AccessMode mode =
      core::wifiNoticeMode(wifi.credentialsSaved(), connected_since_start, core::monotonicMs());
  if (mode == wifi_notice) return;
  wifi_notice = mode;
  core::AccessNotice notice;
  notice.mode = mode;
  if (mode == core::AccessMode::kWifiNotConnected) {
    std::string password;
    core::Wifi::loadCredentials(notice.network, password);
  }
  app->controller().setAccessNotice(notice);
  app->controller().requestFullRefresh();  // the whole screen changes
}

}  // namespace

void setup() {
  // The USB serial line drops what does not fit into its receive buffer
  // (default 256 bytes); a whole configuration file arrives at once.
  Serial.setRxBufferSize(core::kMaxConfigBytes + core::CommandParser::kMaxLine);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // Do not block when no serial monitor is open.

  Serial.printf("Firmware %s\n", core::kFirmwareVersion);
  Serial.printf("Flash: %lu bytes, PSRAM: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()));

  services = widgets::deviceServices();
  static widgets::App the_app(clock_source, display, services);
  app = &the_app;

  core::ConfigIssues issues;
  JsonDocument config;
  // Partition label "littlefs" (partitions.csv); formatted on first start.
  if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
    core::loadConfigFile(config, issues);
  } else {
    issues.push_back({"", "file system not available, using defaults"});
  }
  app->configure(config.as<JsonObjectConst>(), issues);
  printIssues(issues);

  core::SerialLinkOptions link;
  link.firmware_version = core::kFirmwareVersion;
  link.validate = widgets::validateConfig;
  link.info = info;
  core::startSerialLink(wifi, link);
  wifi.begin();

  for (size_t i = 0; i < core::kKeyCount; i++)
    button_logic[i].setLongPress(core::keyHasLongPress(i));
  buttons.begin();
}

void loop() {
  handleButtons();
  wifi.loop();
  const bool connected = wifi.connected();
  if (connected && !time_sync_started) {
    core::startTimeSync(app->config().time_zone.c_str(), app->config().ntp_server.c_str());
    time_sync_started = true;
  }
  app->controller().setNetworkConnected(connected);
  updateWifiNotice(connected);
  const int64_t wait = app->step();
  delay(static_cast<uint32_t>(std::min<int64_t>(std::max<int64_t>(wait, 1), kMaxWaitMs)));
}
