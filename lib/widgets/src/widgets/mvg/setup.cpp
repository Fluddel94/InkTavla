#include "widgets/mvg/setup.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "core/url.h"
#include "widgets/mvg/line_selection.h"

namespace widgets::mvg {

namespace {

constexpr char kBase[] = "https://www.mvg.de/api/bgw-pt/v3/";
constexpr char kAllTypes[] = "SBAHN,UBAHN,TRAM,BUS,REGIONAL_BUS,BAHN";
constexpr char kTypeOrder[] = "SUTBRZ";

int typeRank(char type) {
  const char* p = std::strchr(kTypeOrder, type);
  return p == nullptr || type == '\0' ? 99 : static_cast<int>(p - kTypeOrder);
}

// Night lines (N..) and express buses (X..) after the others, then by the
// first number in the label.
struct SortKey {
  int rank;
  int group;
  long number;
  std::string key;
  bool operator<(const SortKey& o) const {
    if (rank != o.rank) return rank < o.rank;
    if (group != o.group) return group < o.group;
    if (number != o.number) return number < o.number;
    return key < o.key;
  }
};

SortKey sortKey(const std::string& key, char type) {
  SortKey s{typeRank(type), 0, 1000000000L, key};
  if (key.size() > 1 && std::isdigit(static_cast<unsigned char>(key[1]))) {
    if (key[0] == 'N') s.group = 2;
    if (key[0] == 'X') s.group = 1;
  }
  for (size_t i = 0; i < key.size(); i++) {
    if (std::isdigit(static_cast<unsigned char>(key[i]))) {
      s.number = std::strtol(key.c_str() + i, nullptr, 10);
      break;
    }
  }
  return s;
}

}  // namespace

std::string locationsUrl(const std::string& query) {
  return std::string(kBase) + "locations?query=" + core::urlEncode(query);
}

std::string linesUrl(const std::string& station) {
  return std::string(kBase) + "lines/" + core::urlEncode(station);
}

std::string sampleDeparturesUrl(const std::string& station, int offset_minutes) {
  return std::string(kBase) + "departures?globalId=" + core::urlEncode(station) +
         "&limit=100&offsetInMinutes=" + std::to_string(offset_minutes) +
         "&transportTypes=" + kAllTypes;
}

JsonDocument locationsFilter() {
  JsonDocument f;
  f[0]["type"] = true;
  f[0]["globalId"] = true;
  f[0]["name"] = true;
  f[0]["place"] = true;
  f[0]["transportTypes"] = true;
  return f;
}

JsonDocument linesFilter() {
  JsonDocument f;
  f[0]["label"] = true;
  f[0]["transportType"] = true;
  f[0]["sev"] = true;
  return f;
}

JsonDocument sampleDeparturesFilter() {
  JsonDocument f;
  f[0]["label"] = true;
  f[0]["transportType"] = true;
  f[0]["lineId"] = true;
  f[0]["destination"] = true;
  f[0]["sev"] = true;
  return f;
}

std::string stationsJson(JsonArrayConst locations, size_t max) {
  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  for (JsonObjectConst l : locations) {
    if (list.size() >= max) break;
    const char* type = l["type"];
    const char* id = l["globalId"];
    if (type == nullptr || std::strcmp(type, "STATION") != 0 || id == nullptr || *id == '\0') {
      continue;
    }
    JsonObject s = list.add<JsonObject>();
    s["id"] = id;
    s["name"] = l["name"] | "";
    s["place"] = l["place"] | "";
    std::string types;
    for (JsonVariantConst t : l["transportTypes"].as<JsonArrayConst>()) {
      const TransportType tt = transportTypeFromApi(t.as<const char*>());
      if (tt != TransportType::kUnknown) types += static_cast<char>(tt);
    }
    std::sort(types.begin(), types.end(), [](char a, char b) { return typeRank(a) < typeRank(b); });
    s["types"] = types;
  }
  std::string text;
  serializeJson(out, text);
  return text;
}

LineCatalog::Line* LineCatalog::add(const char* label, const char* transport_type) {
  const std::string key = lineKey(label);
  // Empty and collective labels ("S6/8") are no lines one can choose.
  if (key.empty() || key.find('/') != std::string::npos) return nullptr;
  for (Line& l : lines_) {
    if (l.key == key) return &l;
  }
  const TransportType type = transportTypeFromApi(transport_type);
  if (type == TransportType::kUnknown || lines_.size() >= kMaxLines) return nullptr;
  Line line;
  line.key = key;
  line.label = label;
  line.type = static_cast<char>(type);
  lines_.push_back(std::move(line));
  return &lines_.back();
}

void LineCatalog::addLines(JsonArrayConst lines) {
  for (JsonObjectConst l : lines) {
    if (l["sev"] | false) continue;
    add(l["label"], l["transportType"]);
  }
}

void LineCatalog::addDepartures(JsonArrayConst departures) {
  for (JsonObjectConst d : departures) {
    if (d["sev"] | false) continue;
    Line* line = add(d["label"], d["transportType"]);
    if (line == nullptr) continue;
    const Direction direction = lineDirection(d["lineId"]);
    const char* destination = d["destination"];
    if (direction == Direction::kUnknown || destination == nullptr || *destination == '\0') {
      continue;
    }
    std::vector<std::string>& list = line->destinations[direction == Direction::kH ? 0 : 1];
    if (list.size() < kDestinations &&
        std::find(list.begin(), list.end(), destination) == list.end()) {
      list.push_back(destination);
    }
  }
}

std::string LineCatalog::toJson() const {
  std::vector<const Line*> sorted;
  for (const Line& l : lines_) sorted.push_back(&l);
  std::sort(sorted.begin(), sorted.end(), [](const Line* a, const Line* b) {
    return sortKey(a->key, a->type) < sortKey(b->key, b->type);
  });
  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  for (const Line* l : sorted) {
    JsonObject o = list.add<JsonObject>();
    o["key"] = l->key;
    o["label"] = l->label;
    o["type"] = std::string(1, l->type);
    const char* names[] = {"H", "R"};
    for (int d = 0; d < 2; d++) {
      JsonArray dest = o[names[d]].to<JsonArray>();
      for (const std::string& s : l->destinations[d]) dest.add(s);
    }
  }
  std::string text;
  serializeJson(out, text);
  return text;
}

}  // namespace widgets::mvg
