#include "widgets/mvg/config.h"

#include <cctype>

namespace widgets::mvg {

namespace {

constexpr char kPath[] = "widgets.mvg";
constexpr size_t kMaxSectionName = 24;
constexpr size_t kMaxSectionNote = 24;

constexpr TransportType kAllTypes[] = {
    TransportType::kSBahn, TransportType::kUBahn,       TransportType::kTram,
    TransportType::kBus,   TransportType::kRegionalBus, TransportType::kTrain,
};

void addIssue(core::ConfigIssues& issues, const char* key, const std::string& message) {
  issues.push_back(core::ConfigIssue{core::configPath(kPath, key), message});
}

// Reads a list of line entries ("S2:H:S", ...) at obj[key]; invalid entries
// are reported under path.key and dropped.
LineSelection readLines(JsonObjectConst obj, const char* key, const std::string& path,
                        core::ConfigIssues& issues) {
  LineSelection result;
  JsonVariantConst lines = obj[key];
  if (lines.isNull()) return result;
  std::string text;
  bool valid = lines.is<JsonArrayConst>();
  int count = 0;
  for (JsonVariantConst value : lines.as<JsonArrayConst>()) {
    const char* entry = value.as<const char*>();
    LineSelection single;
    if (entry != nullptr) single.parse(entry);
    if (single.size() != 1) {  // also catches "S3:B:S,S4:B:S"
      valid = false;
      continue;
    }
    if (!text.empty()) text += ',';
    text += entry;
    count++;
  }
  const std::string where = core::configPath(path, key);
  if (!valid) {
    issues.push_back(core::ConfigIssue{
        where,
        "must be a list of entries \"line:direction:type\" such as \"S2:H:S\"; invalid "
        "entries are ignored"});
  }
  if (count > LineSelection::kMaxLines) {
    issues.push_back(core::ConfigIssue{where, "has more than " +
                                                  std::to_string(LineSelection::kMaxLines) +
                                                  " entries; the rest is ignored"});
  }
  result.parse(text);
  return result;
}

// Reads "sections": exactly kSectionCount objects with a name and lines.
// Anything invalid gives no sections (one list) and an issue.
std::vector<SectionConfig> readSections(JsonObjectConst section, core::ConfigIssues& issues) {
  std::vector<SectionConfig> result;
  JsonVariantConst value = section["sections"];
  if (value.isNull()) return result;
  JsonArrayConst list = value.as<JsonArrayConst>();
  if (!value.is<JsonArrayConst>() || list.size() != kSectionCount) {
    addIssue(issues, "sections",
             "must be a list of " + std::to_string(kSectionCount) +
                 " sections {\"name\": ..., \"lines\": [...]}; showing one list");
    return result;
  }
  for (size_t i = 0; i < list.size(); i++) {
    const std::string path = std::string(kPath) + ".sections[" + std::to_string(i) + "]";
    JsonObjectConst obj = list[i].as<JsonObjectConst>();
    if (obj.isNull()) {
      issues.push_back(core::ConfigIssue{path, "must be an object; showing one list"});
      return {};
    }
    core::reportUnknownKeys(obj, {"name", "lines", "station", "note"}, path, issues);
    SectionConfig s;
    s.name = core::readString(obj, "name", "", kMaxSectionName, path, issues);
    s.lines = readLines(obj, "lines", path, issues);
    s.note = core::readString(obj, "note", "", kMaxSectionNote, path, issues);
    const std::string station = core::readString(obj, "station", "", 32, path, issues);
    if (!station.empty() && !isValidStationId(station)) {
      issues.push_back(core::ConfigIssue{core::configPath(path, "station"),
                                         "must be an MVG station ID such as \"de:09162:10\"; "
                                         "showing one list"});
      return {};
    }
    s.station = station;
    if (s.name.empty()) {
      issues.push_back(
          core::ConfigIssue{core::configPath(path, "name"), "must not be empty; showing one list"});
      return {};
    }
    // A section of the widget's station needs lines; one with a station of
    // its own shows all its lines without.
    if (s.lines.empty() && s.station.empty()) {
      issues.push_back(core::ConfigIssue{core::configPath(path, "lines"),
                                         "must list at least one line (or the section needs a "
                                         "station of its own); showing one list"});
      return {};
    }
    result.push_back(s);
  }
  return result;
}

bool contains(const std::vector<TransportType>& types, TransportType type) {
  for (TransportType t : types) {
    if (t == type) return true;
  }
  return false;
}

}  // namespace

Config::Config() : transport_types(std::begin(kAllTypes), std::end(kAllTypes)) {}

const std::string& Config::sectionStation(size_t i) const {
  if (i < sections.size() && !sections[i].station.empty()) return sections[i].station;
  return station;
}

bool Config::hasStation() const {
  if (sections.empty()) return !station.empty();
  for (size_t i = 0; i < sections.size(); i++) {
    if (sectionStation(i).empty()) return false;
  }
  return true;
}

bool Config::hasOwnStations() const {
  for (const SectionConfig& s : sections) {
    if (!s.station.empty()) return true;
  }
  return false;
}

bool isValidStationId(const std::string& id) {
  if (id.length() < 4 || id.length() > 32) return false;
  if (!std::islower(static_cast<unsigned char>(id[0])) ||
      !std::islower(static_cast<unsigned char>(id[1])) || id[2] != ':') {
    return false;
  }
  bool part_empty = true;
  for (size_t i = 3; i < id.length(); i++) {
    const char c = id[i];
    if (c == ':') {
      if (part_empty) return false;
      part_empty = true;
    } else if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      part_empty = false;
    } else {
      return false;
    }
  }
  return !part_empty;
}

