#include "core/config.h"

#include <cctype>
#include <cstring>

#include "core/time.h"

#if defined(ARDUINO)
#include <LittleFS.h>
#endif

namespace core {

namespace {

void addIssue(ConfigIssues& issues, const std::string& path, const std::string& message) {
  issues.push_back(ConfigIssue{path, message});
}

// Host name or IP address: letters, digits, dots and hyphens.
bool isHostName(const std::string& text) {
  if (text.empty()) return false;
  for (char c : text) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') return false;
  }
  return true;
}

// Widget ID: lower-case letters, digits and underscores.
bool isWidgetId(const char* text) {
  if (text == nullptr || *text == '\0' || std::strlen(text) > 16) return false;
  for (const char* p = text; *p; p++) {
    if (!std::islower(static_cast<unsigned char>(*p)) &&
        !std::isdigit(static_cast<unsigned char>(*p)) && *p != '_') {
      return false;
    }
  }
  return true;
}

// Columns of the layout (see ui/layout.h).
constexpr size_t kSlots = 3;

}  // namespace

CoreConfig::CoreConfig() : time_zone(kDefaultTimeZone) {}

std::string configPath(const std::string& path, const char* key) {
  return path.empty() ? std::string(key) : path + "." + key;
}

bool readConfig(const char* json, size_t length, JsonDocument& doc, ConfigIssues& issues) {
  doc.clear();
  if (length > kMaxConfigBytes) {
    addIssue(issues, "",
             "file is larger than " + std::to_string(kMaxConfigBytes) + " bytes, using defaults");
    return false;
  }
  const DeserializationError error = deserializeJson(doc, json, length);
  if (error) {
    doc.clear();
    addIssue(issues, "", std::string("invalid JSON (") + error.c_str() + "), using defaults");
    return false;
  }
  if (!doc.is<JsonObjectConst>()) {
    doc.clear();
    addIssue(issues, "", "must be a JSON object, using defaults");
    return false;
  }
  return true;
}

std::string readString(JsonObjectConst obj, const char* key, const std::string& fallback,
                       size_t max_length, const std::string& path, ConfigIssues& issues) {
  JsonVariantConst value = obj[key];
  if (value.isNull()) return fallback;
  if (!value.is<const char*>()) {
    addIssue(issues, configPath(path, key), "must be a text");
    return fallback;
  }
  const std::string text = value.as<const char*>();
  if (text.length() > max_length) {
    addIssue(issues, configPath(path, key),
             "is longer than " + std::to_string(max_length) + " characters");
    return fallback;
  }
  return text;
}

int readInt(JsonObjectConst obj, const char* key, int fallback, int min, int max,
            const std::string& path, ConfigIssues& issues) {
  JsonVariantConst value = obj[key];
  if (value.isNull()) return fallback;
  if (!value.is<int>()) {
    addIssue(issues, configPath(path, key), "must be a whole number");
    return fallback;
  }
  const int number = value.as<int>();
  if (number < min || number > max) {
    addIssue(issues, configPath(path, key),
             "must be between " + std::to_string(min) + " and " + std::to_string(max));
    return fallback;
  }
  return number;
}

bool readBool(JsonObjectConst obj, const char* key, bool fallback, const std::string& path,
              ConfigIssues& issues) {
  JsonVariantConst value = obj[key];
  if (value.isNull()) return fallback;
  if (!value.is<bool>()) {
    addIssue(issues, configPath(path, key), "must be true or false");
    return fallback;
  }
  return value.as<bool>();
}

int readChoice(JsonObjectConst obj, const char* key, std::initializer_list<const char*> choices,
               int fallback_index, const std::string& path, ConfigIssues& issues) {
  JsonVariantConst value = obj[key];
  if (value.isNull()) return fallback_index;
  if (value.is<const char*>()) {
    int index = 0;
    for (const char* choice : choices) {
      if (std::strcmp(choice, value.as<const char*>()) == 0) return index;
      index++;
    }
  }
  std::string list;
  for (const char* choice : choices) {
    if (!list.empty()) list += ", ";
    list += std::string("\"") + choice + "\"";
  }
  addIssue(issues, configPath(path, key), "must be one of " + list);
  return fallback_index;
}

void reportUnknownKeys(JsonObjectConst obj, std::initializer_list<const char*> known,
                       const std::string& path, ConfigIssues& issues) {
  for (JsonPairConst pair : obj) {
    bool found = false;
    for (const char* key : known) {
      if (std::strcmp(key, pair.key().c_str()) == 0) found = true;
    }
    if (!found) addIssue(issues, configPath(path, pair.key().c_str()), "unknown setting, ignored");
  }
}

