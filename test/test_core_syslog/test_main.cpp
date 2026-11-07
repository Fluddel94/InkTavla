// Unit tests for the system log (core/syslog.h).

#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "core/fault_counts.h"
#include "core/syslog.h"
#include "core/system_report.h"
#include "core/time.h"

using core::LogEntry;
using core::LogLevel;
using core::PersistentLog;
using core::SystemLog;

namespace {

constexpr int64_t kNoon = 1792411200;  // 2026-10-19 12:00:00 UTC = 14:00 CEST

std::vector<std::string> texts(const SystemLog& log) {
  std::vector<std::string> result;
  LogEntry e;
  uint32_t seq = 0;
  while (log.next(seq, e)) {
    result.push_back(e.text);
    seq = e.seq;
  }
  return result;
}

}  // namespace

void setUp() { core::setTimeZone(core::kDefaultTimeZone); }
void tearDown() {}

void test_ring_keeps_the_newest_entries() {
  LogEntry storage[3];
  SystemLog log(storage, 3);
  for (int i = 1; i <= 5; i++) {
    log.add(LogLevel::kInfo, "test", std::to_string(i).c_str(), i * 1000, 0);
  }
  TEST_ASSERT_EQUAL(3, log.size());
  const std::vector<std::string> t = texts(log);
  TEST_ASSERT_EQUAL(3, t.size());
  TEST_ASSERT_EQUAL_STRING("3", t[0].c_str());
  TEST_ASSERT_EQUAL_STRING("5", t[2].c_str());

  // next() skips overwritten entries.
  LogEntry e;
  TEST_ASSERT_TRUE(log.next(0, e));
  TEST_ASSERT_EQUAL_STRING("3", e.text);
  TEST_ASSERT_EQUAL(3, e.seq);
  TEST_ASSERT_FALSE(log.next(5, e));
}

void test_newest_filters_by_level() {
  LogEntry storage[10];
  SystemLog log(storage, 10);
  log.add(LogLevel::kInfo, "a", "i1", 0, 0);
  log.add(LogLevel::kWarning, "a", "w1", 0, 0);
  log.add(LogLevel::kError, "a", "e1", 0, 0);
  log.add(LogLevel::kInfo, "a", "i2", 0, 0);
  std::vector<LogEntry> n = log.newest(2);
  TEST_ASSERT_EQUAL(2, n.size());
  TEST_ASSERT_EQUAL_STRING("i2", n[0].text);
  TEST_ASSERT_EQUAL_STRING("e1", n[1].text);
  n = log.newest(10, LogLevel::kWarning);
  TEST_ASSERT_EQUAL(2, n.size());
  TEST_ASSERT_EQUAL_STRING("e1", n[0].text);
  TEST_ASSERT_EQUAL_STRING("w1", n[1].text);
  TEST_ASSERT_EQUAL(1, log.count(LogLevel::kError));
  TEST_ASSERT_EQUAL(2, log.count(LogLevel::kWarning));
}

void test_long_texts_are_cut_without_breaking_characters() {
  LogEntry storage[2];
  SystemLog log(storage, 2);
  std::string text(core::kLogTextSize - 2, 'x');
  text += "äöü";  // "ä" fits half only
  log.add(LogLevel::kInfo, "areaislong", text.c_str(), 0, 0);
  LogEntry e;
  TEST_ASSERT_TRUE(log.next(0, e));
  TEST_ASSERT_EQUAL(core::kLogTextSize - 2, std::strlen(e.text));
  TEST_ASSERT_EQUAL_STRING("areaisl", std::string(e.area).substr(0, 7).c_str());
  TEST_ASSERT_EQUAL(core::kLogAreaSize - 1, std::strlen(e.area));
}

void test_time_before_ntp_is_time_since_start() {
  LogEntry storage[2];
  SystemLog log(storage, 2);
  log.add(LogLevel::kWarning, "wifi", "disconnected, reason 201 (NO_AP_FOUND)", 3725000, 5);
  log.add(LogLevel::kInfo, "time", "clock set", 4000000, kNoon);
  LogEntry e;
  TEST_ASSERT_TRUE(log.next(0, e));
  TEST_ASSERT_EQUAL(0, e.wall_s);  // 5 is no valid time
  TEST_ASSERT_EQUAL_STRING(
      "+1:02:05            WARN  wifi     disconnected, reason 201 (NO_AP_FOUND)",
      core::formatLogLine(e).c_str());
  TEST_ASSERT_TRUE(log.next(e.seq, e));
  TEST_ASSERT_EQUAL_STRING("2026-10-19 14:00:00 INFO  time     clock set",
                           core::formatLogLine(e).c_str());
}

