#pragma once

// Configuration of the weather widget: section "widgets" -> "weather" of the
// configuration file (see docs/configuration.md).

#include <ArduinoJson.h>

#include <string>

#include "core/config.h"

namespace widgets::weather {

struct Config {
  // Location in decimal degrees (WGS 84, as shown by map services). Both
  // must be set for the widget to work.
  double latitude = 0;
  double longitude = 0;
  bool has_location = false;

  // Shown on the right of the tile title, e.g. "München"; "" for none.
  std::string name;
};

// Reads the section; invalid values are reported under "widgets.weather".
Config parseConfig(JsonObjectConst section, core::ConfigIssues& issues);

}  // namespace widgets::weather
