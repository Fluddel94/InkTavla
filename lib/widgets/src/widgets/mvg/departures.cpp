#include "widgets/mvg/departures.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace widgets::mvg {

namespace {

// Raw record, only used inside parseDepartures() to match duplicates and
// combine split trains.
struct RawEntry {
  std::string line;
  std::string destination;
  std::string actual_destination;
  int delay_minutes = 0;
  bool cancelled = false;
  bool realtime = false;
  bool early_termination = false;
  bool superseded = false;
  bool selected = true;  // line selection: show this trip (see steps 1 and 5)
  bool warning = false;
  bool bus = false;
  bool replacement = false;
  bool has_platform = false;  // "platform" only exists for rail
  int platform = kNoPlatform;
  bool multi_destination = false;  // destination is "A/B/C"
  Direction direction = Direction::kUnknown;
  TransportType transport = TransportType::kUnknown;
  long long planned_ms = 0;
};

bool startsWith(const std::string& text, const char* prefix) {
  return text.compare(0, std::strlen(prefix), prefix) == 0;
}

// Known split-train pairings with a fixed short form. The order of the two
// destinations does not matter (the API returns the trips in changing
// order). Only the start of the destination is compared.
struct SplitTrainLabel {
  const char* destination_a;
  const char* destination_b;
  const char* label;
};
constexpr SplitTrainLabel kSplitTrainLabels[] = {
    {"Flughafen", "Freising", "Flugh./Freising"},  // S1 (split in Neufahrn)
};

// Fixed short form for two combined destinations, "" if none matches.
std::string fixedSplitLabel(const std::string& a, const std::string& b) {
  for (const SplitTrainLabel& p : kSplitTrainLabels) {
    if ((startsWith(a, p.destination_a) && startsWith(b, p.destination_b)) ||
        (startsWith(a, p.destination_b) && startsWith(b, p.destination_a))) {
      return p.label;
    }
  }
  return "";
}

// Split trains with different line numbers (regional trains): the parts
// appear in the API as separate lines with the same planned time, platform
// and direction, but without any common attribute (checked 2026-10-03).
// They are therefore only combined within these fixed groups, so unrelated
// trains that happen to leave at the same time stay separate. Lines without
// spaces, with a comma at the start and the end.
struct SplitTrainGroup {
  const char* lines;
  const char* symbol;  // line symbol of the combined row
};
constexpr SplitTrainGroup kSplitTrainGroups[] = {
    {",RB55,RB56,RB57,", "RB"},  // BRB Oberland (Bayrischzell/Lenggries/Tegernsee)
    {",RB6,RB60,", "RB"},        // Werdenfelsbahn (Mittenwald/Pfronten)
    {",RB65,RB66,", "RB"},
    {",RE80,RE89,", "RE"},
};

// Index of the split-train group of a line, -1 if none.
int splitTrainGroup(const std::string& line) {
  const std::string key = "," + line + ",";
  const int count = sizeof(kSplitTrainGroups) / sizeof(kSplitTrainGroups[0]);
  for (int g = 0; g < count; g++) {
    if (std::strstr(kSplitTrainGroups[g].lines, key.c_str()) != nullptr) return g;
  }
  return -1;
}

void removeSpaces(std::string& text) {
  text.erase(std::remove(text.begin(), text.end(), ' '), text.end());
}

}  // namespace

JsonDocument departureFilter() {
  JsonDocument filter;
  JsonObject f = filter[0].to<JsonObject>();
  f["lineId"] = true;
  f["label"] = true;
  f["destination"] = true;
  f["delayInMinutes"] = true;
  f["cancelled"] = true;
  f["realtime"] = true;
  f["plannedDepartureTime"] = true;
  f["transportType"] = true;
  f["product"] = true;
  f["platform"] = true;
  f["sev"] = true;
  f["infos"][0]["type"] = true;
  f["infos"][0]["message"] = true;
  return filter;
}

