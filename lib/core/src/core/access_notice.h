#pragma once

// What the screen shows besides the normal content:
// - the settings page is open in the home network (instead of the sentence
//   in the band), filled by the settings page;
// - there is no Wi-Fi: no network saved yet, or the saved one not reached
//   since the start (the whole screen, with the steps to set up Wi-Fi over
//   the USB cable), see wifiNoticeMode().
// Drawn above core (widgets/briefing/access.h).

#include <cstdint>
#include <string>

namespace core {

enum class AccessMode {
  kNone,
  kSettings,          // settings page open in the home network
  kWifiNotSetUp,      // no Wi-Fi network saved
  kWifiNotConnected,  // saved network not reached since the start
};

struct AccessNotice {
  AccessMode mode = AccessMode::kNone;
  // kSettings: address of the page, e.g. "http://192.168.178.45".
  std::string address;
  // kSettings: one-time code for the page. Shown on the display only,
  // never logged.
  std::string code;
  // kWifiNotConnected: name of the saved network.
  std::string network;
  // Contents of the QR code (kSettings: page address with code); empty =
  // no QR code.
  std::string qr;
  // kSettings: Unix seconds when the page closes; 0 = no time limit.
  int64_t closes_wall_s = 0;

  bool operator==(const AccessNotice& o) const {
    return mode == o.mode && address == o.address && code == o.code && network == o.network &&
           qr == o.qr && closes_wall_s == o.closes_wall_s;
  }
  bool operator!=(const AccessNotice& o) const { return !(*this == o); }
};

// The saved network is shown as not reachable only after this time without
// a connection since the start (the first attempts after a restart often
// fail, a router may still be starting after a power cut).
inline constexpr int64_t kWifiNoticeDelayMs = 10 * 60 * 1000;

// Which Wi-Fi notice the screen shows: kWifiNotSetUp at once without a
// saved network; kWifiNotConnected if a saved network was not reached for
// kWifiNoticeDelayMs since the start; kNone once connected (a connection
// lost later only shows the offline hints of the normal screen).
AccessMode wifiNoticeMode(bool credentials_saved, bool connected_since_start,
                          int64_t ms_since_start);

}  // namespace core
