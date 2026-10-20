#pragma once

// Requests to the MVG API (unofficial, https://www.mvg.de/api/bgw-pt/v3/).
// The URLs are built here; the HTTPS request itself is the core's
// (core/http.h).

#include <ArduinoJson.h>

#include <string>

#include "widgets/mvg/config.h"
#include "widgets/mvg/widget.h"

namespace widgets::mvg {

// "limit" counts all means of transport *before* the line filter: 60
// without a line selection, the maximum of 100 with one (from the MVG
// departure display).
inline constexpr int kLimit = 60;
inline constexpr int kLimitWithLines = 100;

// https://www.mvg.de/api/bgw-pt/v3/departures?globalId=<id>&limit=<n>
//   &transportTypes=<types>  (always set: without it regional buses and
// trains are missing).
std::string departuresUrl(const Config& config);

// https://www.mvg.de/.rest/zdm/stations/<id>: {"name": "..."}.
std::string stationNameUrl(const std::string& station);

#if defined(ARDUINO)
// The MVG API over HTTPS.
class ApiSource : public Source {
 public:
  bool fetchDepartures(const Config& config, JsonDocument& doc) override;
  bool fetchStationName(const std::string& station, std::string& name) override;
};
#endif

}  // namespace widgets::mvg
