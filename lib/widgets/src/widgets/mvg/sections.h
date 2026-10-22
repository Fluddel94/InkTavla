#pragma once

// Splits a downloaded departures response into what the view shows: one
// list, or one block per configured section.

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "widgets/mvg/config.h"
#include "widgets/mvg/departure.h"

namespace widgets::mvg {

// Without sections: one unnamed section with the departures filtered by
// config.lines and config.direction. With sections: one section per
// configured section, each filtered by its own lines (the same response is
// processed once per section; a section without lines takes all), with its
// note. At most max_per_section departures each.
std::vector<Section> departureSections(const JsonDocument& doc, const Config& config,
                                       size_t max_per_section);

// The disruption messages of what departureSections() shows, merged (see
// parseIncidents()).
std::vector<Incident> departureIncidents(const JsonDocument& doc, const Config& config);

// Departures that left more than this many seconds ago are dropped before
// drawing, even if no newer data could be fetched.
inline constexpr int64_t kKeepDepartedS = 60;

// The sections without departures whose expected departure is more than
// kKeepDepartedS before now_s (unchanged if now_s is not a valid time).
std::vector<Section> withoutDeparted(const std::vector<Section>& sections, int64_t now_s);

}  // namespace widgets::mvg
