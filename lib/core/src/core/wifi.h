#pragma once

// Wi-Fi on the device: connects with the credentials stored in NVS and
// reconnects after failures following WifiPolicy (from the MVG departure
// display): Wi-Fi power saving off, scan of all channels and the strongest
// access point (mesh, repeaters), growing pauses, Wi-Fi off and on after
// six failures in a row.
//
// Credentials are entered at runtime via Improv (core/serial_link.h) and
// kept only in NVS; they are never printed.

#if defined(ARDUINO)

#include <atomic>
#include <mutex>
#include <string>

#include "core/wifi_policy.h"

namespace core {

class Wifi {
 public:
  // Once in setup(): station mode, settings, events, first attempt.
  void begin();

  // Regularly from loop(): starts new attempts when needed (non-blocking,
  // except a short pause when Wi-Fi is restarted).
  void loop();

  bool connected() const;

  // While Improv uses Wi-Fi (connection test, scan) the own reconnection
  // pauses.
  void setBusy(bool busy) { busy_ = busy; }

  // Whether a network is saved (cached, no NVS access). Only credentials
  // that connected are saved; call markCredentialsSaved() after saving.
  bool credentialsSaved() const { return has_credentials_; }
  void markCredentialsSaved() { has_credentials_ = true; }

  // Name of the network a setup attempt (Improv) last failed with, "" if
  // none failed since the last successful setup. For the Wi-Fi notice.
  std::string failedNetwork() const;
  void setFailedNetwork(const std::string& ssid);

  static bool hasCredentials();
  static bool loadCredentials(std::string& ssid, std::string& password);
  static bool saveCredentials(const std::string& ssid, const std::string& password);

  // Starts an attempt with the given credentials (also from the Improv task;
  // the policy itself is only touched in loop()). Saves nothing.
  void connect(const std::string& ssid, const std::string& password);

 private:
  WifiPolicy policy_;
  std::atomic<bool> busy_{false};
  std::atomic<bool> has_credentials_{false};  // cached, NVS is not read every loop
  std::atomic<bool> attempt_started_{false};  // connect() may run in another task
  mutable std::mutex failed_mutex_;           // failed_network_ is set in the Improv task
  std::string failed_network_;
  // System log (loop() only): when the connection was lost (or the start,
  // before the first one), -1 while connected; the reason last logged.
  int64_t lost_ms_ = 0;
  int logged_reason_ = -1;
  bool restart_logged_ = false;
};

}  // namespace core

#endif
