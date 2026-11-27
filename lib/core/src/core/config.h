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
  int full_refresh_minutes = 360;  // full refresh against ghosting (6 h)
  // Widgets of the columns from left to right; two entries are the layout
  // band + 2, three band + 3.
  std::vector<std::string> slots = {"mvg", "calendar", "weather"};
  // Dotted lines between the columns ("dividers": false turns them off).
  bool dividers = true;

  CoreConfig();
};

// Parses the configuration text into doc. On failure (too large, invalid
// JSON, not an object) doc is left empty, an issue is added and false is
// returned; the caller then works with the defaults.
bool readConfig(const char* json, size_t length, JsonDocument& doc, ConfigIssues& issues);

// Keys under widgets.calendar that would hold the calendar address. It is a
// secret (core/secrets.h) and never belongs in the file.
inline constexpr const char* kCalendarSecretKeys[] = {"url", "ical", "ical_url", "address"};

// Removes the keys above from a parsed configuration, so a secret in an
// imported or hand-written file is never stored or read back (settings
// page, backup, INKTAVLA CONFIG GET). True if something was removed.
bool stripSecrets(JsonObject root);

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

// Whether the settings page opens by itself once after the start: only
// without any settings yet, i.e. no configuration file, or one that reads
// as an empty object ("{}", also after resetting on the settings page). A
// broken file does not count (its problems are reported instead).
bool opensSettingsOnStart(bool file_exists, bool loaded, JsonVariantConst doc);

#if defined(ARDUINO)
// Reads kConfigPath (or path) from LittleFS, which the caller has mounted.
// A missing or broken file leaves doc empty and adds an issue.
bool loadConfigFile(JsonDocument& doc, ConfigIssues& issues, const char* path = kConfigPath);
#endif

}  // namespace core
