#pragma once

// Configuration of the calendar widget: section "widgets" -> "calendar" of
// the configuration file (see docs/configuration.md). It has no settings
// yet: the iCal address is a secret and kept in NVS (core/secrets.h), never
// in the file.

#include <ArduinoJson.h>

#include "core/config.h"

namespace widgets::calendar {

// Reports unknown keys under "widgets.calendar"; an address put into the
// file by mistake gets a hint how to set it instead.
void checkConfig(JsonObjectConst section, core::ConfigIssues& issues);

}  // namespace widgets::calendar
