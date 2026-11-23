#pragma once

// Texts of the system log (core/syslog.h) that carry numbers, in one place
// so a test can check that even their longest form fits a line of the
// system page on the display (test_briefing). Fixed English texts and
// numbers only (see core/syslog.h).

#include <cstdint>
#include <string>

namespace core {

// "start 0.3.0-dev, reason: software restart" (area "system"; the web page
// and the system page separate the starts at the text "start ").
std::string startText(const char* firmware_version, const char* restart_reason);

// "connected after 0:00:09, -64 dBm, ch 6" (area "wifi")
std::string wifiConnectedText(int64_t lost_s, int rssi_dbm, int channel);

// "attempt failed, -41 dBm, reason 202 (AUTH_FAIL)" or, for a
// lost connection, "disconnected, -78 dBm, reason 200 (BEACON_TIMEOUT)"
// (area "wifi"). The signal is the one of the access point that rejected or
// lost the device (diagnosis: a near access point refuses, a far one is
// taken); it stands before the reason name so a long name cannot push it
// off the line (the system page cuts long lines at the end). No signal (e.g. NO_AP_FOUND): left
// out.
std::string wifiFailedText(bool lost, int rssi_dbm, int reason, const char* reason_name);

// "signal -63 dBm (good)" (area "wifi", hourly)
std::string wifiSignalText(int rssi_dbm);

// "Wi-Fi module restarted after 6 failed attempts" (area "wifi")
std::string wifiRestartText(int failures);

// "heap 183 KB (min 141, block 96), PSRAM 7602 KB" (area "memory", hourly):
// free internal heap, its lowest value since the start, the largest free
// block, free PSRAM.
std::string memoryText(uint32_t heap_free, uint32_t heap_min_free, uint32_t heap_largest,
                       uint32_t psram_free);

// "last hour: 60 ok, 0 failed, avg 0.8, max 1.4 s" (area of the source)
std::string fetchSummaryText(uint32_t successes, uint32_t failures, uint64_t avg_ms,
                             uint64_t max_ms);

// "0.9 s", "12.3 s"
std::string secondsText(uint64_t ms);

}  // namespace core