void test_format_uptime() {
  TEST_ASSERT_EQUAL_STRING("0:00:00", core::formatUptime(0).c_str());
  TEST_ASSERT_EQUAL_STRING("4:05:09", core::formatUptime(4 * 3600 + 5 * 60 + 9).c_str());
  TEST_ASSERT_EQUAL_STRING("2 d 0:00:01", core::formatUptime(2 * 86400 + 1).c_str());
}

void test_important_entries_survive_a_restart() {
  PersistentLog kept;
  std::memset(static_cast<void*>(&kept), 0xA5, sizeof(kept));  // random memory after power on
  {
    LogEntry storage[10];
    SystemLog log(storage, 10);
    log.add(LogLevel::kInfo, "system", "early", 0, 0);  // before attach
    log.attachPersistent(&kept, false);                 // invalid checksum: starts empty
    TEST_ASSERT_EQUAL(1, log.boot());
    log.add(LogLevel::kInfo, "wifi", "connected", 1000, kNoon);
    log.add(LogLevel::kWarning, "mvg", "fetch failed: HTTP 503", 2000, kNoon);
    log.add(LogLevel::kInfo, "system", "restarting (settings saved)", 3000, kNoon);
    const std::vector<std::string> t = texts(log);
    TEST_ASSERT_EQUAL(4, t.size());
    TEST_ASSERT_EQUAL_STRING("early", t[0].c_str());
  }
  // Software restart: the warning and the system entries are back, the
  // plain info is not.
  LogEntry storage[10];
  SystemLog log(storage, 10);
  log.attachPersistent(&kept, false);
  TEST_ASSERT_EQUAL(2, log.boot());
  log.add(LogLevel::kInfo, "system", "start", 0, 0);
  const std::vector<std::string> t = texts(log);
  TEST_ASSERT_EQUAL(4, t.size());
  TEST_ASSERT_EQUAL_STRING("early", t[0].c_str());
  TEST_ASSERT_EQUAL_STRING("fetch failed: HTTP 503", t[1].c_str());
  TEST_ASSERT_EQUAL_STRING("restarting (settings saved)", t[2].c_str());
  TEST_ASSERT_EQUAL_STRING("start", t[3].c_str());
  LogEntry e;
  TEST_ASSERT_TRUE(log.next(0, e));
  TEST_ASSERT_EQUAL(1, e.boot);  // from the start before
  TEST_ASSERT_TRUE(log.next(3, e));
  TEST_ASSERT_EQUAL(2, e.boot);

  // Power on: everything is gone, counting starts again.
  LogEntry storage2[10];
  SystemLog fresh(storage2, 10);
  fresh.attachPersistent(&kept, true);
  TEST_ASSERT_EQUAL(1, fresh.boot());
  TEST_ASSERT_EQUAL(0, fresh.size());
}

void test_persistent_ring_keeps_its_capacity() {
  PersistentLog kept;
  {
    LogEntry storage[100];
    SystemLog log(storage, 100);
    log.attachPersistent(&kept, true);
    for (size_t i = 0; i < PersistentLog::kCapacity + 5; i++) {
      log.add(LogLevel::kError, "x", std::to_string(i).c_str(), 0, 0);
    }
  }
  LogEntry storage[100];
  SystemLog log(storage, 100);
  log.attachPersistent(&kept, false);
  const std::vector<std::string> t = texts(log);
  TEST_ASSERT_EQUAL(PersistentLog::kCapacity, t.size());
  TEST_ASSERT_EQUAL_STRING("5", t.front().c_str());
  TEST_ASSERT_EQUAL_STRING(std::to_string(PersistentLog::kCapacity + 4).c_str(), t.back().c_str());
}

