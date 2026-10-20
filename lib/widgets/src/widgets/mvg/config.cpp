#include "widgets/mvg/config.h"

#include <cctype>

namespace widgets::mvg {

namespace {

constexpr char kPath[] = "widgets.mvg";

constexpr TransportType kAllTypes[] = {
    TransportType::kSBahn, TransportType::kUBahn,       TransportType::kTram,
    TransportType::kBus,   TransportType::kRegionalBus, TransportType::kTrain,
};

void addIssue(core::ConfigIssues& issues, const char* key, const std::string& message) {
  issues.push_back(core::ConfigIssue{core::configPath(kPath, key), message});
}

bool contains(const std::vector<TransportType>& types, TransportType type) {
  for (TransportType t : types) {
    if (t == type) return true;
  }
  return false;
}

}  // namespace

Config::Config() : transport_types(std::begin(kAllTypes), std::end(kAllTypes)) {}

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
      section, {"station", "name", "transport_types", "lines", "direction", "time_format"}, kPath,
      issues);

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

  JsonVariantConst lines = section["lines"];
  if (!lines.isNull()) {
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
    if (!valid) {
      addIssue(issues, "lines",
               "must be a list of entries \"line:direction:type\" such as \"S2:H:S\"; invalid "
               "entries are ignored");
    }
    if (count > LineSelection::kMaxLines) {
      addIssue(issues, "lines",
               "has more than " + std::to_string(LineSelection::kMaxLines) +
                   " entries; the rest is ignored");
    }
    c.lines.parse(text);
  }

  const int direction = core::readChoice(section, "direction", {"all", "H", "R"}, 0, kPath, issues);
  c.direction = direction == 1   ? DirectionFilter::kH
                : direction == 2 ? DirectionFilter::kR
                                 : DirectionFilter::kAll;

  c.time_format =
      core::readChoice(section, "time_format", {"clock", "minutes"}, 1, kPath, issues) == 0
          ? TimeFormat::kClock
          : TimeFormat::kMinutes;
  return c;
}

std::string transportTypesParam(const Config& config) {
  if (!config.lines.empty()) return config.lines.transportTypes();
  std::string types;
  for (TransportType type : kAllTypes) {  // fixed order
    if (!contains(config.transport_types, type)) continue;
    if (!types.empty()) types += ',';
    types += transportTypeApiName(type);
  }
  return types;
}

}  // namespace widgets::mvg
