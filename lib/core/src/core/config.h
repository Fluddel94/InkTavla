#pragma once

// Configuration file: one JSON object in the flash file system (LittleFS),
// see docs/configuration.md. Credentials (Wi-Fi) are not part of it; they
// stay in the device's protected storage.
//
// Core reads its own settings and offers helpers that widgets use for their
// sections under "widgets". Invalid or missing values never stop the
// device: they fall back to defaults and are reported as issues.

#include <ArduinoJson.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

namespace core {

inline constexpr int kConfigVersion = 1;
inline constexpr size_t kMaxConfigBytes = 16384;
inline constexpr char kConfigPath[] = "/config.json";

// A problem found while reading the configuration, e.g.
// {"display.full_refresh_minutes", "must be between 1 and 1440"}.
struct ConfigIssue {
  std::string path;
  std::string message;
};
using ConfigIssues = std::vector<ConfigIssue>;

struct CoreConfig {
  int version = kConfigVersion;
  std::string time_zone;  // POSIX TZ, default kDefaultTimeZone
  std::string language = "de";
  std::string ntp_server = "pool.ntp.org";
  int full_refresh_minutes = 60;        // full refresh against ghosting
  std::string layout = "one_plus_two";  // "one_plus_two", "three_columns" or "briefing"
  // Default {"mvg", "weather", "calendar"}; for "briefing" {"mvg", "calendar",
  // "weather"}.
  std::vector<std::string> slots = {"mvg", "weather", "calendar"};

  CoreConfig();
};

// Parses the configuration text into doc. On failure (too large, invalid
// JSON, not an object) doc is left empty, an issue is added and false is
// returned; the caller then works with the defaults.
bool readConfig(const char* json, size_t length, JsonDocument& doc, ConfigIssues& issues);

// Reads the core settings; missing values keep their defaults.
CoreConfig parseCoreConfig(JsonObjectConst root, ConfigIssues& issues);

// Section of a widget: root["widgets"][id] (null if missing).
JsonObjectConst widgetSection(JsonObjectConst root, const char* id);

// --- Helpers for reading sections -------------------------------------------
// Missing keys return the default silently. A wrong type or an invalid value
// returns the default and adds an issue with path + "." + key.

std::string readString(JsonObjectConst obj, const char* key, const std::string& fallback,
                       size_t max_length, const std::string& path, ConfigIssues& issues);
int readInt(JsonObjectConst obj, const char* key, int fallback, int min, int max,
            const std::string& path, ConfigIssues& issues);
bool readBool(JsonObjectConst obj, const char* key, bool fallback, const std::string& path,
              ConfigIssues& issues);
// One of the given strings; returns its index.
int readChoice(JsonObjectConst obj, const char* key, std::initializer_list<const char*> choices,
               int fallback_index, const std::string& path, ConfigIssues& issues);

// Reports keys that are not in `known` (typically typing errors).
void reportUnknownKeys(JsonObjectConst obj, std::initializer_list<const char*> known,
                       const std::string& path, ConfigIssues& issues);

// "path.key", or just "key" for the root.
std::string configPath(const std::string& path, const char* key);

#if defined(ARDUINO)
// Reads kConfigPath (or path) from LittleFS, which the caller has mounted.
// A missing or broken file leaves doc empty and adds an issue.
bool loadConfigFile(JsonDocument& doc, ConfigIssues& issues, const char* path = kConfigPath);
#endif

}  // namespace core