void test_echo_and_global_log() {
  std::vector<std::string> echoed;
  core::systemLog().setEcho([&](const LogEntry& e) { echoed.push_back(e.text); });
  core::setLogClock([] { return int64_t{61000}; }, [] { return kNoon; });
  core::logWarning("wifi", "disconnected, reason %d", 8);
  core::systemLog().setEcho(nullptr);
  core::setLogClock(nullptr, nullptr);
  TEST_ASSERT_EQUAL(1, echoed.size());
  TEST_ASSERT_EQUAL_STRING("disconnected, reason 8", echoed[0].c_str());
  const std::vector<LogEntry> n = core::systemLog().newest(1);
  TEST_ASSERT_EQUAL(61, n[0].uptime_s);
  TEST_ASSERT_EQUAL(kNoon, n[0].wall_s);
  TEST_ASSERT_EQUAL(core::kLogCapacity, core::systemLog().capacity());
}

void test_fetch_problem_is_taken_once() {
  TEST_ASSERT_EQUAL_STRING("", core::takeFetchProblem().c_str());
  core::noteFetchProblem("HTTP %d", 404);
  TEST_ASSERT_EQUAL_STRING("HTTP 404", core::takeFetchProblem().c_str());
  TEST_ASSERT_EQUAL_STRING("", core::takeFetchProblem().c_str());
}

core::SystemReport sampleReport() {
  core::SystemReport r;
  r.device.firmware = "0.3.0-dev";
  r.device.uptime_s = 3725;
  r.device.restart_reason = "crash (panic)";
  r.device.restart_unplanned = true;
  r.device.boot = 2;
  r.device.wifi_connected = true;
  r.device.rssi_dbm = -70;
  r.device.heap_free = 180 * 1024;
  r.device.heap_min_free = 140 * 1024;
  r.device.heap_largest = 96 * 1024;
  r.device.psram_free = 7600 * 1024;
  core::SourceReport s;
  s.name = "calendar";
  s.state = core::DataState::kStale;
  s.successes = 5;
  s.failures = 2;
  s.fails = 1;
  s.fails_24h = 1;
  s.failures_in_row = 2;
  s.problem = "HTTP 404";
  r.sources.push_back(s);
  r.wifi_drops = 3;
  r.wifi_drops_24h = 1;
  r.warnings = 2;
  r.errors = 1;
  return r;
}

void test_export_header_and_report_json() {
  const core::SystemReport r = sampleReport();
  TEST_ASSERT_EQUAL_STRING(
      "# InkTavla system log\n"
      "# firmware 0.3.0-dev, exported 2026-10-19 14:00:00, uptime 1:02:05\n"
      "# last restart: crash (panic) (unplanned), start 2 since power on\n"
      "# wifi: connected, signal -70 dBm (fair), drops 3 (last 24h 1)\n"
      "# memory: heap 180 KB free (lowest 140 KB, largest block 96 KB), PSRAM 7600 KB free\n"
      "# calendar: stale, 5 ok, 2 failed, fails 1 (last 24h 1), last problem: HTTP 404\n"
      "# time                level area     text\n",
      core::logExportHeader(r, kNoon).c_str());
  const std::string json = core::systemReportJson(r);
  TEST_ASSERT_TRUE(json.find("\"restart_reason\":\"crash (panic)\"") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"state\":\"stale\"") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"rssi\":-70") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"log\":{\"warnings\":2,\"errors\":1}") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"fails\":1,\"fails_24h\":1") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"wifi\":{\"drops\":3,\"drops_24h\":1}") != std::string::npos);
  TEST_ASSERT_EQUAL_STRING("excellent", core::signalQuality(-50));
  TEST_ASSERT_EQUAL_STRING("weak", core::signalQuality(-80));
}

// --- Fault counters (core/fault_counts.h) ---------------------------------------------

constexpr int64_t kHour = 3600;

