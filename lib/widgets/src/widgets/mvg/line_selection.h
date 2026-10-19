#pragma once

#include <array>
#include <string>

#include "widgets/mvg/departure.h"

namespace widgets::mvg {

// Comparison form of a line label: without spaces, upper case
// ("RE 80" -> "RE80", "Lufthansa Express Bus" -> "LUFTHANSAEXPRESSBUS").
std::string lineKey(const char* label);

// Direction code of an MVG line ID, e.g. "ddb:92M03: :H:j26" -> kH.
Direction lineDirection(const char* line_id);

// Means of transport for an API value ("SBAHN", "BAHN", ...); kUnknown for
// anything else, including nullptr.
TransportType transportTypeFromApi(const char* api_value);

// API value for a transport type ("SBAHN", ...); nullptr for kUnknown.
const char* transportTypeApiName(TransportType type);

// Line selection of a station: only these lines are shown.
//
// Text form (stored configuration): entries "line:direction:type" separated
// by commas, e.g. "S2:H:S,RE80:B:Z". Line as lineKey(), direction H, R or B
// (both), type as the TransportType code. Empty = no selection (all lines).
class LineSelection {
 public:
  static constexpr int kMaxLines = 16;

  // Reads the text form. Invalid entries and duplicates are dropped, entries
  // beyond kMaxLines are cut off.
  void parse(const std::string& text);

  // Normalised text form.
  std::string toString() const;

  bool empty() const { return count_ == 0; }
  int size() const { return count_; }

  // Is the line (lineKey()) selected for this direction? kUnknown only
  // matches entries for both directions (B).
  bool matches(const std::string& key, Direction direction) const;

  // Is any line of this transport type selected?
  bool hasType(TransportType type) const;

  // Value for the API parameter "transportTypes" from the types of the
  // selected lines (e.g. only S2 -> "SBAHN"); "" if the selection is empty.
  std::string transportTypes() const;

 private:
  struct Choice {
    std::string key;
    char direction;  // 'H', 'R' or 'B'
    TransportType type;
  };
  std::array<Choice, kMaxLines> items_;
  int count_ = 0;
};

}  // namespace widgets::mvg