JsonObjectConst widgetSection(JsonObjectConst root, const char* id) {
  return root["widgets"][id].as<JsonObjectConst>();
}

CoreConfig parseCoreConfig(JsonObjectConst root, ConfigIssues& issues) {
  CoreConfig c;
  if (root.isNull()) return c;
  reportUnknownKeys(
      root, {"version", "time_zone", "language", "ntp_server", "display", "layout", "widgets"}, "",
      issues);

  // Versions: older files are migrated here when the format changes.
  c.version = readInt(root, "version", kConfigVersion, 1, 1000, "", issues);
  if (c.version > kConfigVersion) {
    addIssue(issues, "version",
             "is newer than this firmware supports (" + std::to_string(kConfigVersion) +
                 "), unknown settings are ignored");
  }

  const std::string tz = readString(root, "time_zone", c.time_zone, 64, "", issues);
  if (isValidTimeZone(tz.c_str())) {
    c.time_zone = tz;
  } else {
    addIssue(issues, "time_zone",
             "is not a valid POSIX time zone, e.g. " + std::string(kDefaultTimeZone));
  }

  c.language = readChoice(root, "language", {"de", "en"}, 0, "", issues) == 0 ? "de" : "en";

  const std::string ntp = readString(root, "ntp_server", c.ntp_server, 64, "", issues);
  if (isHostName(ntp)) {
    c.ntp_server = ntp;
  } else {
    addIssue(issues, "ntp_server", "must be a host name or IP address");
  }

  JsonObjectConst display = root["display"];
  if (!display.isNull()) {
    reportUnknownKeys(display, {"full_refresh_minutes"}, "display", issues);
    c.full_refresh_minutes = readInt(display, "full_refresh_minutes", c.full_refresh_minutes, 1,
                                     1440, "display", issues);
  }

  JsonObjectConst layout = root["layout"];
  if (!layout.isNull()) {
    reportUnknownKeys(layout, {"template", "slots", "grid", "dividers"}, "layout", issues);
    // There is only one layout now; files from older versions may still
    // name one of the removed templates.
    JsonVariantConst name = layout["template"];
    if (!name.isNull() &&
        !(name.is<const char*>() && std::strcmp(name.as<const char*>(), "briefing") == 0)) {
      addIssue(issues, "layout.template", "the other layouts were removed, \"briefing\" is used");
    }

    JsonVariantConst slots = layout["slots"];
    if (!slots.isNull()) {
      std::vector<std::string> ids;
      bool valid = slots.is<JsonArrayConst>();
      for (JsonVariantConst id : slots.as<JsonArrayConst>()) {
        if (!isWidgetId(id.as<const char*>())) valid = false;
        if (valid) ids.push_back(id.as<const char*>());
      }
      if (!valid) {
        addIssue(issues, "layout.slots", "must be a list of widget IDs such as \"mvg\"");
      } else if (ids.size() != kSlots) {
        addIssue(issues, "layout.slots", "must have " + std::to_string(kSlots) + " entries");
      } else {
        c.slots = ids;
      }
    }

    // Variants for trying out on the display (off by default).
    c.equal_columns = readChoice(layout, "grid", {"standard", "equal"}, 0, "layout", issues) == 1;
    c.dividers = readBool(layout, "dividers", c.dividers, "layout", issues);
  }
  return c;
}

#if defined(ARDUINO)
bool loadConfigFile(JsonDocument& doc, ConfigIssues& issues, const char* path) {
  doc.clear();
  if (!LittleFS.exists(path)) {
    addIssue(issues, "", std::string(path) + " not found, using defaults");
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    addIssue(issues, "", std::string(path) + " cannot be opened, using defaults");
    return false;
  }
  const size_t size = file.size();
  if (size > kMaxConfigBytes) {
    file.close();
    return readConfig(nullptr, size, doc, issues);  // reports the size
  }
  std::string text(size, '\0');
  const size_t read = file.read(reinterpret_cast<uint8_t*>(&text[0]), size);
  file.close();
  if (read != size) {
    addIssue(issues, "", std::string(path) + " cannot be read, using defaults");
    return false;
  }
  return readConfig(text.data(), text.size(), doc, issues);
}
#endif

}  // namespace core
