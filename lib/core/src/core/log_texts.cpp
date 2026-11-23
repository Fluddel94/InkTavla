#include "core/log_texts.h"

#include <cstdio>

#include "core/syslog.h"
#include "core/system_report.h"

namespace core {

namespace {

// "0.9", "12.3" (tenths, rounded down like secondsText())
std::string tenths(uint64_t ms) {
  char buffer[24];
  std::snprintf(buffer, sizeof(buffer), "%u.%u", static_cast<unsigned>(ms / 1000),
                static_cast<unsigned>(ms % 1000 / 100));
  return buffer;
}

unsigned kb(uint32_t bytes) { return static_cast<unsigned>(bytes / 1024); }

}  // namespace

std::string startText(const char* firmware_version, const char* restart_reason) {
  return std::string("start ") + firmware_version + ", reason: " + restart_reason;
}

std::string wifiConnectedText(int64_t lost_s, int rssi_dbm, int channel) {
  char buffer[80];
  std::snprintf(buffer, sizeof(buffer), "connected after %s, %d dBm, ch %d",
                formatUptime(lost_s).c_str(), rssi_dbm, channel);
  return buffer;
}

std::string wifiFailedText(bool lost, int rssi_dbm, int reason, const char* reason_name) {
  std::string text = lost ? "disconnected" : "attempt failed";
  // The driver reports 0 (or less than any real signal) without a signal.
  if (rssi_dbm < 0 && rssi_dbm > -127) text += ", " + std::to_string(rssi_dbm) + " dBm";
  text += ", reason " + std::to_string(reason) + " (" + reason_name + ")";
  return text;
}

std::string wifiSignalText(int rssi_dbm) {
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "signal %d dBm (%s)", rssi_dbm, signalQuality(rssi_dbm));
  return buffer;
}

std::string wifiRestartText(int failures) {
  return "Wi-Fi module restarted after " + std::to_string(failures) + " failed attempts";
}

std::string memoryText(uint32_t heap_free, uint32_t heap_min_free, uint32_t heap_largest,
                       uint32_t psram_free) {
  char buffer[80];
  std::snprintf(buffer, sizeof(buffer), "heap %u KB (min %u, block %u), PSRAM %u KB", kb(heap_free),
                kb(heap_min_free), kb(heap_largest), kb(psram_free));
  return buffer;
}

std::string fetchSummaryText(uint32_t successes, uint32_t failures, uint64_t avg_ms,
                             uint64_t max_ms) {
  char buffer[80];
  std::snprintf(buffer, sizeof(buffer), "last hour: %u ok, %u failed, avg %s, max %s s",
                static_cast<unsigned>(successes), static_cast<unsigned>(failures),
                tenths(avg_ms).c_str(), tenths(max_ms).c_str());
  return buffer;
}

std::string secondsText(uint64_t ms) { return tenths(ms) + " s"; }

}  // namespace core
