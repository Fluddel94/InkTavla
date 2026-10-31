// Entry point: provides the hardware parts (clock, panel, Wi-Fi, USB serial
// line) and runs the main loop of the display controller. Everything else is
// built from the configuration by widgets::App, the same code that runs in
// the PC simulation (test/test_app_simulation).

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_ota_ops.h>

#include <algorithm>
#include <string>
#include <utility>

#include "core/config.h"
#include "core/config_command.h"
#include "core/display_controller.h"
#include "core/firmware_check.h"
#include "core/keys.h"
#include "core/secrets.h"
#include "core/serial_link.h"
#include "core/settings_server.h"
#include "core/time.h"
#include "core/version.h"
#include "core/wifi.h"
#include "hal/trmnl_buttons.h"
#include "hal/trmnl_display.h"
#include "widgets/app.h"
#include "widgets/registry.h"

// Generated from web/ by tools/embed_web.py before every build.
#include "generated/settings_page.h"

namespace {

// Longest wait between two passes of the main loop (Wi-Fi checks).
constexpr int64_t kMaxWaitMs = 250;

core::SystemClock clock_source;
hal::TrmnlDisplay display;
core::Wifi wifi;
hal::TrmnlButtons buttons;
core::SettingsServer settings;
core::ButtonLogic button_logic[core::kKeyCount];
// The APIs of the widgets (widgets/registry.cpp) and the app; created in
// setup(), because the calendar API reads NVS (not possible before).
widgets::Services services;
widgets::App* app = nullptr;
bool time_sync_started = false;
bool connected_since_start = false;
bool firmware_pending = false;  // new firmware not confirmed yet
bool notice_whole_screen = false;
// No settings yet: the settings page opens once Wi-Fi is connected.
bool open_settings_once = false;
std::string wifi_network;  // name of the saved network, for the notice

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
        if (wifi.connected() || settings.isOpen()) {
          settings.toggle();
        } else {
          Serial.println("Settings page: needs Wi-Fi");
        }
        break;
      case core::KeyAction::kNone:
        break;
    }
  }
}

// The notice on the display: the settings page while it is open, else the
// steps to set up Wi-Fi when there is none (see core::wifiNoticeMode()).
void updateNotice(bool connected) {
  if (connected) connected_since_start = true;
  core::AccessNotice notice = settings.notice(clock_source.wallTime());
  if (notice.mode == core::AccessMode::kNone) {
    notice.mode =
        core::wifiNoticeMode(wifi.credentialsSaved(), connected_since_start, core::monotonicMs());
    if (notice.mode == core::AccessMode::kWifiNotConnected) {
      if (wifi_network.empty()) {
        std::string password;
        core::Wifi::loadCredentials(wifi_network, password);
      }
      notice.network = wifi_network;
    } else if (notice.mode == core::AccessMode::kWifiNotSetUp) {
      notice.network = wifi.failedNetwork();  // a failed Improv attempt, if any
    }
  }
  const bool whole_screen = notice.mode == core::AccessMode::kWifiNotSetUp ||
                            notice.mode == core::AccessMode::kWifiNotConnected;
  if (whole_screen != notice_whole_screen) {
    notice_whole_screen = whole_screen;
    app->controller().requestFullRefresh();  // the whole screen changes
  }
  app->controller().setAccessNotice(notice);
}

// A firmware from the settings page is confirmed once it runs properly;
// if it crashes or restarts before, the bootloader goes back to the
// previous one.
void confirmFirmware(bool connected) {
  if (!firmware_pending || !core::firmwareConfirmed(core::monotonicMs(), connected)) return;
  esp_ota_mark_app_valid_cancel_rollback();
  firmware_pending = false;
  Serial.println("Firmware: new version confirmed");
}

}  // namespace

// The Arduino core would confirm a new firmware at once; it is confirmed in
// confirmFirmware() instead.
extern "C" bool verifyRollbackLater() { return true; }

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

  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &ota_state) == ESP_OK &&
      ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    firmware_pending = true;
    Serial.println("Firmware: new version, confirmed once it runs");
  }

  services = widgets::deviceServices();
  static widgets::App the_app(clock_source, display, services);
  app = &the_app;

  core::ConfigIssues issues;
  JsonDocument config;
  // Partition label "littlefs" (partitions.csv); formatted on first start.
  if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
    const bool exists = LittleFS.exists(core::kConfigPath);
    const bool loaded = core::loadConfigFile(config, issues);
    open_settings_once = core::opensSettingsOnStart(exists, loaded, config.as<JsonVariantConst>());
  } else {
    issues.push_back({"", "file system not available, using defaults"});
  }
  app->configure(config.as<JsonObjectConst>(), issues);
  printIssues(issues);
  {
    std::string password;
    core::Wifi::loadCredentials(wifi_network, password);
    app->setStartInfo({core::kFirmwareVersion, wifi_network});
  }

  core::SerialLinkOptions link;
  link.firmware_version = core::kFirmwareVersion;
  link.validate = widgets::validateConfig;
  link.info = info;
  core::startSerialLink(wifi, link);
  wifi.begin();

  core::SettingsServerOptions page;
  page.page_gz = kSettingsPageGz;
  page.page_gz_size = sizeof(kSettingsPageGz);
  page.firmware_version = core::kFirmwareVersion;
  page.language = app->config().language;
  page.validate = widgets::validateConfig;
  page.layout_json = widgets::settingsLayoutJson;
  page.routes = widgets::settingsRoutes();
  settings.begin(wifi, std::move(page));

  for (size_t i = 0; i < core::kKeyCount; i++) {
    button_logic[i].setLongPress(core::keyHasLongPress(i));
  }
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
  if (open_settings_once && connected) {
    open_settings_once = false;
    settings.openIfClosed();
  }
  updateNotice(connected);
  confirmFirmware(connected);
  const int64_t wait = app->step();
  delay(static_cast<uint32_t>(std::min<int64_t>(std::max<int64_t>(wait, 1), kMaxWaitMs)));
}
