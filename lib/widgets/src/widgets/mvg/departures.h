#pragma once

// Reading and processing departures from the MVG API.
//
// The MVG API (https://www.mvg.de/api/bgw-pt/v3/departures) is unofficial
// and undocumented. Field names, direction codes (":H:"/":R:") and message
// types (INCIDENT/EARLY_TERMINATION/INFO) were determined empirically, and
// the processing rules below are based on observations of this API only.
// They are not meant to be reused for other providers.

#include <ArduinoJson.h>

#include <cstddef>
#include <vector>

#include "widgets/mvg/departure.h"
#include "widgets/mvg/line_selection.h"

namespace widgets::mvg {

// At most this many API records are processed (the API returns up to 100).
inline constexpr size_t kMaxRawEntries = 100;

// Direction filter for parseDepartures().
enum class DirectionFilter {
  kH,    // only line IDs with ":H:"
  kR,    // only line IDs with ":R:"
  kAll,  // all directions mixed
};

// ArduinoJson filter for a departures response: keeps only the fields that
// parseDepartures() evaluates. A response contains many more fields per
// trip; without the filter the document needs several times the memory.
JsonDocument departureFilter();

// Reads a departures response (JSON array) from a stream into doc, keeping
// only the fields of departureFilter(). TInput is anything ArduinoJson can
// read from, e.g. an Arduino Stream on the device or a std::istream on the
// PC. The response is never held as a string.
template <typename TInput>
DeserializationError readDepartures(TInput& input, JsonDocument& doc) {
  JsonDocument filter = departureFilter();
  return deserializeJson(doc, input, DeserializationOption::Filter(filter));
}

// Processes downloaded departures (no network access, so it can be applied
// to the same data with several filters):
//  1. reads the records, filtered by direction and line selection (empty =
//     all lines); INCIDENT and EARLY_TERMINATION set a warning, INFO is
//     ignored
//  2. drops a cancelled early-terminating trip if a real trip of the same
//     line, direction and planned time exists (the real one gets the warning)
//  3. resolves remaining early terminations: if the trip still stops here
//     (not cancelled), the new destination is shown
//  4. combines split trains and duplicate trips (same line or same coupled
//     regional train group, direction, planned time, platform and
//     cancellation state); delay = smallest delay of the parts with real-time
//     data
//  5. sorts by expected departure (planned + delay; stable) and returns at
//     most max_results departures
std::vector<Departure> parseDepartures(const JsonDocument& doc, DirectionFilter filter,
                                       const LineSelection& lines, size_t max_results);

}  // namespace widgets::mvg