// Since the start and the last 24 hours, by name; the hour 24 hours ago
// drops out.
void test_fault_counts_since_start_and_last_24_hours() {
  core::FaultCounts f;
  f.add("wifi", kNoon);
  f.add("mvg", kNoon);
  f.add("mvg", kNoon + 2 * kHour);
  TEST_ASSERT_EQUAL_UINT32(1, f.sinceStart("wifi"));
  TEST_ASSERT_EQUAL_UINT32(2, f.sinceStart("mvg"));
  TEST_ASSERT_EQUAL_UINT32(0, f.sinceStart("calendar"));
  TEST_ASSERT_EQUAL_UINT32(2, f.last24h("mvg", kNoon + 2 * kHour));
  TEST_ASSERT_EQUAL_UINT32(2, f.last24h("mvg", kNoon + 23 * kHour + 3599));
  TEST_ASSERT_EQUAL_UINT32(1, f.last24h("mvg", kNoon + 24 * kHour));
  TEST_ASSERT_EQUAL_UINT32(0, f.last24h("mvg", kNoon + 26 * kHour));
  TEST_ASSERT_EQUAL_UINT32(0, f.last24h("wifi", kNoon + 24 * kHour));
  // A bucket used again a day later starts empty (the fault two hours
  // later still counts).
  f.add("mvg", kNoon + 24 * kHour);
  TEST_ASSERT_EQUAL_UINT32(2, f.last24h("mvg", kNoon + 24 * kHour));
  TEST_ASSERT_EQUAL_UINT32(3, f.sinceStart("mvg"));
  TEST_ASSERT_EQUAL_STRING("Fails 3 · Last 24h 1", core::faultCountText("Fails", 3, 1).c_str());
}

// Before the clock is set (after a restart): counted, and put into the
// hour in which the clock is first known.
void test_fault_counts_before_the_clock_is_set() {
  core::FaultCounts f;
  f.add("wifi", kNoon);
  f.add("wifi", 0);
  TEST_ASSERT_EQUAL_UINT32(2, f.last24h("wifi", 0));  // relative to the newest hour
  f.add("mvg", kNoon + 5 * kHour);  // the clock is set: the waiting one goes there
  TEST_ASSERT_EQUAL_UINT32(2, f.last24h("wifi", kNoon + 5 * kHour));
  TEST_ASSERT_EQUAL_UINT32(1, f.last24h("wifi", kNoon + 24 * kHour));
}

// The last 24 hours survive a restart (RTC memory), the counts since the
// start do not; after power on or with a wrong checksum it starts empty.
void test_fault_counts_survive_a_restart() {
  core::PersistentFaultCounts rtc;
  std::memset(&rtc, 0xA5, sizeof(rtc));  // random after power on
  {
    core::FaultCounts f;
    f.attachPersistent(&rtc, true);
    TEST_ASSERT_EQUAL_UINT32(0, f.last24h("mvg", kNoon));
    f.add("mvg", kNoon);
    f.add("wifi", kNoon);
  }
  {
    core::FaultCounts f;  // restart
    f.attachPersistent(&rtc, false);
    TEST_ASSERT_EQUAL_UINT32(0, f.sinceStart("mvg"));
    TEST_ASSERT_EQUAL_UINT32(1, f.last24h("mvg", kNoon + kHour));
    TEST_ASSERT_EQUAL_UINT32(1, f.last24h("wifi", 0));  // before NTP
    f.add("mvg", kNoon + kHour);
    TEST_ASSERT_EQUAL_UINT32(2, f.last24h("mvg", kNoon + kHour));
  }
  rtc.hours[0].counts[0] ^= 1;  // damaged
  {
    core::FaultCounts f;
    f.attachPersistent(&rtc, false);
    TEST_ASSERT_EQUAL_UINT32(0, f.last24h("mvg", kNoon + kHour));
  }
  // More names than slots: counted since the start only.
  core::FaultCounts f;
  for (int i = 0; i < 8; i++) f.add(("s" + std::to_string(i)).c_str(), kNoon);
  TEST_ASSERT_EQUAL_UINT32(1, f.sinceStart("s7"));
  TEST_ASSERT_EQUAL_UINT32(0, f.last24h("s7", kNoon));
  TEST_ASSERT_EQUAL_UINT32(1, f.last24h("s5", kNoon));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ring_keeps_the_newest_entries);
  RUN_TEST(test_newest_filters_by_level);
  RUN_TEST(test_long_texts_are_cut_without_breaking_characters);
  RUN_TEST(test_time_before_ntp_is_time_since_start);
  RUN_TEST(test_format_uptime);
  RUN_TEST(test_important_entries_survive_a_restart);
  RUN_TEST(test_persistent_ring_keeps_its_capacity);
  RUN_TEST(test_echo_and_global_log);
  RUN_TEST(test_fetch_problem_is_taken_once);
  RUN_TEST(test_export_header_and_report_json);
  RUN_TEST(test_fault_counts_since_start_and_last_24_hours);
  RUN_TEST(test_fault_counts_before_the_clock_is_set);
  RUN_TEST(test_fault_counts_survive_a_restart);
  return UNITY_END();
}
