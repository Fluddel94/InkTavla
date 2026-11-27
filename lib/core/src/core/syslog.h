#pragma once

// System log: what the device does and why something fails, readable
// without a USB cable (system page on the display with KEY2, tab "System"
// on the settings page, download as text, "INKTAVLA LOG" over USB).
//
// Entries are kept in a ring in PSRAM (kLogCapacity, about a week of normal
// operation). Warnings, errors and the entries of the area "system" (start,
// planned restarts) are also copied to a small ring in RTC memory that
// survives a restart or a crash (not a power cut), so the lines before a
// crash are still there after it. Nothing is written to flash.
//
// Privacy (Grundsatz 12): entries are built from fixed texts and numbers
// only. Never pass text from outside the firmware: no URL, no calendar
// address or title, no Wi-Fi name or password, no IP address, no station,
// no coordinates, no content of a response. Every line may end up in a
// file the user shares to get help.

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace core {

enum class LogLevel : uint8_t { kInfo = 0, kWarning = 1, kError = 2 };

inline constexpr size_t kLogAreaSize = 9;   // "calendar" + terminator
inline constexpr size_t kLogTextSize = 76;  // longer texts are cut

struct LogEntry {
  int64_t wall_s = 0;     // Unix seconds, 0 before NTP
  uint32_t seq = 0;       // running number in this start, from 1
  uint32_t uptime_s = 0;  // seconds since the start the entry was made in
  uint16_t boot = 0;      // number of that start (see SystemLog::boot())
  LogLevel level = LogLevel::kInfo;
  char area[kLogAreaSize] = {};  // e.g. "wifi", "mvg"
  char text[kLogTextSize] = {};
};

// Ring of RTC memory kept over a restart; also used in tests.
struct PersistentLog {
  static constexpr size_t kCapacity = 36;
  uint32_t magic;
  uint16_t boot;
  uint16_t count;
  uint16_t next;
  uint16_t reserved;
  uint32_t checksum;
  LogEntry entries[kCapacity];
};

class SystemLog {
 public:
  // storage: room for capacity entries (owned by the caller).
  SystemLog(LogEntry* storage, size_t capacity);

  // Takes over the ring kept over a restart: its entries become the oldest
  // ones of this log, and new important entries are copied to it. A ring
  // with a wrong checksum (or power_on: the memory is random) starts empty.
  // Counts the starts (boot()).
  void attachPersistent(PersistentLog* persistent, bool power_on);

  // Number of this start since the RTC memory was last empty (power on).
  uint16_t boot() const;

  void add(LogLevel level, const char* area, const char* text, int64_t uptime_ms, int64_t wall_s);

  // The oldest entry with a running number above after; false if none.
  // Copy entry by entry, e.g. to send the whole log while it grows.
  bool next(uint32_t after, LogEntry& out) const;

  // The newest entries of at least min_level, newest first, at most count.
  std::vector<LogEntry> newest(size_t count, LogLevel min_level = LogLevel::kInfo) const;

  size_t size() const;
  size_t capacity() const { return capacity_; }
  // Entries of at least min_level in the log.
  size_t count(LogLevel min_level) const;

  // Called for every new entry (the serial line on the device).
  void setEcho(std::function<void(const LogEntry&)> echo);

  // Whether an entry is also kept over a restart.
  static bool persists(LogLevel level, const char* area);

 private:
  void addLocked(const LogEntry& entry);
  void persist(const LogEntry& entry);

  mutable std::mutex mutex_;
  LogEntry* storage_;
  size_t capacity_;
  size_t start_ = 0;  // oldest entry
  size_t size_ = 0;
  uint32_t seq_ = 0;
  uint16_t boot_ = 1;
  PersistentLog* persistent_ = nullptr;
  std::function<void(const LogEntry&)> echo_;
};

// Entries of the device log (about a week of normal operation).
inline constexpr size_t kLogCapacity = 630;  // 630 x 104 bytes = 64 KB

// The log of the device, created in PSRAM on first use.
SystemLog& systemLog();

// Clock of the log functions below (default: monotonicMs() and the system
// time); the PC simulation sets its simulated clock.
void setLogClock(std::function<int64_t()> monotonic_ms, std::function<int64_t()> wall_s);

// Adds an entry to systemLog() with the time of the log clock. Only fixed
// texts and numbers, see above.
void logInfo(const char* area, const char* format, ...) __attribute__((format(printf, 2, 3)));
void logWarning(const char* area, const char* format, ...) __attribute__((format(printf, 2, 3)));
void logError(const char* area, const char* format, ...) __attribute__((format(printf, 2, 3)));
void logMessage(LogLevel level, const char* area, const char* format, va_list args);

// Why the last request of the calling task failed (e.g. "HTTP 404", "timed
// out"), noted by the HTTP functions or a data source and read by the
// display controller for the log line of the failed fetch. Kept per task.
// Fixed texts and numbers only.
void noteFetchProblem(const char* format, ...) __attribute__((format(printf, 1, 2)));
// The noted problem (empty if none); clears it.
std::string takeFetchProblem();

// "INFO", "WARN", "ERROR".
const char* logLevelName(LogLevel level);

// Time of an entry: "2026-11-06 07:00:03" (local time) or, before NTP,
// "+0:12:05" (time since its start); always 19 characters.
std::string formatLogTime(const LogEntry& entry);

// One line of the text export, without line end:
// "2026-11-06 07:00:03 WARN  wifi     disconnected, reason 201 (NO_AP_FOUND)"
std::string formatLogLine(const LogEntry& entry);

// "1 d 4:05:09" / "4:05:09": a duration in seconds.
std::string formatUptime(int64_t seconds);

#if defined(ARDUINO)
// Once at the start, after Serial.begin(): takes over the entries kept in
// RTC memory, echoes new entries on the serial line and logs the start with
// the reason of the restart (an error after a crash, a watchdog or a
// voltage drop).
void startSystemLog(const char* firmware_version);

// Reason of the last restart, e.g. "software restart", "crash (panic)".
const char* restartReason();
// True if the last restart was unplanned (crash, watchdog, voltage drop).
bool restartWasUnplanned();
#endif

}  // namespace core
