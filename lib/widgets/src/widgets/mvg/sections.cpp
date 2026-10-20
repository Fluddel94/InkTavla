#include "widgets/mvg/sections.h"

#include <utility>

#include "core/time.h"
#include "widgets/mvg/departures.h"

namespace widgets::mvg {

std::vector<Section> departureSections(const JsonDocument& doc, const Config& config,
                                       size_t max_per_section) {
  std::vector<Section> result;
  if (config.sections.empty()) {
    result.push_back({"", parseDepartures(doc, config.direction, config.lines, max_per_section)});
    return result;
  }
  for (const SectionConfig& s : config.sections) {
    result.push_back(
        {s.name, parseDepartures(doc, DirectionFilter::kAll, s.lines, max_per_section)});
  }
  return result;
}

std::vector<Section> withoutDeparted(const std::vector<Section>& sections, int64_t now_s) {
  if (!core::isValidUnixTime(now_s)) return sections;
  std::vector<Section> result;
  for (const Section& s : sections) {
    Section kept{s.name, {}};
    for (const Departure& d : s.departures) {
      if (d.expected() >= now_s - kKeepDepartedS) kept.departures.push_back(d);
    }
    result.push_back(std::move(kept));
  }
  return result;
}

}  // namespace widgets::mvg
