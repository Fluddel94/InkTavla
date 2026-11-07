#include "core/fault_counts.h"

#include <cstddef>
#include <cstring>

#include "core/time.h"

#if defined(ARDUINO)
#include <esp_attr.h>
#include <esp_system.h>
#endif

namespace core {

namespace {

// "ITF" + layout version: buckets of another layout are not taken over.
constexpr uint32_t kMagic = 0x49544601;
constexpr size_t kNames = PersistentFaultCounts::kNames;
constexpr size_t kHours = PersistentFaultCounts::kHours;

uint32_t checksumOf(const PersistentFaultCounts& p) {
  // FNV-1a over everything after the checksum field.
  uint32_t h = 2166136261u;
  const auto* bytes = reinterpret_cast<const uint8_t*>(&p.last_hour);
  const size_t size = sizeof(p) - offsetof(PersistentFaultCounts, last_hour);
  for (size_t i = 0; i < size; i++) h = (h ^ bytes[i]) * 16777619u;
  return h;
}

void clear(PersistentFaultCounts& p) {
  std::memset(static_cast<void*>(&p), 0, sizeof(p));
  p.magic = kMagic;
  p.last_hour = -1;
  for (auto& h : p.hours) h.hour = -1;
  p.checksum = checksumOf(p);
}

int32_t hourOf(int64_t wall_s) {
  return isValidUnixTime(wall_s) ? static_cast<int32_t>(wall_s / 3600) : -1;
}

void addSaturated(uint16_t& count) {
  if (count < UINT16_MAX) count++;
}

}  // namespace

FaultCounts::FaultCounts() {
  clear(own_);
  p_ = &own_;
}

void FaultCounts::attachPersistent(PersistentFaultCounts* persistent, bool power_on) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (persistent == nullptr) return;
  if (power_on || persistent->magic != kMagic || persistent->checksum != checksumOf(*persistent)) {
    clear(*persistent);
  }
  // Attached at the start, before anything was counted; the counts since
  // the start (RAM) stay as they are.
  p_ = persistent;
}

void FaultCounts::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (Count& c : since_start_) c = Count{};
  clear(*p_);
}

int FaultCounts::slotLocked(const char* name, bool add) {
  for (size_t i = 0; i < kNames; i++) {
    if (std::strncmp(p_->names[i], name, kLogAreaSize) == 0) return static_cast<int>(i);
  }
  if (!add) return -1;
  for (size_t i = 0; i < kNames; i++) {
    if (p_->names[i][0] == '\0') {
      std::strncpy(p_->names[i], name, kLogAreaSize - 1);
      p_->names[i][kLogAreaSize - 1] = '\0';
      return static_cast<int>(i);
    }
  }
  return -1;
}

void FaultCounts::foldPendingLocked(int32_t hour) {
  PersistentFaultCounts::Hour& h = p_->hours[static_cast<size_t>(hour) % kHours];
  if (h.hour != hour) {
    std::memset(h.counts, 0, sizeof(h.counts));
    h.hour = hour;
  }
  for (size_t i = 0; i < kNames; i++) {
    const uint32_t sum = static_cast<uint32_t>(h.counts[i]) + p_->pending[i];
    h.counts[i] = static_cast<uint16_t>(sum > UINT16_MAX ? UINT16_MAX : sum);
    p_->pending[i] = 0;
  }
  if (hour > p_->last_hour) p_->last_hour = hour;
}

void FaultCounts::sealLocked() { p_->checksum = checksumOf(*p_); }

void FaultCounts::add(const char* name, int64_t wall_s) {
  if (name == nullptr || name[0] == '\0') return;
  std::lock_guard<std::mutex> lock(mutex_);
  for (Count& c : since_start_) {
    if (c.name[0] == '\0') {
      std::strncpy(c.name, name, kLogAreaSize - 1);
    } else if (std::strncmp(c.name, name, kLogAreaSize) != 0) {
      continue;
    }
    if (c.count < UINT32_MAX) c.count++;
    break;
  }
  const int slot = slotLocked(name, true);
  if (slot < 0) return;  // more names than slots: only since the start
  const int32_t hour = hourOf(wall_s);
  if (hour < 0) {
    addSaturated(p_->pending[slot]);
  } else {
    foldPendingLocked(hour);
    addSaturated(p_->hours[static_cast<size_t>(hour) % kHours].counts[slot]);
  }
  sealLocked();
}

uint32_t FaultCounts::sinceStart(const char* name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const Count& c : since_start_) {
    if (std::strncmp(c.name, name, kLogAreaSize) == 0) return c.count;
  }
  return 0;
}

uint32_t FaultCounts::last24h(const char* name, int64_t wall_s) const {
  std::lock_guard<std::mutex> lock(mutex_);
  int slot = -1;
  for (size_t i = 0; i < kNames; i++) {
    if (std::strncmp(p_->names[i], name, kLogAreaSize) == 0) slot = static_cast<int>(i);
  }
  if (slot < 0) return 0;
  // Before the clock is set: relative to the newest hour known.
  int32_t now_hour = hourOf(wall_s);
  if (now_hour < p_->last_hour) now_hour = p_->last_hour;
  uint32_t sum = p_->pending[slot];
  for (const auto& h : p_->hours) {
    if (h.hour >= 0 && h.hour <= now_hour && now_hour - h.hour < static_cast<int32_t>(kHours)) {
      sum += h.counts[slot];
    }
  }
  return sum;
}

FaultCounts& faultCounts() {
  static FaultCounts counts;
  return counts;
}

std::string faultCountText(const char* label, uint32_t since_start, uint32_t last_24h) {
  return std::string(label) + " " + std::to_string(since_start) + " · Last 24h " +
         std::to_string(last_24h);
}

#if defined(ARDUINO)

namespace {

// Kept over a software restart and a crash (not over a power cut). Raw
// bytes, so no constructor clears them at start.
RTC_NOINIT_ATTR uint8_t kept_counts[sizeof(PersistentFaultCounts)] __attribute__((aligned(8)));

}  // namespace

void startFaultCounts() {
  faultCounts().attachPersistent(reinterpret_cast<PersistentFaultCounts*>(kept_counts),
                                 esp_reset_reason() == ESP_RST_POWERON);
}

#endif

}  // namespace core