Config parseConfig(JsonObjectConst section, core::ConfigIssues& issues) {
  Config c;
  if (section.isNull()) return c;
  core::reportUnknownKeys(
      section,
      {"station", "name", "transport_types", "lines", "direction", "sections", "time_format"},
      kPath, issues);

  const std::string station = core::readString(section, "station", "", 32, kPath, issues);
  if (station.empty() || isValidStationId(station)) {
    c.station = station;
  } else {
    addIssue(issues, "station", "must be an MVG station ID such as \"de:09162:10\"");
  }

  c.name = core::readString(section, "name", "", 64, kPath, issues);

  JsonVariantConst types = section["transport_types"];
  if (!types.isNull()) {
    std::vector<TransportType> selected;
    bool valid = types.is<JsonArrayConst>();
    for (JsonVariantConst value : types.as<JsonArrayConst>()) {
      const TransportType type = transportTypeFromApi(value.as<const char*>());
      if (type == TransportType::kUnknown) {
        valid = false;
      } else if (!contains(selected, type)) {
        selected.push_back(type);
      }
    }
    if (!valid || selected.empty()) {
      addIssue(issues, "transport_types",
               "must list one or more of \"SBAHN\", \"UBAHN\", \"TRAM\", \"BUS\", "
               "\"REGIONAL_BUS\", \"BAHN\"");
    } else {
      c.transport_types = selected;
    }
  }

  c.lines = readLines(section, "lines", kPath, issues);

  const int direction = core::readChoice(section, "direction", {"all", "H", "R"}, 0, kPath, issues);
  c.direction = direction == 1   ? DirectionFilter::kH
                : direction == 2 ? DirectionFilter::kR
                                 : DirectionFilter::kAll;

  c.sections = readSections(section, issues);
  if (!c.sections.empty()) {
    for (const char* key : {"lines", "direction"}) {
      if (!section[key].isNull()) addIssue(issues, key, "is ignored because sections are set");
    }
  }

  c.time_format =
      core::readChoice(section, "time_format", {"clock", "minutes"}, 0, kPath, issues) == 1
          ? TimeFormat::kMinutes
          : TimeFormat::kClock;
  return c;
}

std::string transportTypesParam(const Config& config) {
  bool all_lines = false;  // a section without lines shows all of its station
  for (const SectionConfig& s : config.sections) all_lines = all_lines || s.lines.empty();
  if (!config.sections.empty() && !all_lines) {
    // Types of all sections, in the fixed order.
    std::string types;
    for (TransportType type : kAllTypes) {
      bool used = false;
      for (const SectionConfig& s : config.sections) used = used || s.lines.hasType(type);
      if (!used) continue;
      if (!types.empty()) types += ',';
      types += transportTypeApiName(type);
    }
    return types;
  }
  if (config.sections.empty() && !config.lines.empty()) return config.lines.transportTypes();
  std::string types;
  for (TransportType type : kAllTypes) {  // fixed order
    if (!contains(config.transport_types, type)) continue;
    if (!types.empty()) types += ',';
    types += transportTypeApiName(type);
  }
  return types;
}

}  // namespace widgets::mvg