std::vector<Departure> parseDepartures(const JsonDocument& doc, DirectionFilter filter,
                                       const LineSelection& lines, size_t max_results) {
  // --- Step 1: read records, recognise relevant message types ---
  std::vector<RawEntry> raw;
  raw.reserve(std::min(doc.size(), kMaxRawEntries));

  for (JsonObjectConst dep : doc.as<JsonArrayConst>()) {
    if (raw.size() >= kMaxRawEntries) break;

    // Direction from the line ID: filtered with kH/kR, otherwise only kept
    // for matching duplicates and split trains.
    const Direction direction = lineDirection(dep["lineId"]);
    if (filter == DirectionFilter::kH && direction != Direction::kH) continue;
    if (filter == DirectionFilter::kR && direction != Direction::kR) continue;

    // Field "transportType" (bgw-pt/v3), "product" as fallback (older API).
    // Observed values: SBAHN, UBAHN, TRAM, BUS, REGIONAL_BUS, BAHN.
    const char* transport_type = dep["transportType"];
    if (transport_type == nullptr) transport_type = dep["product"];
    const bool sev = dep["sev"] | false;
    const bool is_train = transport_type != nullptr && std::strcmp(transport_type, "BAHN") == 0;

    // Line selection: only selected lines (with direction). Replacement
    // buses for S-Bahn/tram carry the replaced line as label and match
    // directly. Replacement buses for regional trains carry a train number
    // (type BAHN) or, since 2026-10, sometimes no label at all (type BUS);
    // the API does not say which line is replaced, so they are shown as soon
    // as any regional train is selected. Parts of coupled regional trains
    // (kSplitTrainGroups) are kept for now: if another part is selected,
    // step 4 shows the whole row, otherwise the trip is dropped in step 5.
    const std::string key = lineKey(dep["label"]);
    const bool train_replacement = sev && (is_train || key.empty());
    bool selected = true;
    if (!lines.empty()) {
      selected = lines.matches(key, direction) ||
                 (train_replacement && lines.hasType(TransportType::kTrain));
      if (!selected && splitTrainGroup(key) < 0) continue;
    }

    RawEntry r;
    r.selected = selected;
    r.direction = direction;
    r.transport = transportTypeFromApi(transport_type);
    r.line = dep["label"] | "";
    r.destination = dep["destination"] | "";
    r.actual_destination = r.destination;
    r.delay_minutes = dep["delayInMinutes"] | 0;
    r.cancelled = dep["cancelled"] | false;
    r.realtime = dep["realtime"] | false;
    r.planned_ms = dep["plannedDepartureTime"] | 0LL;
    r.bus = transport_type != nullptr && std::strstr(transport_type, "BUS") != nullptr;

    // Regional trains are labelled "RB 56" / "RE 5"; shown without the space.
    if (is_train) removeSpaces(r.line);

    // Rail replacement service ("sev": true, observed 2026-10-03): the label
    // is the train number ("67116", type BAHN), the replaced line ("S2",
    // type BUS) or the tram number ("25", type BUS). Shown uniformly as
    // "SEV" in a bus frame. The API includes replacement buses in the filter
    // of the replaced means of transport.
    if (sev) {
      r.line = "SEV";
      r.bus = true;
      r.replacement = true;
    }

    // Platform (rail only), used to recognise split trains.
    r.has_platform = dep["platform"].is<int>();
    r.platform = dep["platform"] | kNoPlatform;

    // Only these two message types are relevant for the trip and trigger a
    // warning. Others (e.g. "INFO": fare notes) are ignored on purpose.
    for (JsonObjectConst info : dep["infos"].as<JsonArrayConst>()) {
      const char* type = info["type"];
      if (type == nullptr) continue;
      const bool early_termination = std::strcmp(type, "EARLY_TERMINATION") == 0;
      if (early_termination || std::strcmp(type, "INCIDENT") == 0) r.warning = true;
      if (early_termination) {
        r.early_termination = true;
        const char* message = info["message"];  // "Fährt nur bis <stop>"
        if (message != nullptr) {
          const std::string m = message;
          const size_t idx = m.find("bis ");
          if (idx != std::string::npos) r.actual_destination = m.substr(idx + 4);
        }
      }
    }

    raw.push_back(std::move(r));
  }

  const size_t raw_count = raw.size();

  // --- Step 2: match duplicates (cancelled trip + real trip) ---
  // Line and direction are compared as well, so that with mixed directions
  // no train of the opposite direction in the same minute is matched.
  for (size_t i = 0; i < raw_count; i++) {
    if (!raw[i].cancelled || !raw[i].early_termination || raw[i].superseded) continue;
    for (size_t j = 0; j < raw_count; j++) {
      if (i == j) continue;
      if (raw[j].cancelled || raw[j].superseded) continue;
      if (raw[j].planned_ms != raw[i].planned_ms) continue;
      if (raw[j].line != raw[i].line) continue;
      if (raw[j].direction != raw[i].direction) continue;
      // Matching real trip found.
      raw[j].warning = true;
      raw[i].superseded = true;
      break;
    }
  }

  // --- Step 3: resolve early terminations without a real trip ---
  // Whether the train still stops here is decided by the API's cancellation
  // flag (observed 2026-09-30: if the trip ends before or at this stop, it is
  // "cancelled" here). The position of the new terminus is NOT derived from
  // the text. Afterwards destination holds the destination to show, so that
  // step 4 can combine such trips like all others.
  for (size_t i = 0; i < raw_count; i++) {
    if (raw[i].superseded || !raw[i].early_termination) continue;
    // Cancelled: keep the original destination. Stops here but ends earlier:
    // show the actual destination.
    if (!raw[i].cancelled) raw[i].destination = raw[i].actual_destination;
    raw[i].warning = true;
  }

  // --- Step 4: combine split trains and duplicate trips ---
  // A train that is split on its way (e.g. S1 -> airport / Freising) appears
  // in the API as several trips with the same line, planned time and
  // platform, but different destinations. They become one row: known
  // pairings with a fixed short form (kSplitTrainLabels), otherwise all
  // destinations sorted by line and joined with "/". Regional trains from
  // kSplitTrainGroups are combined across lines; the row then shows the
  // group symbol ("RB").
  // Delay: the smallest one of the trips with real-time data. The parts leave
  // together, so nobody is late. Differences are mostly temporary (real-time
  // data arrives per train part at different times, checked 2026-10-03).
  // Trips with the same destination (e.g. both parts of an S1 not joined
  // during a disruption, observed 2026-09-30) result in a single row.
  // Not combined on purpose:
  // - buses (no platform; extra buses would be combined wrongly)
  // - records without a platform
  // - different cancellation state: if only one part is cancelled, it stays
  //   visible as its own, struck-through row
  // A warning of any part applies to the whole row.
  constexpr int kMaxParts = 4;
  constexpr int kNoDelay = 10000;
  for (size_t i = 0; i < raw_count; i++) {
    if (raw[i].superseded || raw[i].bus || !raw[i].has_platform) continue;

    const int group = splitTrainGroup(raw[i].line);
    std::string part_line[kMaxParts];
    std::string part_destination[kMaxParts];
    part_line[0] = raw[i].line;
    part_destination[0] = raw[i].destination;
    int parts = 1;
    bool mixed_lines = false;
    int min_realtime_delay = raw[i].realtime ? raw[i].delay_minutes : kNoDelay;
    for (size_t j = i + 1; j < raw_count; j++) {
      if (raw[j].superseded || raw[j].bus || !raw[j].has_platform) continue;
      const bool same_line = raw[j].line == raw[i].line;
      if (!same_line && (group < 0 || splitTrainGroup(raw[j].line) != group)) continue;
      if (raw[j].direction != raw[i].direction) continue;
      if (raw[j].planned_ms != raw[i].planned_ms) continue;
      if (raw[j].platform != raw[i].platform) continue;
      if (raw[j].cancelled != raw[i].cancelled) continue;

      // Destination already included -> duplicate trip, otherwise another part.
      bool known = false;
      for (int k = 0; k < parts; k++) {
        if (part_destination[k] == raw[j].destination) known = true;
      }
      if (!known && parts < kMaxParts) {
        part_line[parts] = raw[j].line;
        part_destination[parts] = raw[j].destination;
        parts++;
      }

      if (!same_line) mixed_lines = true;
      if (raw[j].realtime && raw[j].delay_minutes < min_realtime_delay) {
        min_realtime_delay = raw[j].delay_minutes;
      }
      raw[i].warning = raw[i].warning || raw[j].warning;
      raw[i].selected = raw[i].selected || raw[j].selected;
      raw[i].realtime = raw[i].realtime || raw[j].realtime;
      raw[j].superseded = true;
    }

    if (mixed_lines) raw[i].line = kSplitTrainGroups[group].symbol;
    if (min_realtime_delay != kNoDelay) raw[i].delay_minutes = min_realtime_delay;

    if (parts > 1) {
      // Sort destinations by line (stable), so that the order does not change
      // with the order of the API response.
      for (int a = 1; a < parts; a++) {
        for (int b = a; b > 0 && part_line[b] < part_line[b - 1]; b--) {
          std::swap(part_line[b], part_line[b - 1]);
          std::swap(part_destination[b], part_destination[b - 1]);
        }
      }
      const std::string fixed =
          parts == 2 ? fixedSplitLabel(part_destination[0], part_destination[1]) : "";
      if (!fixed.empty()) {
        raw[i].destination = fixed;
      } else {
        raw[i].destination = part_destination[0];
        for (int k = 1; k < parts; k++) {
          raw[i].destination += "/";
          raw[i].destination += part_destination[k];
        }
        raw[i].multi_destination = true;
      }
    }
  }

  // --- Step 5: final list, sorted by expected departure ---
  std::vector<Departure> result;
  for (size_t i = 0; i < raw_count; i++) {
    const RawEntry& r = raw[i];
    if (r.superseded || !r.selected) continue;
    Departure d;
    d.line = r.line;
    d.destination = r.destination;
    d.planned = static_cast<int64_t>(r.planned_ms / 1000);
    d.delay_minutes = r.delay_minutes;
    d.transport = r.transport;
    d.direction = r.direction;
    d.platform = r.has_platform ? r.platform : kNoPlatform;
    d.realtime = r.realtime;
    d.cancelled = r.cancelled;
    d.warning = r.warning;
    d.replacement = r.replacement;
    d.bus = r.bus;
    d.multi_destination = r.multi_destination;
    result.push_back(std::move(d));
  }
  // The API returns trips by planned time; delayed trips can be overtaken.
  std::stable_sort(result.begin(), result.end(), [](const Departure& a, const Departure& b) {
    return a.expected() < b.expected();
  });
  if (result.size() > max_results) result.resize(max_results);
  return result;
}

}  // namespace widgets::mvg
