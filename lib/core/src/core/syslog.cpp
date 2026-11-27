#include "core/syslog.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>

#include "core/log_texts.h"
#include "core/psram.h"
#include "core/time.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#endif

namespace core {

namespace {

// "ITL" + layout version: a ring of another firmware with another layout
// is not taken over.
constexpr uint32_t kMagic = 0x494C5401;

uint32_t checksumOf(const PersistentLog& p) {
  // FNV-1a over everything after the checksum field.
  uint32_t h = 2166136261u;
  const auto* bytes = reinterpret_cast<const uint8_t*>(&p.entries);
  for (size_t i = 0; i < sizeof(p.entries); i++) h = (h ^ bytes[i]) * 16777619u;
  h = (h ^ p.boot) * 16777619u;
  h = (h ^ p.count) * 16777619u;
  h = (h ^ p.next) * 16777619u;
  return h;
}

bool valid(const PersistentLog& p) {
  return p.magic == kMagic && p.count <= PersistentLog::kCapacity &&
         p.next < PersistentLog::kCapacity && p.checksum == checksumOf(p);
}

void copyText(char* to, size_t size, const char* from) {
  std::strncpy(to, from == nullptr ? "" : from, size - 1);
  to[size - 1] = '\0';
  // Do not leave half a UTF-8 character at the end of a cut text.
  size_t n = std::strlen(to);
  if (n == size - 1) {
    size_t i = n;
    while (i > 0 && (static_cast<uint8_t>(to[i - 1]) & 0xC0) == 0x80) i--;
    if (i > 0 && (static_cast<uint8_t>(to[i - 1]) & 0x80) != 0) {
      const uint8_t lead = static_cast<uint8_t>(to[i - 1]);
      const size_t length = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
      if (n - (i - 1) < length) to[i - 1] = '\0';
    }
  }
}

std::mutex clock_mutex;
std::function<int64_t()> clock_monotonic;
std::function<int64_t()> clock_wall;

// One per task, so a failed search on the settings page (own task) never
// gives its reason to a fetch of the main loop. A plain array: thread-local
// storage takes room on every task's stack, and no destructor runs when a
// task ends.
thread_local char fetch_problem[kLogTextSize] = "";

}  // namespace

SystemLog::SystemLog(LogEntry* storage, size_t capacity) : storage_(storage), capacity_(capacity) {}

void SystemLog::attachPersistent(PersistentLog* persistent, bool power_on) {
  std::lock_guard<std::mutex> lock(mutex_);
  persistent_ = persistent;
  if (persistent == nullptr) return;
  if (power_on || !valid(*persistent)) {
    std::memset(static_cast<void*>(persistent), 0, sizeof(*persistent));
    persistent->magic = kMagic;
    persistent->boot = 0;
  }
  persistent->boot++;
  if (persistent->boot == 0) persistent->boot = 1;
  boot_ = persistent->boot;
  // The kept entries become the oldest ones here (the ring is still empty
  // at the start; entries made before are kept after them).
  std::vector<LogEntry> earlier;
  for (size_t i = 0; i < size_; i++) earlier.push_back(storage_[(start_ + i) % capacity_]);
  start_ = 0;
  size_ = 0;
  seq_ = 0;
  const size_t count = persistent->count;
  const size_t first =
      (persistent->next + PersistentLog::kCapacity - count) % PersistentLog::kCapacity;
  for (size_t i = 0; i < count; i++) {
    addLocked(persistent->entries[(first + i) % PersistentLog::kCapacity]);
  }
  for (LogEntry& e : earlier) {
    e.boot = boot_;
    addLocked(e);
    persist(e);
  }
  persistent->checksum = checksumOf(*persistent);
}

uint16_t SystemLog::boot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return boot_;
}

bool SystemLog::persists(LogLevel level, const char* area) {
  return level != LogLevel::kInfo || (area != nullptr && std::strcmp(area, "system") == 0);
}

