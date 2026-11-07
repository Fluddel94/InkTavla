#include "core/system_report.h"

#include <ArduinoJson.h>

#include <cstdio>

#include "core/fault_counts.h"
#include "core/time.h"

namespace core {

namespace {

std::mutex reports_mutex;
std::vector<SourceReport> reports;

}  // namespace

const char* signalQuality(int rssi_dbm) {
  if (rssi_dbm >= -55) return "excellent";
  if (rssi_dbm >= -67) return "good";
  if (rssi_dbm >= -75) return "fair";
  return "weak";
}

void publishSourceReports(const std::vector<SourceReport>& r) {
  std::lock_guard<std::mutex> lock(reports_mutex);
  reports = r;
}

std::vector<SourceReport> publishedSourceReports() {
  std::lock_guard<std::mutex> lock(reports_mutex);
  return reports;
}

void addFaultCounts(SystemReport& report, int64_t wall_s) {
  FaultCounts& counts = faultCounts();
  report.wifi_drops = counts.sinceStart(kWifiFaults);
  report.wifi_drops_24h = counts.last24h(kWifiFaults, wall_s);
  for (SourceReport& s : report.sources) {
    s.fails = counts.sinceStart(s.name.c_str());
    s.fails_24h = counts.last24h(s.name.c_str(), wall_s);
  }
}

const char* dataStateName(DataState state) {
  switch (state) {
    case DataState::kFresh:
      return "ok";
    case DataState::kStale:
      return "stale";
    case DataState::kWaiting:
      return "waiting";
    case DataState::kNoData:
      return "no data";
    case DataState::kNotSetUp:
      return "not set up";
  }
  return "";
}

std::string logExportHeader(const SystemReport& report, int64_t now_s) {
  const DeviceInfo& d = report.device;
  LogEntry now;
  now.wall_s = isValidUnixTime(now_s) ? now_s : 0;
  now.uptime_s = static_cast<uint32_t>(d.uptime_s);
  char line[200];
  std::string out = "# InkTavla system log\n";
  std::snprintf(line, sizeof(line), "# firmware %s, exported %s, uptime %s\n", d.firmware.c_str(),
                now.wall_s != 0 ? formatLogTime(now).c_str() : "(clock not set)",
                formatUptime(d.uptime_s).c_str());
  out += line;
  std::snprintf(line, sizeof(line), "# last restart: %s%s, start %u since power on\n",
                d.restart_reason.c_str(), d.restart_unplanned ? " (unplanned)" : "",
                static_cast<unsigned>(d.boot));
  out += line;
  if (d.wifi_connected) {
    std::snprintf(line, sizeof(line),
                  "# wifi: connected, signal %d dBm (%s), drops %u (last 24h %u)\n", d.rssi_dbm,
                  signalQuality(d.rssi_dbm), static_cast<unsigned>(report.wifi_drops),
                  static_cast<unsigned>(report.wifi_drops_24h));
  } else {
    std::snprintf(line, sizeof(line), "# wifi: not connected, drops %u (last 24h %u)\n",
                  static_cast<unsigned>(report.wifi_drops),
                  static_cast<unsigned>(report.wifi_drops_24h));
  }
  out += line;
  std::snprintf(
      line, sizeof(line),
      "# memory: heap %u KB free (lowest %u KB, largest block %u KB), PSRAM %u KB free\n",
      static_cast<unsigned>(d.heap_free / 1024), static_cast<unsigned>(d.heap_min_free / 1024),
      static_cast<unsigned>(d.heap_largest / 1024), static_cast<unsigned>(d.psram_free / 1024));
  out += line;
  for (const SourceReport& s : report.sources) {
    std::snprintf(line, sizeof(line), "# %s: %s, %u ok, %u failed, fails %u (last 24h %u)%s%s\n",
                  s.name.c_str(), dataStateName(s.state), static_cast<unsigned>(s.successes),
                  static_cast<unsigned>(s.failures), static_cast<unsigned>(s.fails),
                  static_cast<unsigned>(s.fails_24h),
                  s.problem.empty() ? "" : ", last problem: ", s.problem.c_str());
    out += line;
  }
  out += "# time                level area     text\n";
  return out;
}

std::string systemReportJson(const SystemReport& report) {
  JsonDocument doc;
  const DeviceInfo& d = report.device;
  JsonObject device = doc["device"].to<JsonObject>();
  device["firmware"] = d.firmware;
  device["uptime_s"] = d.uptime_s;
  device["restart_reason"] = d.restart_reason;
  device["restart_unplanned"] = d.restart_unplanned;
  device["boot"] = d.boot;
  device["wifi_connected"] = d.wifi_connected;
  if (d.wifi_connected) device["rssi"] = d.rssi_dbm;
  device["heap_free"] = d.heap_free;
  device["heap_min_free"] = d.heap_min_free;
  device["heap_largest"] = d.heap_largest;
  device["psram_free"] = d.psram_free;
  device["psram_size"] = d.psram_size;
  device["time_valid"] = d.time_valid;
  JsonArray sources = doc["sources"].to<JsonArray>();
  for (const SourceReport& s : report.sources) {
    JsonObject o = sources.add<JsonObject>();
    o["name"] = s.name;
    o["state"] = dataStateName(s.state);
    o["last_success"] = s.last_success_wall_s;
    o["last_failure"] = s.last_failure_wall_s;
    o["problem"] = s.problem;
    o["failures_in_row"] = s.failures_in_row;
    o["duration_ms"] = s.last_duration_ms;
    o["successes"] = s.successes;
    o["failures"] = s.failures;
    o["fails"] = s.fails;
    o["fails_24h"] = s.fails_24h;
  }
  doc["wifi"]["drops"] = report.wifi_drops;
  doc["wifi"]["drops_24h"] = report.wifi_drops_24h;
  doc["log"]["warnings"] = report.warnings;
  doc["log"]["errors"] = report.errors;
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace core
