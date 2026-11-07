#pragma once

// Fault counters for the system page (KEY2), the tab "System" of the
// settings page, the downloaded log and "INKTAVLA LOG": how often the Wi-Fi
// connection was lost ("drops") and how often each data source failed
// ("fails"), since the start and in the last 24 hours. A fail is a series
// of failed attempts until the source works again: it counts once, however
// often it is retried (Tobias 2026-11-07).
//
// "Since the start" is kept in RAM. The last 24 hours are kept in 24 hourly
// buckets (keyed by the wall-clock hour) in RTC memory on the device, so
// they survive a restart or a crash (not a power cut), like the important
// log lines (core/syslog.h). Faults before the clock is set (NTP) wait in a
// separate bucket and go into the hour in which the clock is first known.
//
// Counted by the display controller: a lost connection after it was up,
// and the first failed fetch of a series after the first connection since
// the start (fetches stopped for a button never count).

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include "core/syslog.h"

namespace core {

// Name of the Wi-Fi counter (the sources use their log name, e.g. "mvg").
inline constexpr const char* kWifiFaults = "wifi";

// Buckets kept over a restart; also used in tests.
struct PersistentFaultCounts {
  static constexpr size_t kNames = 6;  // Wi-Fi + up to 5 sources
  static constexpr size_t kHours = 24;
  struct Hour {
    int32_t hour;  // wall-clock hour (Unix seconds / 3600), -1: empty
    uint16_t counts[kNames];
  };
  uint32_t magic;
  uint32_t checksum;
  int32_t last_hour;  // newest known hour, -1: none yet
  char names[kNames][kLogAreaSize];
  uint16_t pending[kNames];  // before the clock was set
  Hour hours[kHours];
};

class FaultCounts {
 public:
  FaultCounts();

  // Takes over the buckets kept over a restart. Buckets with a wrong
  // checksum (or power_on: the memory is random) start empty.
  void attachPersistent(PersistentFaultCounts* persistent, bool power_on);

  // Counts one fault of `name` (kWifiFaults or the log name of a source) at
  // the wall-clock time wall_s (invalid before NTP).
  void add(const char* name, int64_t wall_s);

  // Faults of `name` since the start.
  uint32_t sinceStart(const char* name) const;

  // Faults of `name` in the last 24 hours (the current hour and the 23
  // before it), including those before the clock was set.
  uint32_t last24h(const char* name, int64_t wall_s) const;

  // Clears everything (tests).
  void reset();

 private:
  struct Count {
    char name[kLogAreaSize] = {};
    uint32_t count = 0;
  };
  // Index of `name` in the persistent names; adds it if there is room
  // (-1 if not).
  int slotLocked(const char* name, bool add);
  void foldPendingLocked(int32_t hour);
  void sealLocked();

  mutable std::mutex mutex_;
  Count since_start_[2 * PersistentFaultCounts::kNames];
  PersistentFaultCounts own_{};  // used until (or without) attachPersistent()
  PersistentFaultCounts* p_ = nullptr;
};

// The counters of the device.
FaultCounts& faultCounts();

// "Fails 3 · Last 24h 1" (label "Fails" or "Drops"): a line of the system
// page.
std::string faultCountText(const char* label, uint32_t since_start, uint32_t last_24h);

#if defined(ARDUINO)
// Once at the start (after startSystemLog()): takes over the buckets kept
// in RTC memory.
void startFaultCounts();
#endif

}  // namespace core
