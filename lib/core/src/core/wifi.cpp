#if defined(ARDUINO)

#include "core/wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include <atomic>

#include "core/time.h"

namespace core {

namespace {

// NVS namespace and keys; never rename them, or stored credentials are lost
// with an update.
constexpr char kNamespace[] = "wifi";
constexpr char kKeySsid[] = "ssid";
constexpr char kKeyPassword[] = "pass";

// Reason ASSOC_LEAVE: own disconnect (WiFi.disconnect()), ends no attempt.
constexpr int kReasonAssocLeave = 8;

// Written in the Wi-Fi task, read in loop().
std::atomic<bool> disconnect_event{false};
std::atomic<bool> connected_event{false};

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    const int reason = info.wifi_sta_disconnected.reason;
    Serial.printf("Wi-Fi: disconnected, reason %d (%s)\n", reason,
                  WiFi.STA.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
    if (reason != kReasonAssocLeave) disconnect_event = true;
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    connected_event = true;
    Serial.printf("Wi-Fi: connected, channel %d, signal %d dBm\n", static_cast<int>(WiFi.channel()),
                  static_cast<int>(WiFi.RSSI()));
  }
}

}  // namespace

void Wifi::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // power saving causes dropouts
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  // The automatic reconnect of the ESP32 retries at once and without pause
  // and gets in the way of the own attempts.
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(onWifiEvent);
  std::string ssid, password;
  if (loadCredentials(ssid, password)) {
    has_credentials_ = true;
    connect(ssid, password);
  } else {
    Serial.println("Wi-Fi: no credentials yet, waiting for Improv");
  }
}

bool Wifi::connected() const { return WiFi.status() == WL_CONNECTED; }

void Wifi::connect(const std::string& ssid, const std::string& password) {
  has_credentials_ = true;
  attempt_started_ = true;
  WiFi.begin(ssid.c_str(), password.c_str());
}

void Wifi::loop() {
  const int64_t now = monotonicMs();
  if (attempt_started_.exchange(false)) policy_.attemptStarted(now);
  if (connected_event.exchange(false)) policy_.connected();
  if (disconnect_event.exchange(false)) policy_.disconnected(now);
  if (busy_) return;

  switch (policy_.poll(now, connected(), has_credentials_)) {
    case WifiPolicy::Action::kNone:
      return;
    case WifiPolicy::Action::kRestartAndConnect:
      Serial.println("Wi-Fi: restarting the Wi-Fi module");
      WiFi.mode(WIFI_OFF);
      delay(200);
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      [[fallthrough]];
    case WifiPolicy::Action::kConnect: {
      std::string ssid, password;
      if (!loadCredentials(ssid, password)) return;
      Serial.printf("Wi-Fi: new attempt (%d failed in a row)\n", policy_.failures());
      WiFi.begin(ssid.c_str(), password.c_str());
      return;
    }
  }
}

bool Wifi::hasCredentials() {
  std::string ssid, password;
  return loadCredentials(ssid, password);
}

bool Wifi::loadCredentials(std::string& ssid, std::string& password) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  ssid = prefs.getString(kKeySsid, "").c_str();
  password = prefs.getString(kKeyPassword, "").c_str();
  prefs.end();
  return !ssid.empty();
}

bool Wifi::saveCredentials(const std::string& ssid, const std::string& password) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putString(kKeySsid, ssid.c_str()) > 0 &&
                  (password.empty() || prefs.putString(kKeyPassword, password.c_str()) > 0);
  if (password.empty()) prefs.remove(kKeyPassword);
  prefs.end();
  return ok;
}

}  // namespace core

#endif
