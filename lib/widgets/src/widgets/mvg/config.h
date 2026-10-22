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

// A freely named section of the departure view ("Stadt", "Raus") with the
// lines (and their MVG direction codes) that belong to it. A section can
// show a station of its own ("two stations"); then its lines may be empty
// (all lines of that station).
struct SectionConfig {
  std::string name;
  LineSelection lines;
  std::string station;  // "" = the station of the widget
  std::string note;     // free text on the right of the title, "" = none
};

// Number of named sections the view supports.
inline constexpr size_t kSectionCount = 2;

struct Config {
  // Station as MVG globalId, e.g. "de:09162:10"; "" = not set up yet.
  std::string station;

  // Name shown in the column title; "" = the name from the MVG API.
  std::string name;

  // Means of transport to request; default all. Replacement buses come with
  // the means of transport they replace.
  std::vector<TransportType> transport_types;

  // Line selection (empty = all lines).
  LineSelection lines;

  DirectionFilter direction = DirectionFilter::kAll;

  // Two named sections instead of one list; empty = one list. With sections,
  // `lines` and `direction` are not used: only lines assigned to a section
  // are shown.
  std::vector<SectionConfig> sections;

  TimeFormat time_format = TimeFormat::kClock;

  Config();

  // Station of section i (its own or the widget's).
  const std::string& sectionStation(size_t i) const;
  // A station is known for everything shown (the list or every section).
  bool hasStation() const;
  // At least one section has a station of its own.
  bool hasOwnStations() const;
};

// Reads the section; invalid values fall back to the defaults and are
// reported in issues under "widgets.mvg".
Config parseConfig(JsonObjectConst section, core::ConfigIssues& issues);

// Value of the API parameter "transportTypes": from the lines of the
// sections or the selected lines if there are any, otherwise from
// transport_types (also if a section has no lines). Without this
// parameter the API leaves out regional buses and trains.
std::string transportTypesParam(const Config& config);

// Checks the format of a globalId: country code, then ":"-separated parts
// of letters, digits and underscores ("de:09162:10").
bool isValidStationId(const std::string& id);

}  // namespace widgets::mvg
