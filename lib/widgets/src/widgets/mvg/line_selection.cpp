#include "widgets/mvg/line_selection.h"

#include <cctype>
#include <cstring>

namespace widgets::mvg {

namespace {

struct TypeName {
  TransportType type;
  const char* api_name;
};

// Order defines the order in LineSelection::transportTypes().
constexpr TypeName kTypeNames[] = {
    {TransportType::kSBahn, "SBAHN"},
    {TransportType::kUBahn, "UBAHN"},
    {TransportType::kTram, "TRAM"},
    {TransportType::kBus, "BUS"},
    {TransportType::kRegionalBus, "REGIONAL_BUS"},
    {TransportType::kTrain, "BAHN"},
};

// Transport type for a selection code ('S', 'Z', ...); kUnknown if invalid.
TransportType typeFromCode(char code) {
  for (const TypeName& t : kTypeNames) {
    if (static_cast<char>(t.type) == code) return t.type;
  }
  return TransportType::kUnknown;
}

}  // namespace

std::string lineKey(const char* label) {
  std::string key;
  if (label == nullptr) return key;
  for (const char* p = label; *p; p++) {
    if (*p != ' ') key += static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
  }
  return key;
}

Direction lineDirection(const char* line_id) {
  if (line_id == nullptr) return Direction::kUnknown;
  if (std::strstr(line_id, ":H:") != nullptr) return Direction::kH;
  if (std::strstr(line_id, ":R:") != nullptr) return Direction::kR;
  return Direction::kUnknown;
}

TransportType transportTypeFromApi(const char* api_value) {
  if (api_value == nullptr) return TransportType::kUnknown;
  for (const TypeName& t : kTypeNames) {
    if (std::strcmp(t.api_name, api_value) == 0) return t.type;
  }
  return TransportType::kUnknown;
}

const char* transportTypeApiName(TransportType type) {
  for (const TypeName& t : kTypeNames) {
    if (t.type == type) return t.api_name;
  }
  return nullptr;
}

void LineSelection::parse(const std::string& text) {
  count_ = 0;
  size_t start = 0;
  const size_t len = text.length();
  while (start < len && count_ < kMaxLines) {
    size_t end = text.find(',', start);
    if (end == std::string::npos) end = len;
    const std::string entry = text.substr(start, end - start);
    start = end + 1;

    // "line:direction:type"
    const size_t c1 = entry.find(':');
    const size_t c2 = c1 == std::string::npos ? std::string::npos : entry.find(':', c1 + 1);
    if (c1 == std::string::npos || c1 == 0 || c2 != c1 + 2 || entry.length() != c2 + 2) continue;
    const std::string key = lineKey(entry.substr(0, c1).c_str());
    const char direction =
        static_cast<char>(std::toupper(static_cast<unsigned char>(entry[c1 + 1])));
    const char code = static_cast<char>(std::toupper(static_cast<unsigned char>(entry[c2 + 1])));
    if (key.empty() || key.length() > 24) continue;
    if (direction != 'H' && direction != 'R' && direction != 'B') continue;
    const TransportType type = typeFromCode(code);
    if (type == TransportType::kUnknown) continue;
    bool known = false;
    for (int i = 0; i < count_; i++) {
      if (items_[i].key == key) known = true;
    }
    if (known) continue;
    items_[count_] = Choice{key, direction, type};
    count_++;
  }
}

std::string LineSelection::toString() const {
  std::string text;
  for (int i = 0; i < count_; i++) {
    if (i > 0) text += ',';
    text += items_[i].key;
    text += ':';
    text += items_[i].direction;
    text += ':';
    text += static_cast<char>(items_[i].type);
  }
  return text;
}

bool LineSelection::matches(const std::string& key, Direction direction) const {
  for (int i = 0; i < count_; i++) {
    if (items_[i].key == key) {
      return items_[i].direction == 'B' || items_[i].direction == static_cast<char>(direction);
    }
  }
  return false;
}

bool LineSelection::hasType(TransportType type) const {
  for (int i = 0; i < count_; i++) {
    if (items_[i].type == type) return true;
  }
  return false;
}

std::string LineSelection::transportTypes() const {
  std::string types;
  for (const TypeName& t : kTypeNames) {
    if (!hasType(t.type)) continue;
    if (!types.empty()) types += ',';
    types += t.api_name;
  }
  return types;
}

}  // namespace widgets::mvg
