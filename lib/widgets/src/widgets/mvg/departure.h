#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace widgets::mvg {

// Means of transport as reported by the MVG API (field "transportType", with
// "product" as fallback). The character values are the codes used in the
// stored line selection (see LineSelection).
enum class TransportType : char {
  kSBahn = 'S',        // SBAHN
  kUBahn = 'U',        // UBAHN
  kTram = 'T',         // TRAM
  kBus = 'B',          // BUS (city bus)
  kRegionalBus = 'R',  // REGIONAL_BUS
  kTrain = 'Z',        // BAHN (regional and long-distance trains)
  kUnknown = '?',
};

// Direction code of an MVG line ID (":H:" or ":R:"). The code is fixed per
// line and does not mean "towards the city centre"; at mixed stations it
// contradicts itself. It is kept until the new direction concept (freely
// named views) replaces it.
enum class Direction : char {
  kH = 'H',
  kR = 'R',
  kUnknown = '?',
};

inline constexpr int kNoPlatform = -1;

// One departure, fully processed and ready to be shown.
//
// Times are kept as numbers; formatting (local time, minutes until
// departure, delay display rules) belongs to the view.
struct Departure {
  // Line as shown: "S2", "173", "RB56" (trains without the space), "SEV"
  // for rail replacement services, or the group symbol ("RB", "RE") for
  // coupled regional trains with different line numbers.
  std::string line;

  // Destination in UTF-8. Split trains: several destinations joined with
  // "/" (see multi_destination) or a fixed short form ("Flugh./Freising").
  std::string destination;

  // Planned departure, seconds since 1970-01-01 UTC.
  int64_t planned = 0;

  // Delay in minutes, negative when early. For combined trips the smallest
  // delay of the parts with real-time data.
  int delay_minutes = 0;

  TransportType transport = TransportType::kUnknown;
  Direction direction = Direction::kUnknown;

  // Platform number (rail only), kNoPlatform if none.
  int platform = kNoPlatform;

  bool realtime = false;     // real-time data available
  bool cancelled = false;    // trip cancelled at this stop
  bool warning = false;      // show a warning sign (incident, early termination)
  bool replacement = false;  // rail replacement service (SEV)
  bool bus = false;          // drawn with a bus frame (bus or replacement service)

  // Destination lists several destinations "A/B/C"; shorten each evenly.
  bool multi_destination = false;

  // Expected departure (planned + delay), seconds since 1970-01-01 UTC.
  int64_t expected() const { return planned + static_cast<int64_t>(delay_minutes) * 60; }
};

// Departures of one section. A single section without a name is drawn as
// one list.
struct Section {
  std::string name;
  std::vector<Departure> departures;
  // Free text on the right of the section title ("4 min zu Fuß").
  std::string note;
  // The widget shows two stations: title with a location pin instead of
  // the arrow of a direction.
  bool own_station = false;
};

// A disruption message ("INCIDENT" in the MVG API) and the lines it is
// reported for. The API gives only a short text per trip ("Verspätung eines
// vorausfahrenden Zuges") without a heading; the same text usually comes
// with several lines.
struct Incident {
  std::string message;
  std::vector<std::string> lines;  // as shown: "S3", "RB6", "SEV"
};

}  // namespace widgets::mvg
