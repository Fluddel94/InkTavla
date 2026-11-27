#pragma once

// The state of the device at a glance, for the system page on the display
// (KEY2) and the tab "System" of the settings page: firmware, uptime, reason
// of the last restart, Wi-Fi signal, memory and per data source the last
// success and failure. Like the log (core/syslog.h) it holds no private
// data: no Wi-Fi name, no address, no station.

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "core/freshness.h"
#include "core/syslog.h"

namespace core {

// One data source as seen by the display controller.
struct SourceReport {
  std::string name;  // id of the source, e.g. "mvg"
  DataState state = DataState::kWaiting;
  int64_t last_success_wall_s = 0;  // 0: none (or before NTP)
  int64_t last_failure_wall_s = 0;
  std::string problem;           // of the last failure, e.g. "HTTP 404"
  uint32_t failures_in_row = 0;  // 0 while it works
  uint32_t last_duration_ms = 0;
  uint32_t successes = 0;  // since the start (attempts)
  uint32_t failures = 0;
  // Fails (series of failed attempts, core/fault_counts.h) since the start
  // and in the last 24 hours.
  uint32_t fails = 0;
  uint32_t fails_24h = 0;
};

// All fields but state equal (a new field belongs in here too).
inline bool sameExceptState(const SourceReport& a, const SourceReport& b) {
  return a.name == b.name && a.last_success_wall_s == b.last_success_wall_s &&
         a.last_failure_wall_s == b.last_failure_wall_s && a.problem == b.problem &&
         a.failures_in_row == b.failures_in_row && a.last_duration_ms == b.last_duration_ms &&
         a.successes == b.successes && a.failures == b.failures && a.fails == b.fails &&
         a.fails_24h == b.fails_24h;
}

// What only the hardware knows (filled by main.cpp, fixed in the PC
// simulation).
struct DeviceInfo {
  std::string firmware;
  int64_t uptime_s = 0;
  std::string restart_reason;  // e.g. "software restart"
  bool restart_unplanned = false;
  uint16_t boot = 1;  // starts since power on
  bool wifi_connected = false;
  int rssi_dbm = 0;  // signal while connected
  uint32_t heap_free = 0;
  uint32_t heap_min_free = 0;  // lowest since the start
  uint32_t heap_largest = 0;   // largest free block
  uint32_t psram_free = 0;
  uint32_t psram_size = 0;
  bool time_valid = false;
};

struct SystemReport {
  DeviceInfo device;
  std::vector<SourceReport> sources;
  uint32_t wifi_drops = 0;  // lost connections since the start
  uint32_t wifi_drops_24h = 0;
  size_t warnings = 0;  // in the log
  size_t errors = 0;
};

// Wi-Fi signal in words: "excellent" (>= -55 dBm), "good" (>= -67),
// "fair" (>= -75), "weak".
const char* signalQuality(int rssi_dbm);

// The sources as last published by the display controller (it runs in the
// main loop; the settings page reads them in its own task).
void publishSourceReports(const std::vector<SourceReport>& reports);
std::vector<SourceReport> publishedSourceReports();

// Fills the Wi-Fi drops and the fails of the sources from
// core/fault_counts.h (the sources must be filled already).
void addFaultCounts(SystemReport& report, int64_t wall_s);

// The report as JSON for the settings page (GET /api/system).
std::string systemReportJson(const SystemReport& report);

// First lines of the downloaded log ("# ..."): firmware, time of the
// export, uptime, restart reason, Wi-Fi signal, memory and the state of
// every source.
std::string logExportHeader(const SystemReport& report, int64_t now_s);

// "ok", "stale", "waiting", "no data", "not set up".
const char* dataStateName(DataState state);

}  // namespace core
