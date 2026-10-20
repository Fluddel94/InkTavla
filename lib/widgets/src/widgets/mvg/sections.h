#pragma once

// Splits a downloaded departures response into what the view shows: one
// list, or one block per configured section.

#include <ArduinoJson.h>

#include <cstddef>
#include <vector>

#include "widgets/mvg/config.h"
#include "widgets/mvg/view.h"

namespace widgets::mvg {

// Without sections: one unnamed section with the departures filtered by
// config.lines and config.direction. With sections: one section per
// configured section, each filtered by its own lines (the same response is
// processed once per section). At most max_per_section departures each.
std::vector<Section> departureSections(const JsonDocument& doc, const Config& config,
                                       size_t max_per_section);

}  // namespace widgets::mvg
