#pragma once

// Configuration of the MVG widget: section "widgets" -> "mvg" of the
// configuration file (see docs/configuration.md).

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "core/config.h"
#include "widgets/mvg/departure.h"
#include "widgets/mvg/departures.h"
#include "widgets/mvg/line_selection.h"
#include "widgets/mvg/time_display.h"

namespace widgets::mvg {

struct Config {
  // Station as MVG globalId, e.g. "de:09162:10"; "" = not set up yet.
  std::string station;

  // Name shown in the header; "" = the name from the MVG API.
  std::string name;

  // Means of transport to request; default all. Replacement buses come with
  // the means of transport they replace.
  std::vector<TransportType> transport_types;

  // Line selection (empty = all lines).
  LineSelection lines;

  DirectionFilter direction = DirectionFilter::kAll;
  TimeFormat time_format = TimeFormat::kMinutes;

  Config();
};

// Reads the section; invalid values fall back to the defaults and are
// reported in issues under "widgets.mvg".
Config parseConfig(JsonObjectConst section, core::ConfigIssues& issues);

// Value of the API parameter "transportTypes": from the selected lines if
// there is a selection, otherwise from transport_types. Without this
// parameter the API leaves out regional buses and trains.
std::string transportTypesParam(const Config& config);

// Checks the format of a globalId: country code, then ":"-separated parts
// of letters, digits and underscores ("de:09162:10").
bool isValidStationId(const std::string& id);

}  // namespace widgets::mvg
