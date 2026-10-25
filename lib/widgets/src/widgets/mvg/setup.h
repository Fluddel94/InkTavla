#pragma once

// Help for setting up the MVG widget on the settings page: the station
// search and the lines of a station with example destinations per MVG
// direction code, from the MVG departure display (portal and LineCollector
// there). The page lets the user assign every line and direction to a
// section; the destinations show which way a direction code goes.
//
// The URLs and the processing of the responses are here and tested with
// saved responses; the requests are made by the settings page on the
// device (widgets/registry.cpp).

#include <ArduinoJson.h>

#include <string>
#include <vector>

namespace widgets::mvg {

// https://www.mvg.de/api/bgw-pt/v3/locations?query=<urlencoded>
std::string locationsUrl(const std::string& query);

// https://www.mvg.de/api/bgw-pt/v3/lines/<id>
std::string linesUrl(const std::string& station);

// Departures of all means of transport, 100 from the given offset: example
// destinations for the line list.
std::string sampleDeparturesUrl(const std::string& station, int offset_minutes);

// Offsets of the departure requests for the example destinations: about 12
// hours, so rare lines get destinations as well.
inline constexpr int kSampleOffsets[] = {0, 120, 360, 720};

// Filters for the responses (only the fields used here).
JsonDocument locationsFilter();
JsonDocument linesFilter();
JsonDocument sampleDeparturesFilter();

// Stations of a search response (type "STATION", at most `max`) as JSON:
// [{"id": "de:09162:10", "name": "Pasing", "place": "München",
//   "types": "ZSTB"}], types as the codes of the means of transport.
std::string stationsJson(JsonArrayConst locations, size_t max = 12);

// The lines of a station with up to kDestinations example destinations per
// MVG direction code (H, R).
class LineCatalog {
 public:
  static constexpr size_t kDestinations = 3;
  static constexpr size_t kMaxLines = 120;

  // Response of the lines request. Replacement services and collective
  // labels ("S6/8") are left out.
  void addLines(JsonArrayConst lines);
  // A departures response: adds lines that are missing and destinations.
  void addDepartures(JsonArrayConst departures);

  size_t size() const { return lines_.size(); }

  // [{"key": "S3", "label": "S3", "type": "S", "H": ["Holzkirchen"],
  //   "R": ["Mammendorf"]}], sorted by means of transport (S, U, T, B, R,
  // Z), then by number.
  std::string toJson() const;

 private:
  struct Line {
    std::string key;    // lineKey(): "RE80"
    std::string label;  // as shown: "RE 80"
    char type = '?';
    std::vector<std::string> destinations[2];  // H, R
  };
  Line* add(const char* label, const char* transport_type);

  std::vector<Line> lines_;
};

}  // namespace widgets::mvg
