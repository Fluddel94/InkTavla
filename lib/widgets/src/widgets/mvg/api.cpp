#include "widgets/mvg/api.h"

#include <cctype>

#include "core/url.h"
#include "widgets/mvg/departures.h"

#if defined(ARDUINO)
#include "core/http.h"
#endif

namespace widgets::mvg {

namespace {

// The station ID is checked when the configuration is read (letters, digits,
// ":" and "_", see isValidStationId()) and the transport types are fixed
// names, so both go into the URL as they are, like in the MVG departure
// display. Anything else is percent-encoded.
std::string safeOrEncoded(const std::string& text) {
  for (char c : text) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != ':' && c != '_' && c != ',') {
      return core::urlEncode(text);
    }
  }
  return text;
}

}  // namespace

std::string departuresUrl(const Config& config) {
  const bool selection = !config.lines.empty() || !config.sections.empty();
  std::string url = "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=";
  url += safeOrEncoded(config.station);
  url += "&limit=";
  url += std::to_string(selection ? kLimitWithLines : kLimit);
  url += "&transportTypes=";
  url += safeOrEncoded(transportTypesParam(config));
  return url;
}

std::string stationNameUrl(const std::string& station) {
  return "https://www.mvg.de/.rest/zdm/stations/" + safeOrEncoded(station);
}

#if defined(ARDUINO)
bool ApiSource::fetchDepartures(const Config& config, JsonDocument& doc) {
  return core::fetchJsonStream(departuresUrl(config), doc, departureFilter());
}

bool ApiSource::fetchStationName(const std::string& station, std::string& name) {
  JsonDocument filter;
  filter["name"] = true;
  JsonDocument doc;
  if (!core::fetchJsonStream(stationNameUrl(station), doc, filter)) return false;
  const char* value = doc["name"];
  if (value == nullptr || *value == '\0') return false;
  name = value;
  return true;
}
#endif

}  // namespace widgets::mvg