void SystemLog::addLocked(const LogEntry& entry) {
  LogEntry e = entry;
  e.seq = ++seq_;
  if (size_ < capacity_) {
    storage_[(start_ + size_) % capacity_] = e;
    size_++;
  } else {
    storage_[start_] = e;
    start_ = (start_ + 1) % capacity_;
  }
}

void SystemLog::persist(const LogEntry& entry) {
  if (persistent_ == nullptr || !persists(entry.level, entry.area)) return;
  persistent_->entries[persistent_->next] = entry;
  persistent_->next = static_cast<uint16_t>((persistent_->next + 1) % PersistentLog::kCapacity);
  if (persistent_->count < PersistentLog::kCapacity) persistent_->count++;
}

void SystemLog::add(LogLevel level, const char* area, const char* text, int64_t uptime_ms,
                    int64_t wall_s) {
  LogEntry e;
  e.wall_s = isValidUnixTime(wall_s) ? wall_s : 0;
  e.uptime_s = static_cast<uint32_t>(uptime_ms < 0 ? 0 : uptime_ms / 1000);
  e.level = level;
  copyText(e.area, sizeof(e.area), area);
  copyText(e.text, sizeof(e.text), text);
  std::function<void(const LogEntry&)> echo;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    e.boot = boot_;
    if (capacity_ == 0) return;
    addLocked(e);
    e.seq = seq_;
    if (persistent_ != nullptr && persists(level, area)) {
      persist(e);
      persistent_->checksum = checksumOf(*persistent_);
    }
    echo = echo_;
  }
  if (echo) echo(e);
}

bool SystemLog::next(uint32_t after, LogEntry& out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (size_ == 0) return false;
  const uint32_t oldest = storage_[start_].seq;
  if (after + 1 < oldest) after = oldest - 1;  // older ones were overwritten
  const uint32_t index = after + 1 - oldest;
  if (index >= size_) return false;
  out = storage_[(start_ + index) % capacity_];
  return true;
}

std::vector<LogEntry> SystemLog::newest(size_t count, LogLevel min_level) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<LogEntry> result;
  for (size_t i = size_; i > 0 && result.size() < count; i--) {
    const LogEntry& e = storage_[(start_ + i - 1) % capacity_];
    if (e.level >= min_level) result.push_back(e);
  }
  return result;
}

size_t SystemLog::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return size_;
}

size_t SystemLog::count(LogLevel min_level) const {
  std::lock_guard<std::mutex> lock(mutex_);
  size_t n = 0;
  for (size_t i = 0; i < size_; i++) {
    if (storage_[(start_ + i) % capacity_].level >= min_level) n++;
  }
  return n;
}

void SystemLog::setEcho(std::function<void(const LogEntry&)> echo) {
  std::lock_guard<std::mutex> lock(mutex_);
  echo_ = std::move(echo);
}

SystemLog& systemLog() {
  static SystemLog* log = [] {
    void* memory = psramAllocate(sizeof(LogEntry) * kLogCapacity);
    auto* storage = new (memory) LogEntry[kLogCapacity];
    return new SystemLog(storage, kLogCapacity);
  }();
  return *log;
}

void setLogClock(std::function<int64_t()> monotonic_ms, std::function<int64_t()> wall_s) {
  std::lock_guard<std::mutex> lock(clock_mutex);
  clock_monotonic = std::move(monotonic_ms);
  clock_wall = std::move(wall_s);
}

void logMessage(LogLevel level, const char* area, const char* format, va_list args) {
  char text[kLogTextSize + 32];
  std::vsnprintf(text, sizeof(text), format, args);
  int64_t now_ms;
  int64_t wall_s;
  {
    std::lock_guard<std::mutex> lock(clock_mutex);
    now_ms = clock_monotonic ? clock_monotonic() : monotonicMs();
    wall_s = clock_wall ? clock_wall() : static_cast<int64_t>(std::time(nullptr));
  }
  systemLog().add(level, area, text, now_ms, wall_s);
}

void logInfo(const char* area, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(LogLevel::kInfo, area, format, args);
  va_end(args);
}

