#include "widgets/mvg/sections.h"

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

}  // namespace widgets::mvg