void logWarning(const char* area, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(LogLevel::kWarning, area, format, args);
  va_end(args);
}

void logError(const char* area, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logMessage(LogLevel::kError, area, format, args);
  va_end(args);
}

void noteFetchProblem(const char* format, ...) {
  char text[kLogTextSize];
  va_list args;
  va_start(args, format);
  std::vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  std::memcpy(fetch_problem, text, sizeof(fetch_problem));
}

std::string takeFetchProblem() {
  std::string problem = fetch_problem;
  fetch_problem[0] = '\0';
  return problem;
}

const char* logLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarning:
      return "WARN";
    case LogLevel::kError:
      return "ERROR";
  }
  return "";
}

std::string formatUptime(int64_t seconds) {
  if (seconds < 0) seconds = 0;
  const int64_t days = seconds / 86400;
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", static_cast<int>(seconds % 86400 / 3600),
                static_cast<int>(seconds % 3600 / 60), static_cast<int>(seconds % 60));
  if (days == 0) return buffer;
  return std::to_string(days) + " d " + buffer;
}

std::string formatLogTime(const LogEntry& entry) {
  char buffer[40];
  LocalTime t;
  if (entry.wall_s != 0 && toLocalTime(entry.wall_s, t)) {
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", t.year, t.month, t.day,
                  t.hour, t.minute, t.second);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%-19s", ("+" + formatUptime(entry.uptime_s)).c_str());
  }
  return buffer;
}

std::string formatLogLine(const LogEntry& entry) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "%s %-5s %-8s %s", formatLogTime(entry).c_str(),
                logLevelName(entry.level), entry.area, entry.text);
  return buffer;
}

#if defined(ARDUINO)

namespace {

// Kept over a software restart and a crash (not over a power cut). Raw
// bytes, so no constructor clears them at start.
RTC_NOINIT_ATTR uint8_t kept_log[sizeof(PersistentLog)] __attribute__((aligned(8)));

esp_reset_reason_t reset_reason = ESP_RST_UNKNOWN;

}  // namespace

const char* restartReason() {
  switch (reset_reason) {
    case ESP_RST_POWERON:
      return "power on";
    case ESP_RST_EXT:
      return "reset pin";
    case ESP_RST_SW:
      return "software restart";
    case ESP_RST_PANIC:
      return "crash (panic)";
    case ESP_RST_INT_WDT:
      return "watchdog (interrupt)";
    case ESP_RST_TASK_WDT:
      return "watchdog (task)";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "wake from deep sleep";
    case ESP_RST_BROWNOUT:
      return "voltage drop (brownout)";
    case ESP_RST_SDIO:
      return "SDIO";
    case ESP_RST_USB:
      return "USB (flashing or reset)";
    case ESP_RST_JTAG:
      return "JTAG";
    case ESP_RST_EFUSE:
      return "eFuse error";
    case ESP_RST_PWR_GLITCH:
      return "power glitch";
    case ESP_RST_CPU_LOCKUP:
      return "CPU lockup";
    default:
      return "unknown";
  }
}

bool restartWasUnplanned() {
  switch (reset_reason) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
    case ESP_RST_PWR_GLITCH:
    case ESP_RST_CPU_LOCKUP:
      return true;
    default:
      return false;
  }
}

void startSystemLog(const char* firmware_version) {
  reset_reason = esp_reset_reason();
  SystemLog& log = systemLog();
  log.attachPersistent(reinterpret_cast<PersistentLog*>(kept_log), reset_reason == ESP_RST_POWERON);
  log.setEcho([](const LogEntry& e) {
    Serial.printf("%-5s %-8s %s\n", logLevelName(e.level), e.area, e.text);
  });
  if (restartWasUnplanned()) {
    logError("system", "%s", startText(firmware_version, restartReason()).c_str());
  } else {
    logInfo("system", "%s", startText(firmware_version, restartReason()).c_str());
  }
}

#endif

}  // namespace core
